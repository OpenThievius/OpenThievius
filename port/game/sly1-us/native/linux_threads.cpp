// SPDX-License-Identifier: AGPL-3.0-or-later
// Native Linux thread services for Sly's EE thread syscall wrappers.
#if !defined(__linux__) || !defined(__i386__)
#error "The native thread service currently targets Linux i386 only."
#endif

using u32 = unsigned int;
using ThreadEntry = void (*)(void*);

struct EeThreadParameters {
    int attributes;
    ThreadEntry entry;
    void* guest_stack;
    int guest_stack_size;
    void* gp;
    int priority;
    int option;
};

namespace {
constexpr unsigned int kThreadCapacity = 16;
constexpr unsigned int kHostStackSize = 64 * 1024;

struct NativeThread {
    int identifier;
    ThreadEntry entry;
    int active;
    alignas(16) unsigned char stack[kHostStackSize];
};

NativeThread g_threads[kThreadCapacity]{};
volatile int g_thread_lock;

void lock_threads() {
    while (__sync_lock_test_and_set(&g_thread_lock, 1) != 0) {
        while (g_thread_lock != 0) __asm__ __volatile__("pause");
    }
}

void unlock_threads() { __sync_lock_release(&g_thread_lock); }

NativeThread* find_thread(int identifier) {
    for (unsigned int i = 0; i < kThreadCapacity; ++i)
        if (g_threads[i].active && g_threads[i].identifier == identifier)
            return &g_threads[i];
    return nullptr;
}
} // namespace

extern "C" int sly1_clone_thread(ThreadEntry entry, void* stack_top, void* argument);

extern "C" int func_001F6A10(const EeThreadParameters* parameters) {
    if (parameters == nullptr || parameters->entry == nullptr) return -1;
    lock_threads();
    for (unsigned int i = 0; i < kThreadCapacity; ++i) {
        NativeThread* thread = &g_threads[i];
        if (thread->active) continue;
        thread->entry = parameters->entry;
        thread->identifier = static_cast<int>(i + 1);
        thread->active = 1;
        const int identifier = thread->identifier;
        unlock_threads();
        return identifier;
    }
    unlock_threads();
    return -1;
}

extern "C" int func_001F6A20(int identifier, void* argument) {
    lock_threads();
    NativeThread* thread = find_thread(identifier);
    if (thread == nullptr) {
        unlock_threads();
        return -1;
    }
    const int result = sly1_clone_thread(
        thread->entry, thread->stack + sizeof(thread->stack), argument);
    if (result < 0) thread->active = 0;
    else thread->identifier = result;
    unlock_threads();
    return result < 0 ? -1 : 0;
}

extern "C" int func_001F6A50(int identifier, int priority) {
    (void)identifier;
    (void)priority;
    return 0;
}

extern "C" int func_001F6A90() {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "0"(224) : "memory", "cc");
    return result;
}
