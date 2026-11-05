// SPDX-License-Identifier: AGPL-3.0-or-later
// Host replacement for the EE interrupt-disable/restore pair used around
// shared Sony library state. A recursive spin lock preserves the critical
// section across native game and worker threads without executing EE code.
namespace {
volatile int g_lock_owner = 0;
volatile unsigned int g_lock_depth = 0;
volatile unsigned int g_lock_spins = 0;

int current_thread_id() {
    int result;
    __asm__ volatile("int $0x80" : "=a"(result) : "0"(224) : "memory", "cc");
    return result;
}
} // namespace

extern "C" int func_001F9D60(...) {
    const int tid = current_thread_id();
    if (__atomic_load_n(&g_lock_owner, __ATOMIC_RELAXED) == tid) {
        ++g_lock_depth;
        return 0;
    }
    for (;;) {
        int expected = 0;
        if (__atomic_compare_exchange_n(&g_lock_owner, &expected, tid, false,
                                        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
            break;
        __atomic_add_fetch(&g_lock_spins, 1u, __ATOMIC_RELAXED);
        __asm__ volatile("pause" ::: "memory");
    }
    g_lock_depth = 1;
    return 0;
}

extern "C" int func_001F9DA8(...) {
    const int tid = current_thread_id();
    if (__atomic_load_n(&g_lock_owner, __ATOMIC_RELAXED) != tid || g_lock_depth == 0)
        return 0;
    if (--g_lock_depth == 0)
        __atomic_store_n(&g_lock_owner, 0, __ATOMIC_RELEASE);
    return 0;
}

extern "C" int sly1_native_critical_owner() {
    return __atomic_load_n(&g_lock_owner, __ATOMIC_RELAXED);
}

extern "C" unsigned int sly1_native_critical_depth() {
    return __atomic_load_n(&g_lock_depth, __ATOMIC_RELAXED);
}

extern "C" unsigned int sly1_native_critical_spins() {
    return __atomic_load_n(&g_lock_spins, __ATOMIC_RELAXED);
}
