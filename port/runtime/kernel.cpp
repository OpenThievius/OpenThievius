/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 the OpenRAC contributors
 *
 * Minimal EE kernel-call behavior for the boot services Sly's entry uses.
 * Service IDs and register arguments follow OpenRAC's interpreter syscall
 * model; unsupported calls are kept out of the host translation.
 */
#include "openrac/guest.h"

#include <atomic>

namespace {
std::atomic<bool> g_exit_requested{false};
uint32_t g_stack_top = GUEST_RAM_SIZE;
}

extern "C" int openrac_guest_exit_requested(void) {
    return g_exit_requested.load(std::memory_order_relaxed);
}

extern "C" void openrac_guest_clear_exit_requested(void) {
    g_exit_requested.store(false, std::memory_order_relaxed);
}

extern "C" uint64_t openrac_guest_kernel_syscall(uint32_t service, uint64_t a0, uint64_t a1,
                                                  uint64_t a2, uint64_t, uint64_t,
                                                  uint64_t, uint64_t, uint64_t) {
    switch (service) {
    case 0x02: // SetGsCrt(interlace, mode, field); display setup belongs to the host renderer.
        return 0;
    case 0x04: // Exit(status); a host frame loop observes the request and closes the session.
        g_exit_requested.store(true, std::memory_order_relaxed);
        return 0;
    case 0x64: // FlushCache: host code has no EE caches.
        return 0;
    case 0x23: // ExitThread: the port runs a single host thread.
        return 0;
    case 0x3C: { // SetupThread(gp, stack, stack_size, args, root)
        const uint32_t stack = static_cast<uint32_t>(a1) == UINT32_MAX
                                   ? GUEST_RAM_SIZE - static_cast<uint32_t>(a2)
                                   : static_cast<uint32_t>(a1);
        g_stack_top = stack + static_cast<uint32_t>(a2);
        return g_stack_top;
    }
    case 0x3D: // SetupHeap(start, size)
        return static_cast<uint32_t>(a1) == UINT32_MAX
                   ? g_stack_top - 0x4000u
                   : static_cast<uint32_t>(a0) + static_cast<uint32_t>(a1);
    default:
        openrac_guest_missing("unsupported EE kernel call reached");
        return 0;
    }
}
