// SPDX-License-Identifier: AGPL-3.0-or-later
// Native Linux thread services for the direct Sly host32 build. The matching
// EE syscall veneers are adapted only in generated host source and call these
// functions; no matched game function is replaced here.
#if !defined(__linux__) || !defined(__i386__)
#error "The native thread service currently targets Linux i386 only."
#endif

#include "native_function_lookup.hpp"

using u32 = unsigned int;
using ThreadEntry = void (*)(void*);

extern "C" int sly1_clone_thread(ThreadEntry entry, void* stack_top, void* argument);
extern "C" void sly1_native_trace_thread_service(u32 service, u32 result, u32 a0, u32 a1);

namespace {
constexpr unsigned int kThreadCapacity = 32;
constexpr unsigned int kHostStackSize = 0x20000;

struct NativeThread {
    int identifier;
    ThreadEntry entry;
    void* argument;
    volatile int active;
    alignas(16) unsigned char stack[kHostStackSize];
};

NativeThread g_threads[kThreadCapacity]{};
volatile int g_thread_lock;
int g_next_identifier = 1;

void lock_threads() {
    while (__sync_lock_test_and_set(&g_thread_lock, 1) != 0) {
        while (g_thread_lock != 0) __asm__ __volatile__("pause");
    }
}

void unlock_threads() { __sync_lock_release(&g_thread_lock); }

NativeThread* find_thread(int identifier) {
    for (unsigned int i = 0; i < kThreadCapacity; ++i) {
        if (g_threads[i].active && g_threads[i].identifier == identifier)
            return &g_threads[i];
    }
    return nullptr;
}

void thread_entry(void* opaque) {
    NativeThread* thread = static_cast<NativeThread*>(opaque);
    thread->entry(thread->argument);
    __atomic_store_n(&thread->active, 0, __ATOMIC_RELEASE);
}
} // namespace

extern "C" int sly1_native_make_thread(const void* parameters) {
    if (parameters == nullptr) {
        sly1_native_trace_thread_service(0x20u, static_cast<u32>(-1), 0, 0);
        return -1;
    }
    const auto* words = static_cast<const volatile u32*>(parameters);
    const u32 raw_entry = words[1];
    const u32 entry = sly1::native_function_address(raw_entry);
    if (entry == 0) {
        sly1_native_trace_thread_service(0x20u, static_cast<u32>(-1),
                                         reinterpret_cast<u32>(parameters), raw_entry);
        return -1;
    }

    lock_threads();
    for (unsigned int i = 0; i < kThreadCapacity; ++i) {
        NativeThread* thread = &g_threads[i];
        if (__atomic_load_n(&thread->active, __ATOMIC_ACQUIRE)) continue;
        thread->identifier = g_next_identifier++;
        if (g_next_identifier <= 0) g_next_identifier = 1;
        thread->entry = reinterpret_cast<ThreadEntry>(entry);
        thread->argument = nullptr;
        __atomic_store_n(&thread->active, 1, __ATOMIC_RELEASE);
        const int identifier = thread->identifier;
        unlock_threads();
        sly1_native_trace_thread_service(0x20u, static_cast<u32>(identifier),
                                         reinterpret_cast<u32>(parameters), raw_entry);
        return identifier;
    }
    unlock_threads();
    return -1;
}

extern "C" int sly1_native_start_thread(int identifier, void* argument) {
    lock_threads();
    NativeThread* thread = find_thread(identifier);
    if (thread == nullptr) {
        unlock_threads();
        return -1;
    }
    thread->argument = argument;
    const int result = sly1_clone_thread(
        thread_entry, thread->stack + sizeof(thread->stack), thread);
    if (result < 0) __atomic_store_n(&thread->active, 0, __ATOMIC_RELEASE);
    unlock_threads();
    sly1_native_trace_thread_service(0x22u,
        static_cast<u32>(result < 0 ? -1 : 0), static_cast<u32>(identifier),
        reinterpret_cast<u32>(argument));
    return result < 0 ? -1 : 0;
}

extern "C" int sly1_native_set_thread_priority(int identifier, int priority) {
    (void)identifier;
    (void)priority;
    return 0;
}

extern "C" int sly1_native_get_thread_id(void) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "0"(224) : "memory", "cc");
    return result;
}
