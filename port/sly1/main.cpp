// SPDX-License-Identifier: AGPL-3.0-or-later
// Native launcher for the translated Sly 1 executable.
#include "openrac/elf.h"
#include "openrac/disc_image.h"
#include "openrac/game_host.h"
#include "openrac/memory.h"

extern "C" {
#include "game_protos.h"
}

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

extern "C" {
const char* openrac_game_disc_image = nullptr;
}

namespace {

#define SLY1_REGISTER_ENTRY(name, target)                                                        \
    uint64_t name(                                                                               \
        uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,          \
        float, float, float, float, float, float, float, float                                   \
    ) {                                                                                          \
        (void)target(1, -1);                                                                     \
        return 0;                                                                                \
    }                                                                                            \
    uint64_t name##_thunk(const uint64_t*, const float*, float*) {                               \
        (void)target(1, -1);                                                                     \
        return 0;                                                                                \
    }

SLY1_REGISTER_ENTRY(register_0016c450, func_0016C360)
SLY1_REGISTER_ENTRY(register_001963b8, func_00196330)
SLY1_REGISTER_ENTRY(register_001aec90, func_001AEA70)
SLY1_REGISTER_ENTRY(register_001e08d8, func_001E0848)
SLY1_REGISTER_ENTRY(register_001f2738, func_001F2688)

#undef SLY1_REGISTER_ENTRY

void register_sly1_subentries() {
    static const openrac_fn_entry functions[] = {
        {0x0016C450u, reinterpret_cast<openrac_host_fn>(register_0016c450), "func_0016C450"},
        {0x001963B8u, reinterpret_cast<openrac_host_fn>(register_001963b8), "func_001963B8"},
        {0x001AEC90u, reinterpret_cast<openrac_host_fn>(register_001aec90), "func_001AEC90"},
        {0x001E08D8u, reinterpret_cast<openrac_host_fn>(register_001e08d8), "func_001E08D8"},
        {0x001F2738u, reinterpret_cast<openrac_host_fn>(register_001f2738), "func_001F2738"},
    };
    static const openrac_thunk_entry thunks[] = {
        {reinterpret_cast<openrac_host_fn>(register_0016c450), register_0016c450_thunk},
        {reinterpret_cast<openrac_host_fn>(register_001963b8), register_001963b8_thunk},
        {reinterpret_cast<openrac_host_fn>(register_001aec90), register_001aec90_thunk},
        {reinterpret_cast<openrac_host_fn>(register_001e08d8), register_001e08d8_thunk},
        {reinterpret_cast<openrac_host_fn>(register_001f2738), register_001f2738_thunk},
    };
    openrac_guest_register(OPENRAC_OVERLAY_EXE, functions, sizeof(functions) / sizeof(functions[0]));
    openrac_guest_register_thunks(thunks, sizeof(thunks) / sizeof(thunks[0]));
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2 && argc != 3) {
        std::cerr << "usage: sly1_native <extracted Sly 1 boot executable> [disc ISO or BIN]\n";
        return 2;
    }

    std::vector<std::uint8_t> executable;
    if (!openrac::runtime::read_file(argv[1], &executable)) {
        std::cerr << "could not read the Sly 1 boot executable: " << argv[1] << '\n';
        return 1;
    }

    openrac::runtime::ElfImage image;
    std::string reason;
    if (!openrac::runtime::read_elf(executable, &image, &reason)) {
        std::cerr << "invalid Sly 1 boot executable: " << reason << '\n';
        return 1;
    }

    openrac::runtime::Memory::create();
    if (!openrac::runtime::load_elf(executable, image, &reason)) {
        std::cerr << "could not load the Sly 1 boot executable: " << reason << '\n';
        return 1;
    }

    if (argc == 3) {
        openrac_game_disc_image = argv[2];
        if (!openrac_guest_disc_open(openrac_game_disc_image)) {
            std::cerr << "could not open the Sly 1 disc image as ISO 9660: " << argv[2] << '\n';
            return 1;
        }
    }

    openrac::runtime::install_crash_handler();
    openrac_game_register_functions();
    register_sly1_subentries();
    openrac_guest_set_overlay(OPENRAC_OVERLAY_EXE);
    std::atomic<bool> trace_stop{false};
    std::thread trace_thread;
    const char* trace_setting = std::getenv("OPENRAC_STARTUP_TRACE");
    if (trace_setting != nullptr && std::string(trace_setting) != "0") {
        openrac_guest_trace_enable(1);
        trace_thread = std::thread([&trace_stop] {
            while (!trace_stop.load(std::memory_order_relaxed)) {
                const gaddr address = openrac_guest_last_called_address();
                const char* name = openrac_guest_function_name(address);
                const uint32_t service = openrac_guest_last_kernel_service();
                const uint32_t thread = openrac_guest_last_kernel_thread();
                const gaddr thread_entry = openrac_guest_thread_entry(thread);
                const char* thread_name = openrac_guest_function_name(thread_entry);
                std::cerr << "[trace] guest=" << (name == nullptr ? "unknown" : name)
                          << " address=0x" << std::hex << address << std::dec
                          << " kernel_service=";
                if (service == 0xFFFFFFFFu) std::cerr << "none";
                else std::cerr << "0x" << std::hex << service << std::dec;
                std::cerr << " kernel_thread=" << thread << " thread_entry="
                          << (thread_name == nullptr ? "main/unknown" : thread_name)
                          << "(0x" << std::hex << thread_entry << std::dec << ')';
                std::cerr << '\n';
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        });
    }
    if (!openrac_game_run()) {
        trace_stop.store(true, std::memory_order_relaxed);
        if (trace_thread.joinable()) trace_thread.join();
        std::cerr << "the Sly 1 entry point is not available in this build\n";
        return 1;
    }
    trace_stop.store(true, std::memory_order_relaxed);
    if (trace_thread.joinable()) trace_thread.join();
    openrac_guest_report_missing();
    if (openrac_guest_exit_requested()) {
        std::cerr << "Sly 1 requested guest Exit; the native frame loop is not active yet\n";
    } else {
        std::cerr << "Sly 1 entry returned without requesting guest Exit\n";
    }
    return 0;
}
