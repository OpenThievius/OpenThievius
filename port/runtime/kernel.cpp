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
#include <condition_variable>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include "common/log.h"
#include "openrac/ee.h"
#include "openrac/host_env.h"

namespace {
std::atomic<bool> g_exit_requested{false};
std::atomic<bool> g_cpu_interrupts_enabled{true};
thread_local uint32_t g_stack_top = GUEST_RAM_SIZE;
std::atomic<uint32_t> g_next_id{2};
std::atomic<uint32_t> g_last_kernel_service{0xFFFFFFFFu};
std::atomic<uint32_t> g_last_kernel_thread{1};
std::atomic<uint32_t> g_sif_trace_get_count{0};
std::atomic<uint32_t> g_sif_trace_set_count{0};
std::mutex g_kernel_mutex;
struct Semaphore {
    std::mutex mutex;
    std::condition_variable changed;
    int32_t count = 0;
    int32_t max_count = INT32_MAX;
    bool deleted = false;
};
struct GuestThread {
    gaddr entry;
    gaddr stack;
    uint32_t stack_size;
    bool started = false;
    uint32_t status = 0x10u; // THS_DORMANT
    uint32_t wait_type = 0;
    uint32_t wait_id = 0;
    uint32_t wakeup_count = 0;
};
struct EeThreadStatus {
    uint32_t status;
    gaddr entry;
    gaddr stack;
    int32_t stack_size;
    gaddr gp;
    int32_t initial_priority;
    int32_t current_priority;
    uint32_t attr;
    uint32_t option;
    uint32_t wait_type;
    uint32_t wait_id;
    uint32_t wakeup_count;
};
static_assert(sizeof(EeThreadStatus) == 0x30);
struct DmacHandler { gaddr entry; uint32_t id; };
std::map<uint32_t, std::shared_ptr<Semaphore>> g_semaphores;
std::map<uint32_t, GuestThread> g_threads;
std::map<uint32_t, DmacHandler> g_dmac_handlers;
std::set<uint32_t> g_enabled_dmac;
std::map<uint32_t, uint32_t> g_sif_registers;
std::map<uint32_t, uint32_t> g_interrupt_handlers;
std::set<uint32_t> g_enabled_interrupts;
uint64_t g_gs_imr = 0x7F00u;
thread_local uint32_t g_current_thread_id = 1;

std::shared_ptr<Semaphore> find_semaphore(uint32_t id) {
    std::lock_guard lock(g_kernel_mutex);
    const auto it = g_semaphores.find(id);
    return it == g_semaphores.end() ? nullptr : it->second;
}
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

extern "C" uint32_t openrac_guest_last_kernel_service(void) {
    return g_last_kernel_service.load(std::memory_order_relaxed);
}

extern "C" uint32_t openrac_guest_last_kernel_thread(void) {
    return g_last_kernel_thread.load(std::memory_order_relaxed);
}

extern "C" uint32_t openrac_guest_current_thread_id(void) {
    return g_current_thread_id;
}

extern "C" gaddr openrac_guest_thread_entry(uint32_t thread_id) {
    std::lock_guard lock(g_kernel_mutex);
    const auto it = g_threads.find(thread_id);
    return it == g_threads.end() ? 0 : it->second.entry;
}

extern "C" void openrac_guest_sif_set_register(uint32_t reg, uint32_t value) {
    std::lock_guard lock(g_kernel_mutex);
    g_sif_registers[reg] = value;
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

extern "C" int openrac_lib_sly1_signal_sema(uint32_t semaphore) {
    // Retail's 16-byte wrapper at func_001F6B50 issues iSignalSema.
    return static_cast<int>(openrac_guest_kernel_syscall(0x43u, semaphore, 0, 0, 0,
                                                         0, 0, 0, 0));
}

extern "C" int openrac_lib_sly1_wait_sema(uint32_t semaphore) {
    // Retail's 16-byte wrapper at func_001F6B60 issues WaitSema.
    return static_cast<int>(openrac_guest_kernel_syscall(0x44u, semaphore, 0, 0, 0,
                                                         0, 0, 0, 0));
}

extern "C" int openrac_lib_sly1_create_sema(uint32_t parameters) {
    return static_cast<int>(openrac_guest_kernel_syscall(0x40u, parameters, 0, 0, 0,
                                                         0, 0, 0, 0));
}

extern "C" int openrac_lib_sly1_start_thread(int thread, uint32_t argument) {
    return static_cast<int>(openrac_guest_kernel_syscall(0x22u,
        static_cast<uint32_t>(thread), argument, 0, 0, 0, 0, 0, 0));
}

extern "C" int openrac_lib_sly1_dmac_add_handler(uint32_t channel, uint32_t handler,
                                                    uint32_t next, uint32_t argument,
                                                    uint32_t flags) {
    return static_cast<int>(openrac_guest_kernel_syscall(0x12u, channel, handler, next,
                                                         argument, flags, 0, 0, 0));
}

extern "C" int openrac_lib_sly1_dmac_enable(uint32_t channel) {
    // The retail wrapper preserves its interrupt state around EnableDmac.
    return static_cast<int>(openrac_guest_kernel_syscall(0x16u, channel, 0, 0, 0,
                                                         0, 0, 0, 0));
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

extern "C" int openrac_guest_raise_dmac(uint32_t channel) {
    DmacHandler callback{};
    {
        std::lock_guard lock(g_kernel_mutex);
        if (!g_enabled_dmac.contains(channel)) return 0;
        const auto it = g_dmac_handlers.find(channel);
        if (it == g_dmac_handlers.end()) return 0;
        callback = it->second;
    }
    const uint64_t ints[8] = {channel};
    const float floats[8] = {};
    float result = 0.0f;
    openrac_guest_call_regs(callback.entry, ints, floats, &result, 0);
    return 1;
}

extern "C" uint64_t openrac_guest_kernel_syscall(uint32_t service, uint64_t a0, uint64_t a1,
                                                  uint64_t a2, uint64_t, uint64_t,
                                                  uint64_t, uint64_t, uint64_t) {
    g_last_kernel_service.store(service, std::memory_order_relaxed);
    g_last_kernel_thread.store(g_current_thread_id, std::memory_order_relaxed);
    switch (service) {
    case 0x12: { // AddDmacHandler
        const uint32_t channel = static_cast<uint32_t>(a0);
        const gaddr handler = static_cast<gaddr>(a1);
        if (channel >= 10 || handler == 0) return static_cast<uint64_t>(-1);
        const uint32_t id = g_next_id.fetch_add(1, std::memory_order_relaxed);
        std::lock_guard lock(g_kernel_mutex);
        g_dmac_handlers[channel] = DmacHandler{handler, id};
        return id;
    }
    case 0x16: { // EnableDmac
        const uint32_t channel = static_cast<uint32_t>(a0);
        std::lock_guard lock(g_kernel_mutex);
        const bool was_enabled = g_enabled_dmac.contains(channel);
        g_enabled_dmac.insert(channel);
        return was_enabled ? 0u : 1u;
    }
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
    case 0x79: { // SifSetReg; SIF registers are represented in host memory.
        if (openrac_env_string("OPENRAC_TRACE_SIF") != nullptr &&
            g_sif_trace_set_count.fetch_add(1, std::memory_order_relaxed) < 32) {
            openrac::log::info("SIF SetReg thread={} reg={:#010x} value={:#010x}",
                               g_current_thread_id, static_cast<uint32_t>(a0),
                               static_cast<uint32_t>(a1));
        }
        openrac_guest_sif_set_register(static_cast<uint32_t>(a0), static_cast<uint32_t>(a1));
        return 0;
    }
    case 0x78: // SifSetDChain; the native runtime has no EE DMA channel to configure.
    case static_cast<uint32_t>(-0x78):
        return 0;
    case 0x7A: { // SifGetReg
        std::lock_guard lock(g_kernel_mutex);
        const auto it = g_sif_registers.find(static_cast<uint32_t>(a0));
        const uint32_t value = it == g_sif_registers.end() ? 0 : it->second;
        if (openrac_env_string("OPENRAC_TRACE_SIF") != nullptr &&
            g_sif_trace_get_count.fetch_add(1, std::memory_order_relaxed) < 32) {
            openrac::log::info("SIF GetReg thread={} reg={:#010x} value={:#010x}",
                               g_current_thread_id, static_cast<uint32_t>(a0), value);
        }
        return value;
    }
    case 0x7F: // GetMemorySize; Sly's configured host RAM is 32 MiB.
        return GUEST_RAM_SIZE;
    case 0x30: { // ReferThreadStatus: report native guest-thread state in EE layout.
        const uint32_t requested = static_cast<uint32_t>(a0);
        const uint32_t id = requested == 0 ? g_current_thread_id : requested;
        const uint32_t output = openrac_guest_ram_address(static_cast<uint32_t>(a1));
        if (output > openrac_guest_ram_size || openrac_guest_ram_size - output < sizeof(EeThreadStatus))
            return static_cast<uint64_t>(-1);

        EeThreadStatus status{};
        if (id == g_current_thread_id) {
            status.status = 0x01u; // THS_RUN
            status.stack = openrac_guest_sp;
            status.current_priority = 10;
            status.initial_priority = 10;
        } else {
            std::lock_guard lock(g_kernel_mutex);
            const auto it = g_threads.find(id);
            if (it == g_threads.end()) return static_cast<uint64_t>(-1);
            status.status = it->second.status;
            status.entry = it->second.entry;
            status.stack = it->second.stack;
            status.stack_size = static_cast<int32_t>(it->second.stack_size);
            status.initial_priority = 10;
            status.current_priority = 10;
            status.wait_type = it->second.wait_type;
            status.wait_id = it->second.wait_id;
            status.wakeup_count = it->second.wakeup_count;
        }
        std::memcpy(openrac_guest_base + output, &status, sizeof(status));
        return 0;
    }
    case 0x82: // _InitTLB; the host maps guest RAM directly and has no EE TLB.
        return 0;
    case 0x83: // FindAddress is used only by the bypassed guest exception setup.
        return 0;
    case 0x20: { // CreateThread(ThreadParam*): retain the entry and its guest stack.
        const uint32_t param = openrac_guest_ram_address(static_cast<uint32_t>(a0));
        if (param > openrac_guest_ram_size || openrac_guest_ram_size - param < 24) {
            openrac::log::warn("CreateThread parameter {:#010x} is outside guest RAM", param);
            return static_cast<uint64_t>(-1);
        }
        const gaddr entry = GREF(uint32_t, param + 4);
        const gaddr raw_stack = GREF(uint32_t, param + 8);
        const gaddr stack = openrac_guest_ram_address(raw_stack);
        const uint32_t stack_size = GREF(uint32_t, param + 12);
        if (entry == 0 || stack > openrac_guest_ram_size || stack_size == 0 ||
            stack_size > openrac_guest_ram_size - stack) {
            openrac::log::warn(
                "CreateThread rejected: param={:#010x} entry={:#010x} stack={:#010x} "
                "normalized_stack={:#010x} stack_size={:#x} guest_ram={:#x}",
                param, entry, raw_stack, stack, stack_size, openrac_guest_ram_size
            );
            return static_cast<uint64_t>(-1);
        }
        const uint32_t id = g_next_id.fetch_add(1, std::memory_order_relaxed);
        std::lock_guard lock(g_kernel_mutex);
        g_threads.emplace(id, GuestThread{entry, stack, stack_size, false});
        return id;
    }
    case 0x22: { // StartThread(thread_id, arg): execute the guest entry on a host thread.
        const uint32_t id = static_cast<uint32_t>(a0);
        GuestThread guest_thread{};
        {
            std::lock_guard lock(g_kernel_mutex);
            const auto it = g_threads.find(id);
            if (it == g_threads.end() || it->second.started) return static_cast<uint64_t>(-1);
            it->second.started = true;
            it->second.status = 0x02u; // THS_READY until the host thread begins.
            guest_thread = it->second;
        }
        try {
            std::thread([id, guest_thread, argument = a1] {
                g_current_thread_id = id;
                {
                    std::lock_guard lock(g_kernel_mutex);
                    if (auto it = g_threads.find(id); it != g_threads.end())
                        it->second.status = 0x01u; // THS_RUN
                }
                const gaddr top = guest_thread.stack + guest_thread.stack_size;
                if (openrac_guest_set_stack(guest_thread.stack, top)) {
                    openrac_guest_sp = top;
                    openrac_ee_float_mode(0, 0);
                    uint64_t ints[8] = {argument};
                    const float floats[8] = {};
                    float result = 0.0f;
                    openrac_guest_call_regs(guest_thread.entry, ints, floats, &result, 0);
                }
                std::lock_guard lock(g_kernel_mutex);
                if (auto it = g_threads.find(id); it != g_threads.end())
                    it->second.status = 0x10u; // THS_DORMANT after the guest entry returns.
            }).detach();
        } catch (const std::system_error&) {
            std::lock_guard lock(g_kernel_mutex);
            if (auto it = g_threads.find(id); it != g_threads.end()) it->second.started = false;
            return static_cast<uint64_t>(-1);
        }
        return 0;
    }
    case 0x23: // ExitThread: translated thread entry returns to its host wrapper.
    case 0x26:
        return 0;
    case 0x21: // DeleteThread
    case 0x24: // ExitDeleteThread
    case 0x25: // TerminateThread
    case 0x29: // ChangeThreadPriority
    case 0x2B: // RotateThreadReadyQueue
    case 0x32: // SleepThread
    case 0x33: // WakeupThread
    case 0x37: // SuspendThread
        return 0;
    case 0x2F: // GetThreadId: return the current guest thread's id.
        return g_current_thread_id;
    case 0x40: { // CreateSema; the initial count is the third word in SemaParam.
        const uint32_t param = openrac_guest_ram_address(static_cast<uint32_t>(a0));
        if (param > openrac_guest_ram_size || openrac_guest_ram_size - param < 12) {
            openrac_guest_missing("CreateSema parameter is outside guest RAM");
            return static_cast<uint64_t>(-1);
        }
        const int32_t maximum = static_cast<int32_t>(GREF(uint32_t, param + 4));
        const int32_t initial = static_cast<int32_t>(GREF(uint32_t, param + 8));
        const uint32_t id = g_next_id.fetch_add(1, std::memory_order_relaxed);
        auto semaphore = std::make_shared<Semaphore>();
        semaphore->count = initial;
        semaphore->max_count = maximum > 0 ? maximum : INT32_MAX;
        std::lock_guard lock(g_kernel_mutex);
        g_semaphores[id] = std::move(semaphore);
        return id;
    }
    case 0x41: { // DeleteSema
        std::shared_ptr<Semaphore> semaphore;
        {
            std::lock_guard lock(g_kernel_mutex);
            const auto it = g_semaphores.find(static_cast<uint32_t>(a0));
            if (it != g_semaphores.end()) {
                semaphore = std::move(it->second);
                g_semaphores.erase(it);
            }
        }
        if (semaphore) {
            {
                std::lock_guard lock(semaphore->mutex);
                semaphore->deleted = true;
            }
            semaphore->changed.notify_all();
        }
        return a0;
    }
    case 0x42:
    case 0x43: { // SignalSema / iSignalSema
        auto semaphore = find_semaphore(static_cast<uint32_t>(a0));
        if (!semaphore) return static_cast<uint64_t>(-1);
        {
            std::lock_guard lock(semaphore->mutex);
            if (semaphore->deleted) return static_cast<uint64_t>(-1);
            if (semaphore->count < semaphore->max_count) ++semaphore->count;
        }
        semaphore->changed.notify_one();
        return a0;
    }
    case 0x44: { // WaitSema: sleep the host thread until another guest thread signals it.
        const uint32_t id = static_cast<uint32_t>(a0);
        auto semaphore = find_semaphore(id);
        bool deleted = false;
        if (!semaphore) return static_cast<uint64_t>(-1);
        {
            std::lock_guard lock(g_kernel_mutex);
            if (auto thread = g_threads.find(g_current_thread_id); thread != g_threads.end()) {
                thread->second.status = 0x04u; // THS_WAIT
                thread->second.wait_type = 2u; // TSW_SEMA
                thread->second.wait_id = id;
            }
        }
        {
            std::unique_lock lock(semaphore->mutex);
            semaphore->changed.wait(lock, [&] {
                return semaphore->deleted || semaphore->count > 0;
            });
            deleted = semaphore->deleted;
            if (!deleted) --semaphore->count;
        }
        {
            std::lock_guard lock(g_kernel_mutex);
            if (auto thread = g_threads.find(g_current_thread_id); thread != g_threads.end()) {
                thread->second.status = 0x01u; // THS_RUN
                thread->second.wait_type = 0;
                thread->second.wait_id = 0;
            }
        }
        if (deleted) return static_cast<uint64_t>(-1);
        return a0;
    }
    case 0x45:
    case 0x46: { // PollSema / iPollSema
        auto semaphore = find_semaphore(static_cast<uint32_t>(a0));
        if (!semaphore) return static_cast<uint64_t>(-1);
        std::lock_guard lock(semaphore->mutex);
        if (semaphore->deleted || semaphore->count <= 0)
            return static_cast<uint64_t>(-1);
        --semaphore->count;
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
