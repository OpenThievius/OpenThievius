// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only implementation of the worker-start callback at retail address 0x0018E5E8.
#include "openrac/guest.h"

#include <cstring>

extern "C" void sly1_native_start_worker(void) {
    const gaddr saved_sp = openrac_guest_sp;
    const gaddr params = openrac_guest_frame(0x48u);
    std::memset(G(params), 0, 0x48u);

    GREF(uint32_t, params + 4u) = 1;
    const uint32_t semaphore = static_cast<uint32_t>(openrac_guest_kernel_syscall(
        0x40u, params, 0, 0, 0, 0, 0, 0, 0));
    GREF(uint32_t, 0x00269B30u) = semaphore;
    GREF(uint32_t, 0x00269A0Cu) = 0;

    const gaddr thread_params = params + 0x18u;
    GREF(uint32_t, thread_params + 0x04u) = 0x0018E558u;
    GREF(uint32_t, thread_params + 0x08u) = 0x002C2B00u;
    GREF(uint32_t, thread_params + 0x0Cu) = 0x00001000u;
    GREF(uint32_t, thread_params + 0x10u) = 0x002832F0u;
    GREF(uint32_t, thread_params + 0x14u) = 2u;
    const uint32_t thread = static_cast<uint32_t>(openrac_guest_kernel_syscall(
        0x20u, thread_params, 0, 0, 0, 0, 0, 0, 0));
    GREF(uint32_t, 0x00275964u) = thread;
    (void)openrac_guest_kernel_syscall(0x22u, thread, 0, 0, 0, 0, 0, 0, 0);
    openrac_guest_sp = saved_sp;
}
