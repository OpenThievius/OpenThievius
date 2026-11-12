// SPDX-License-Identifier: AGPL-3.0-or-later
// Native replacements for PS2 data-cache maintenance APIs. Host memory is
// coherent; the barriers preserve ordering around transfers and worker threads.
extern "C" void func_001FA7D8(const void* start, int size)
{
    if (start != nullptr && size > 0)
        __atomic_thread_fence(__ATOMIC_RELEASE);
}

extern "C" void func_00204E20(const void* start, const void* end)
{
    const __UINTPTR_TYPE__ begin_address =
        reinterpret_cast<__UINTPTR_TYPE__>(start);
    const __UINTPTR_TYPE__ end_address =
        reinterpret_cast<__UINTPTR_TYPE__>(end);
    if (begin_address != 0 && end_address > begin_address)
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
}
