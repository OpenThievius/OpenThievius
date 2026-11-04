// SPDX-License-Identifier: AGPL-3.0-or-later
// Native replacement for Sly's PS2 boot entry, kept outside matching source.
#include "openrac/guest.h"
#include "openrac/native_dma.h"

#include <cstring>

extern "C" int openrac_lib_sly1_interrupt_enable(void);

extern "C" int openrac_lib_sly1_native_bootstrap(void) {
    // Retail clears its BSS before setting up the main thread and heap.
    std::memset(G(0x0027B300u), 0, 0x0064C700u - 0x0027B300u);

    // Native equivalents of SetupThread(gp, stack, size, args, root) and
    // SetupHeap(start, -1). The host compiler owns the actual call stack.
    openrac_guest_kernel_syscall(0x3Cu, 0x002832F0u, 0x0062C700u, 0x00020000u,
                                 0x0027B380u, 0x001000C0u, 0, 0, 0);
    // SetupThread records the range, but the native main thread does not pass
    // through the worker-thread trampoline that normally positions its guest
    // stack pointer at the top of that range.
    openrac_guest_sp = 0x0064C700u;
    openrac_guest_kernel_syscall(0x3Du, 0x0064C700u, 0xFFFFFFFFu, 0, 0, 0, 0, 0, 0);

    // The native DMA and interrupt services replace the corresponding EE
    // hardware setup performed by the retail kernel initialization routine.
    openrac_lib_sly1_dma_reset(0);
    openrac_lib_sly1_interrupt_enable();

    const gaddr initial_state = GREF(gaddr, 0x0027B380u);
    const uint64_t ints[8] = {initial_state, 0x0027B384u};
    const float floats[8] = {};
    float result = 0.0f;
    openrac_guest_call_regs(0x00185758u, ints, floats, &result, 0);
    return 0;
}
