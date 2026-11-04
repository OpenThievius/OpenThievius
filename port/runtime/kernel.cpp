/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 the OpenRAC contributors
 *
 * Minimal EE kernel-call behavior for the boot services Sly's entry uses.
 * Service IDs and register arguments follow OpenRAC's interpreter syscall
 * model; unsupported calls are kept out of the host translation.
 */
#include "openrac/guest.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <vector>

namespace {
std::atomic<bool> g_exit_requested{false};
std::atomic<bool> g_cpu_interrupts_enabled{true};
uint32_t g_stack_top = GUEST_RAM_SIZE;
std::atomic<uint32_t> g_next_id{2};
std::mutex g_kernel_mutex;
std::map<uint32_t, int32_t> g_semaphores;
std::map<uint32_t, uint32_t> g_interrupt_handlers;
std::set<uint32_t> g_enabled_interrupts;
uint64_t g_gs_imr = 0x7F00u;
}

/* The EE replacements for `di` and `ei` return the previous Status.IE bit,
 * which callers use to restore nested critical sections correctly. */
extern "C" int openrac_lib_sly1_interrupt_disable(void) {
    return g_cpu_interrupts_enabled.exchange(false, std::memory_order_relaxed) ? 1 : 0;
}

extern "C" int openrac_lib_sly1_interrupt_enable(void) {
    return g_cpu_interrupts_enabled.exchange(true, std::memory_order_relaxed) ? 1 : 0;
}

extern "C" int openrac_guest_exit_requested(void) {
    return g_exit_requested.load(std::memory_order_relaxed);
}

extern "C" void openrac_guest_clear_exit_requested(void) {
    g_exit_requested.store(false, std::memory_order_relaxed);
}

extern "C" uint32_t openrac_guest_cp0_count(void) {
    const auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
    const double cycles = std::chrono::duration<double>(elapsed).count() * 294'912'000.0;
    return static_cast<uint32_t>(static_cast<uint64_t>(cycles));
}

extern "C" uint32_t openrac_guest_cp0_status(void) {
    return g_cpu_interrupts_enabled.load(std::memory_order_relaxed) ? 0x10000u : 0u;
}

extern "C" void openrac_lib_sly1_cache_sync(uint32_t start, uint32_t end) {
    (void)start;
    (void)end;
    // Guest and host share coherent memory; preserve the EE sync ordering only.
    std::atomic_thread_fence(std::memory_order_seq_cst);
}

extern "C" int openrac_lib_sly1_console_printf(uint32_t guest_format) {
    (void)guest_format;
    // The EE serial diagnostic stream is not part of the game's native interface.
    return 0;
}

extern "C" int openrac_guest_raise_interrupt(uint32_t cause) {
    uint32_t handler = 0;
    {
        std::lock_guard lock(g_kernel_mutex);
        if (!g_cpu_interrupts_enabled.load(std::memory_order_relaxed) ||
            !g_enabled_interrupts.contains(cause)) return 0;
        const auto it = g_interrupt_handlers.find(cause);
        if (it == g_interrupt_handlers.end()) return 0;
        handler = it->second;
    }
    const uint64_t ints[8] = {};
    const float floats[8] = {};
    float result = 0.0f;
    openrac_guest_call_regs(handler, ints, floats, &result, 0);
    return 1;
}

