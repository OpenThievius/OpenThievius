// SPDX-License-Identifier: AGPL-3.0-or-later
// Native launcher for the translated Sly 1 executable.
#include "openrac/elf.h"
#include "openrac/disc_image.h"
#include "openrac/game_host.h"
#include "openrac/memory.h"
#include "openrac/native_gs.h"
#include "openrac/native_window.h"

#include <SDL3/SDL.h>

extern "C" {
#include "game_protos.h"
}

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
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
            std::uint32_t previous_range_state[8]{};
            bool have_previous_range_state = false;
            auto last_full_trace = std::chrono::steady_clock::now() -
                std::chrono::seconds(1);
            while (!trace_stop.load(std::memory_order_relaxed)) {
                const gaddr address = openrac_guest_last_called_address();
                const char* name = openrac_guest_function_name(address);
                const uint32_t service = openrac_guest_last_kernel_service();
                const uint32_t thread = openrac_guest_last_kernel_thread();
                const gaddr thread_entry = openrac_guest_thread_entry(thread);
                const char* thread_name = openrac_guest_function_name(thread_entry);
                uint64_t framebuffer = 0;
                uint64_t scissor = 0;
                uint64_t bitbltbuf = 0;
                uint64_t trxpos = 0;
                uint64_t trxreg = 0;
                uint64_t trxdir = 0;
                uint64_t pmode = 0;
                uint64_t dispfb1 = 0;
                uint64_t display1 = 0;
                uint64_t tex0_1 = 0;
                uint64_t tex1_1 = 0;
                uint64_t tex0_2 = 0;
                uint64_t xyoffset = 0;
                uint64_t prim = 0;
                uint64_t uv = 0;
                openrac_guest_gs_read_register(0x4Cu, &framebuffer);
                openrac_guest_gs_read_register(0x40u, &scissor);
                openrac_guest_gs_read_register(0x06u, &tex0_1);
                openrac_guest_gs_read_register(0x07u, &tex0_2);
                openrac_guest_gs_read_register(0x18u, &xyoffset);
                openrac_guest_gs_read_register(0x14u, &tex1_1);
                openrac_guest_gs_read_register(0x00u, &prim);
                openrac_guest_gs_read_register(0x03u, &uv);
                openrac_guest_gs_read_register(0x50u, &bitbltbuf);
                openrac_guest_gs_read_register(0x51u, &trxpos);
                openrac_guest_gs_read_register(0x52u, &trxreg);
                openrac_guest_gs_read_register(0x53u, &trxdir);
                openrac_guest_gs_read_privileged_register(0x00u, &pmode);
                openrac_guest_gs_read_privileged_register(0x70u, &dispfb1);
                openrac_guest_gs_read_privileged_register(0x80u, &display1);
                uint32_t vif1_chcr = 0;
                uint32_t vif1_madr = 0;
                uint32_t vif1_qwc = 0;
                uint32_t vif1_tadr = 0;
                std::memcpy(&vif1_chcr, openrac_guest_base + 0x10009000u,
                            sizeof(vif1_chcr));
                std::memcpy(&vif1_madr, openrac_guest_base + 0x10009010u,
                            sizeof(vif1_madr));
                std::memcpy(&vif1_qwc, openrac_guest_base + 0x10009020u,
                            sizeof(vif1_qwc));
                std::memcpy(&vif1_tadr, openrac_guest_base + 0x10009030u,
                            sizeof(vif1_tadr));
                std::uint32_t range_state[8]{};
                std::memcpy(range_state, openrac_guest_base + 0x00275AF0u,
                            sizeof(range_state));
                std::uint32_t range_record[8]{};
                const bool have_range_record = range_state[1] != 0 &&
                    range_state[1] < 0x02000000u;
                if (have_range_record) {
                    std::memcpy(range_record, openrac_guest_base + range_state[1],
                                sizeof(range_record));
                }
                if (!have_previous_range_state ||
                    std::memcmp(previous_range_state, range_state, sizeof(range_state)) != 0) {
                    std::cerr << "[range] state=";
                    for (const std::uint32_t word : range_state)
                        std::cerr << std::hex << word << ',';
                    if (have_range_record) {
                        std::cerr << " record=";
                        for (const std::uint32_t word : range_record)
                            std::cerr << std::hex << word << ',';
                    }
                    std::cerr << std::dec << '\n';
                    std::memcpy(previous_range_state, range_state, sizeof(range_state));
                    have_previous_range_state = true;
                }
                const auto now = std::chrono::steady_clock::now();
                if (now - last_full_trace < std::chrono::seconds(1)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                last_full_trace = now;
                std::cerr << "[trace] guest=" << (name == nullptr ? "unknown" : name)
                          << " address=0x" << std::hex << address << std::dec
                          << " kernel_service=";
                if (service == 0xFFFFFFFFu) std::cerr << "none";
                else std::cerr << "0x" << std::hex << service << std::dec;
                std::cerr << " kernel_thread=" << thread << " thread_entry="
                          << (thread_name == nullptr ? "main/unknown" : thread_name)
                          << "(0x" << std::hex << thread_entry << std::dec << ')'
                          << " gif_eop=" << openrac_guest_gif_eop_count()
                          << " frame1=0x" << std::hex << framebuffer
                          << " scissor=0x" << scissor << " blit=0x" << bitbltbuf
                          << " trxpos=0x" << trxpos << " trxreg=0x" << trxreg
                          << " trxdir=0x" << trxdir << " pmode=0x" << pmode
                          << " tex0_1=0x" << tex0_1 << " tex0_2=0x" << tex0_2
                          << " tex1_1=0x" << tex1_1 << " xyoffset=0x" << xyoffset
                          << " prim=0x" << prim
                          << " uv=0x" << uv << " vif1_chcr=0x" << vif1_chcr
                          << " vif1_madr=0x" << vif1_madr << " vif1_qwc=0x" << vif1_qwc
                          << " vif1_tadr=0x" << vif1_tadr
                          << " range_state=";
                for (const std::uint32_t word : range_state)
                    std::cerr << std::hex << word << ',';
                if (have_range_record) {
                    std::cerr << " range_record=";
                    for (const std::uint32_t word : range_record)
                        std::cerr << std::hex << word << ',';
                }
                std::cerr << std::dec
                          << " dispfb1=0x" << dispfb1 << " display1=0x" << display1
                          << " vertices=" << std::dec << openrac_guest_gs_published_vertex_count()
                          << " image_tags=" << openrac_guest_gif_image_tag_count()
                          << " image_qwords=" << openrac_guest_gif_image_qword_count()
                          << " blit_writes=" << openrac_guest_gs_register_write_count(0x50u)
                          << " trxpos_writes=" << openrac_guest_gs_register_write_count(0x51u)
                          << " trxreg_writes=" << openrac_guest_gs_register_write_count(0x52u)
                          << " trxdir_writes=" << openrac_guest_gs_register_write_count(0x53u)
                          << std::dec;
                std::cerr << " active_threads=";
                for (uint32_t id = 1; id < 16; ++id) {
                    const gaddr active = openrac_guest_last_called_address_for_thread(id);
                    if (active == 0) continue;
                    const char* active_name = openrac_guest_function_name(active);
                    std::cerr << id << ':'
                              << (active_name == nullptr ? "unknown" : active_name)
                              << "(0x" << std::hex << active << std::dec << "),";
                }
                const size_t vertex_count = openrac_guest_gs_published_vertex_count();
                if (vertex_count != 0 && vertex_count <= (1u << 20)) {
                    std::vector<openrac_gs_vertex> vertices(vertex_count);
                    const size_t read_count = openrac_guest_gs_read_vertices(
                        vertices.data(), vertices.size());
                    float min_x = std::numeric_limits<float>::infinity();
                    float min_y = std::numeric_limits<float>::infinity();
                    float max_x = -std::numeric_limits<float>::infinity();
                    float max_y = -std::numeric_limits<float>::infinity();
                    size_t visible_vertices = 0;
                    size_t opaque_vertices = 0;
                    for (size_t i = 0; i < read_count; ++i) {
                        min_x = std::min(min_x, vertices[i].x);
                        min_y = std::min(min_y, vertices[i].y);
                        max_x = std::max(max_x, vertices[i].x);
                        max_y = std::max(max_y, vertices[i].y);
                        if (vertices[i].x >= 0.0f && vertices[i].x < 640.0f &&
                            vertices[i].y >= 0.0f && vertices[i].y < 224.0f)
                            ++visible_vertices;
                        if ((vertices[i].rgba >> 24u) != 0) ++opaque_vertices;
                    }
                    std::cerr << " vertex_bounds=" << min_x << ',' << min_y << '-'
                              << max_x << ',' << max_y
                              << " visible_vertices=" << visible_vertices
                              << " opaque_vertices=" << opaque_vertices;
                    std::cerr << " vertex_samples=";
                    for (size_t i = 0; i < std::min<size_t>(read_count, 3); ++i) {
                        if (i != 0) std::cerr << ';';
                        std::cerr << vertices[i].x << ',' << vertices[i].y << ":0x"
                                  << std::hex << vertices[i].rgba << std::dec;
                    }
                }
                std::cerr << '\n';
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }
    if (!openrac_native_window_create()) {
        trace_stop.store(true, std::memory_order_relaxed);
        if (trace_thread.joinable()) trace_thread.join();
        std::cerr << "could not create the SDL3 native presentation window: "
                  << SDL_GetError() << '\n';
        return 1;
    }

    std::atomic<bool> game_started{false};
    bool game_result = false;
    std::thread game_thread([&] {
        game_result = openrac_game_run();
        game_started.store(true, std::memory_order_release);
    });
    while (!game_started.load(std::memory_order_acquire)) {
        openrac_native_window_present();
        if (openrac_native_window_poll_quit()) {
            game_thread.detach();
            openrac_native_window_destroy();
            trace_stop.store(true, std::memory_order_relaxed);
            if (trace_thread.joinable()) trace_thread.join();
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    game_thread.join();
    openrac_native_window_destroy();
    if (!game_result) {
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
