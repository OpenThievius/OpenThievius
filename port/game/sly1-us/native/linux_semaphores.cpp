// SPDX-License-Identifier: AGPL-3.0-or-later
// Native Linux semaphore services for the direct Sly host32 build. The
// matching EE syscall veneers are adapted only in generated host source.
#if !defined(__linux__) || !defined(__i386__)
#error "The native semaphore service currently targets Linux i386 only."
#endif

namespace {
struct NativeSemaphoreParameters {
    int count;
    int max_count;
    int init_count;
    int wait_threads;
    unsigned int attributes;
    unsigned int option;
};

struct NativeSemaphore {
    int active;
    int identifier;
    volatile int count;
    int waiters;
    int max_count;
};

constexpr unsigned int kSemaphoreCapacity = 1024;
constexpr int kFutexWait = 0;
constexpr int kFutexWake = 1;
NativeSemaphore g_semaphores[kSemaphoreCapacity]{};
volatile int g_registry_lock;
int g_next_identifier = 1;

void lock_registry() {
    while (__sync_lock_test_and_set(&g_registry_lock, 1) != 0) {
        while (g_registry_lock != 0) __asm__ __volatile__("pause");
    }
}

void unlock_registry() { __sync_lock_release(&g_registry_lock); }

int futex(volatile int* address, int operation, int value) {
    int result;
    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "0"(240), "b"(address), "c"(operation), "d"(value), "S"(0)
        : "memory", "cc");
    return result;
}

NativeSemaphore* lookup_locked(int identifier) {
    if (identifier <= 0) return nullptr;
    for (unsigned int i = 0; i < kSemaphoreCapacity; ++i) {
        NativeSemaphore* semaphore = &g_semaphores[i];
        if (semaphore->active && semaphore->identifier == identifier)
            return semaphore;
    }
    return nullptr;
}

int signal_semaphore(int identifier) {
    lock_registry();
    NativeSemaphore* semaphore = lookup_locked(identifier);
    if (semaphore == nullptr) {
        unlock_registry();
        return -1;
    }
    const int count = __atomic_load_n(&semaphore->count, __ATOMIC_RELAXED);
    if (count < 0 || count >= semaphore->max_count) {
        unlock_registry();
        return -1;
    }
    __atomic_store_n(&semaphore->count, count + 1, __ATOMIC_RELEASE);
    if (count == 0) futex(&semaphore->count, kFutexWake, 1);
    unlock_registry();
    return 0;
}

int poll_semaphore(int identifier) {
    lock_registry();
    NativeSemaphore* semaphore = lookup_locked(identifier);
    if (semaphore == nullptr) {
        unlock_registry();
        return -1;
    }
    const int count = __atomic_load_n(&semaphore->count, __ATOMIC_RELAXED);
    if (count <= 0) {
        unlock_registry();
        return -1;
    }
    __atomic_store_n(&semaphore->count, count - 1, __ATOMIC_RELEASE);
    unlock_registry();
    return 0;
}

int wait_semaphore(int identifier) {
    for (;;) {
        lock_registry();
        NativeSemaphore* semaphore = lookup_locked(identifier);
        if (semaphore == nullptr) {
            unlock_registry();
            return -1;
        }
        const int count = __atomic_load_n(&semaphore->count, __ATOMIC_RELAXED);
        if (count > 0) {
            __atomic_store_n(&semaphore->count, count - 1, __ATOMIC_RELEASE);
            unlock_registry();
            return 0;
        }
        ++semaphore->waiters;
        unlock_registry();
        futex(&semaphore->count, kFutexWait, 0);
        lock_registry();
        --semaphore->waiters;
        const bool deleted = !semaphore->active || semaphore->identifier != identifier;
        unlock_registry();
        if (deleted) return -1;
    }
}
} // namespace

extern "C" int sly1_native_create_semaphore(const void* parameters) {
    if (parameters == nullptr) return -1;
    const auto* params = static_cast<const volatile NativeSemaphoreParameters*>(parameters);
    if (params->max_count <= 0 || params->init_count < 0 ||
        params->init_count > params->max_count) return -1;

    lock_registry();
    for (unsigned int i = 0; i < kSemaphoreCapacity; ++i) {
        NativeSemaphore* semaphore = &g_semaphores[i];
        if (semaphore->active || semaphore->waiters != 0) continue;
        semaphore->max_count = params->max_count;
        semaphore->identifier = g_next_identifier++;
        if (g_next_identifier <= 0) g_next_identifier = 1;
        __atomic_store_n(&semaphore->count, params->init_count, __ATOMIC_RELAXED);
        semaphore->active = 1;
        const int identifier = semaphore->identifier;
        unlock_registry();
        return identifier;
    }
    unlock_registry();
    return -1;
}

extern "C" int sly1_native_delete_semaphore(int identifier) {
    lock_registry();
    NativeSemaphore* semaphore = lookup_locked(identifier);
    if (semaphore == nullptr) {
        unlock_registry();
        return -1;
    }
    semaphore->active = 0;
    __atomic_store_n(&semaphore->count, -1, __ATOMIC_RELEASE);
    futex(&semaphore->count, kFutexWake, 0x7fffffff);
    unlock_registry();
    return 0;
}

extern "C" int sly1_native_signal_semaphore(int identifier) {
    return signal_semaphore(identifier);
}

extern "C" int sly1_native_wait_semaphore(int identifier) {
    return wait_semaphore(identifier);
}

extern "C" int sly1_native_poll_semaphore(int identifier) {
    return poll_semaphore(identifier);
}