extern "C" uint64_t openrac_guest_kernel_syscall(uint32_t service, uint64_t a0, uint64_t a1,
                                                  uint64_t a2, uint64_t, uint64_t,
                                                  uint64_t, uint64_t, uint64_t) {
    switch (service) {
    case 0x10: { // InstallIntcHandler; the host renderer raises registered causes when ready.
        const uint32_t cause = static_cast<uint32_t>(a0);
        const uint32_t handler = static_cast<uint32_t>(a1);
        if (handler == 0) return 0;
        std::lock_guard lock(g_kernel_mutex);
        g_interrupt_handlers[cause] = handler;
        return handler;
    }
    case 0x14: { // EnableIntc
        std::lock_guard lock(g_kernel_mutex);
        g_enabled_interrupts.insert(static_cast<uint32_t>(a0));
        return 0;
    }
    case 0x15: { // DisableIntc
        std::lock_guard lock(g_kernel_mutex);
        g_enabled_interrupts.erase(static_cast<uint32_t>(a0));
        return 0;
    }
    case 0x02: // SetGsCrt(interlace, mode, field); display setup belongs to the host renderer.
        return 0;
    case 0x04: // Exit(status); a host frame loop observes the request and closes the session.
        g_exit_requested.store(true, std::memory_order_relaxed);
        return 0;
    case 0x5A: { // Copy(dest, src, size); KSEG0 aliases normalize to the same guest RAM.
        const uint32_t dest = openrac_guest_ram_address(static_cast<uint32_t>(a0));
        const uint32_t src = openrac_guest_ram_address(static_cast<uint32_t>(a1));
        const uint32_t size = static_cast<uint32_t>(a2);
        if (dest > openrac_guest_ram_size || src > openrac_guest_ram_size ||
            size > openrac_guest_ram_size - dest || size > openrac_guest_ram_size - src) {
            openrac_guest_missing("EE Copy range is outside guest RAM");
            return 0;
        }
        std::memmove(openrac_guest_base + dest, openrac_guest_base + src, size);
        return 0;
    }
    case 0x5B: // GetEntryAddress: the host dispatches calls without guest exception vectors.
        return 0;
    case 0x56: // _SetTLBEntry: guest RAM has a direct host mapping.
        return 0;
    case 0x64: // FlushCache: host code has no EE caches.
        return 0;
    case 0x74: // SetSyscall: guest exception vectors are not used by translated host code.
        return 0;
    case 0x70: // GsGetIMR: the host renderer owns GS interrupts, so preserve the guest mask in software.
        return g_gs_imr;
    case 0x71: { // GsPutIMR returns the previous mask.
        const uint64_t previous = g_gs_imr;
        g_gs_imr = a0;
        return previous;
    }
    case 0x7F: // GetMemorySize; Sly's configured host RAM is 32 MiB.
        return GUEST_RAM_SIZE;
    case 0x82: // _InitTLB; the host maps guest RAM directly and has no EE TLB.
        return 0;
    case 0x83: // FindAddress is used only by the bypassed guest exception setup.
        return 0;
    case 0x23: // ExitThread: the port runs a single host thread.
        return 0;
    case 0x20: // CreateThread: worker threads are represented but not scheduled separately.
        return g_next_id.fetch_add(1, std::memory_order_relaxed);
    case 0x21: // DeleteThread
    case 0x22: // StartThread
    case 0x24: // ExitDeleteThread
    case 0x25: // TerminateThread
    case 0x26: // ExitThread
    case 0x29: // ChangeThreadPriority
    case 0x2B: // RotateThreadReadyQueue
    case 0x32: // SleepThread
    case 0x33: // WakeupThread
    case 0x37: // SuspendThread
        return 0;
    case 0x2F: // GetThreadId: the host runs the game on one thread.
        return 1;
    case 0x40: { // CreateSema; the initial count is the third word in SemaParam.
        const uint32_t param = openrac_guest_ram_address(static_cast<uint32_t>(a0));
        if (param > openrac_guest_ram_size || openrac_guest_ram_size - param < 12) {
            openrac_guest_missing("CreateSema parameter is outside guest RAM");
            return static_cast<uint64_t>(-1);
        }
        const int32_t initial = static_cast<int32_t>(GREF(uint32_t, param + 8));
        const uint32_t id = g_next_id.fetch_add(1, std::memory_order_relaxed);
        std::lock_guard lock(g_kernel_mutex);
        g_semaphores[id] = initial;
        return id;
    }
    case 0x41: { // DeleteSema
        std::lock_guard lock(g_kernel_mutex);
        g_semaphores.erase(static_cast<uint32_t>(a0));
        return a0;
    }
    case 0x42:
    case 0x43: { // SignalSema / iSignalSema
        std::lock_guard lock(g_kernel_mutex);
        auto it = g_semaphores.find(static_cast<uint32_t>(a0));
        if (it != g_semaphores.end()) ++it->second;
        return a0;
    }
    case 0x44: { // WaitSema; the single-threaded host cannot block on a missing signal.
        std::lock_guard lock(g_kernel_mutex);
        auto it = g_semaphores.find(static_cast<uint32_t>(a0));
        if (it != g_semaphores.end() && it->second > 0) --it->second;
        return a0;
    }
    case 0x45:
    case 0x46: { // PollSema / iPollSema
        std::lock_guard lock(g_kernel_mutex);
        auto it = g_semaphores.find(static_cast<uint32_t>(a0));
        if (it == g_semaphores.end() || it->second <= 0) return static_cast<uint64_t>(-1);
        --it->second;
        return a0;
    }
    case 0x3E: // EndOfHeap
        return g_stack_top - 0x4000u;
    case 0x4A: // SetOsdConfigParam: settings are owned by the host.
        return 0;
    case 0x4B: { // GetOsdConfigParam: English, non-Japanese console defaults.
        const uint32_t address = openrac_guest_ram_address(static_cast<uint32_t>(a0));
        if (address > openrac_guest_ram_size || openrac_guest_ram_size - address < 4) {
            openrac_guest_missing("GetOsdConfigParam output is outside guest RAM");
            return 0;
        }
        const uint32_t config = 0x00014010u;
        std::memcpy(openrac_guest_base + address, &config, sizeof(config));
        return 0;
    }
    case 0x3C: { // SetupThread(gp, stack, stack_size, args, root)
        const uint32_t stack = static_cast<uint32_t>(a1) == UINT32_MAX
                                   ? GUEST_RAM_SIZE - static_cast<uint32_t>(a2)
                                   : static_cast<uint32_t>(a1);
        g_stack_top = stack + static_cast<uint32_t>(a2);
        if (!openrac_guest_set_stack(stack, g_stack_top)) return 0;
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
