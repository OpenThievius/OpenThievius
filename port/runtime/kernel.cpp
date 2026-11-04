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

namespace {
std::atomic<bool> g_exit_requested{false};
uint32_t g_stack_top = GUEST_RAM_SIZE;
std::atomic<uint32_t> g_next_id{2};
std::mutex g_kernel_mutex;
std::map<uint32_t, int32_t> g_semaphores;
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

extern "C" uint64_t openrac_guest_kernel_syscall(uint32_t service, uint64_t a0, uint64_t a1,
                                                  uint64_t a2, uint64_t, uint64_t,
                                                  uint64_t, uint64_t, uint64_t) {
    switch (service) {
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
