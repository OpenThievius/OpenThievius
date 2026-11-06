// SPDX-License-Identifier: AGPL-3.0-or-later
// Native launcher for the translated Sly 1 executable.
#include "openrac/elf.h"
#include "openrac/disc_image.h"
#include "openrac/game_host.h"
#include "openrac/memory.h"
#include "openrac/native_gs.h"
#include "openrac/native_window.h"
#include "../platform/clock.h"

#include <SDL3/SDL.h>

extern "C" {
#include "game_protos.h"
}

#include <atomic>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <vector>

extern "C" {
const char* openrac_game_disc_image = nullptr;
void sly1_native_start_worker(void);
void sly1_native_start_loader(void);
}

extern "C" void game_sly1_native_trace_font(std::uint32_t stage, std::uint32_t a0,
                                              std::uint32_t a1, std::uint32_t a2,
                                              std::uint32_t a3, std::uint32_t a4) {
    if (std::getenv("OPENRAC_TRACE_FONT") == nullptr) return;
    static std::array<std::atomic<unsigned>, 36> traces{};
    if (stage >= traces.size()) return;
    const unsigned index = traces[stage].fetch_add(1, std::memory_order_relaxed);
    constexpr unsigned limits[] = {2, 2, 8, 16, 4, 4, 4, 2, 8, 16, 16, 8, 8, 8, 8, 8, 24, 32, 8, 4, 4, 4, 64, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8};
    if (index >= limits[stage]) return;
    std::cerr << "[font trace] stage=" << stage << " args=" << std::hex << a0 << ','
              << a1 << ',' << a2 << ',' << a3 << ',' << a4 << std::dec << '\n';
    if (stage == 7 && std::getenv("OPENRAC_DUMP_FONT_RESOURCE") != nullptr &&
        a0 <= 0xffffffffu - a1) {
        std::ofstream dump("/tmp/sly1-font-resource.bin", std::ios::binary);
        if (dump) dump.write(reinterpret_cast<const char*>(openrac_guest_base + a0), a1);
    }
    if (stage == 8 && std::getenv("OPENRAC_DUMP_FONT_PACKETS") != nullptr &&
        a1 <= a2 && a2 - a1 <= 0x10000u) {
        static std::atomic<unsigned> packet_index{};
        const unsigned packet = packet_index.fetch_add(1, std::memory_order_relaxed);
        const std::string path = "/tmp/sly1-font-packet-" +
                                 std::to_string(packet) + ".bin";
        std::ofstream dump(path, std::ios::binary);
        if (dump) dump.write(reinterpret_cast<const char*>(openrac_guest_base + a1),
                             a2 - a1);
    }
}

extern "C" void game_sly1_native_trace_resource_range(std::uint32_t sector,
                                                         std::uint32_t byte_count) {
    if (std::getenv("OPENRAC_STARTUP_TRACE") == nullptr) return;
    std::cerr << "[resource-range] sector=" << sector << " bytes=" << byte_count
              << '\n';
}

extern "C" std::uint32_t game_sly1_native_boot_level_key() {
    static const std::uint32_t key = [] {
        const char* value = std::getenv("OPENRAC_BOOT_LEVEL");
        if (value == nullptr || *value == '\0') return 0u;
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(value, &end, 0);
        if (end == value || *end != '\0' || parsed > 0xFFFFFFFFul) return 0u;
        return static_cast<std::uint32_t>(parsed);
    }();
    return key;
}

namespace {

#define SLY1_REGISTER_ENTRY(name, target)                                                        \
    uint64_t name(                                                                               \
        uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,          \
        float, float, float, float, float, float, float, float                                   \
    ) {                                                                                          \
        using RegisterTarget = int (*)(int, int);                                                \
        (void)reinterpret_cast<RegisterTarget>(target)(1, -1);                                   \
        return 0;                                                                                \
    }                                                                                            \
    uint64_t name##_thunk(const uint64_t*, const float*, float*) {                               \
        using RegisterTarget = int (*)(int, int);                                                \
        (void)reinterpret_cast<RegisterTarget>(target)(1, -1);                                   \
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

void register_sly1_native_backfills() {
    static const openrac_fn_entry functions[] = {
        {0x0018E5E8u, sly1_native_start_worker, "sly1_native_start_worker"},
        {0x001E2398u, sly1_native_start_loader, "sly1_native_start_loader"},
        {0x0015E810u, reinterpret_cast<openrac_host_fn>(func_0015E810), "func_0015E810"},
        {0x0015EDB8u, reinterpret_cast<openrac_host_fn>(func_0015EDB8), "func_0015EDB8"},
        {0x0015EE80u, reinterpret_cast<openrac_host_fn>(func_0015EE80), "func_0015EE80"},
    };
    openrac_guest_replace_functions(OPENRAC_OVERLAY_EXE, functions,
                                    sizeof(functions) / sizeof(functions[0]));
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
    register_sly1_native_backfills();
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
                std::uint32_t frame_state[13]{};
                std::memcpy(frame_state, openrac_guest_base + 0x00275BC0u,
                            sizeof(frame_state));
                std::uint32_t frame_top = 0;
                std::uint32_t frame_top_vtable = 0;
                std::uint32_t frame_top_callback = 0;
                std::uint32_t frame_top_kind = 0;
                std::uint32_t frame_top_mode = 0;
                std::uint32_t frame_top_state = 0;
                std::uint32_t frame_top_flags = 0;
                std::uint32_t frame_top_action = 0;
                std::uint32_t frame_top_pending = 0;
                std::uint32_t frame_top_timer_bits = 0;
                std::uint32_t global_state_ptr = 0;
                std::uint32_t global_state[10]{};
                const std::uint32_t frame_count = frame_state[11];
                if (frame_count > 0 && frame_count <= 8) {
                    std::memcpy(&frame_top,
                                openrac_guest_base + 0x00275BC0u + 0x0Cu +
                                    (frame_count - 1u) * sizeof(frame_top),
                                sizeof(frame_top));
                    const std::uint32_t frame_top_physical =
                        openrac_guest_ram_address(frame_top);
                    if (frame_top != 0 && frame_top_physical <= openrac_guest_ram_size &&
                        0x28Cu <=
                            openrac_guest_ram_size - frame_top_physical) {
                        std::memcpy(&frame_top_vtable,
                                    openrac_guest_base + frame_top_physical,
                                    sizeof(frame_top_vtable));
                        std::memcpy(&frame_top_mode,
                                    openrac_guest_base + frame_top_physical + 0x250u,
                                    sizeof(frame_top_mode));
                        std::memcpy(&frame_top_kind,
                                    openrac_guest_base + frame_top_physical + 0x260u,
                                    sizeof(frame_top_kind));
                        std::memcpy(&frame_top_state,
                                    openrac_guest_base + frame_top_physical + 0x274u,
                                    sizeof(frame_top_state));
                        std::memcpy(&frame_top_flags,
                                    openrac_guest_base + frame_top_physical + 0x278u,
                                    sizeof(frame_top_flags));
                        std::memcpy(&frame_top_pending,
                                    openrac_guest_base + frame_top_physical + 0x288u,
                                    sizeof(frame_top_pending));
                        std::memcpy(&frame_top_action,
                                    openrac_guest_base + frame_top_physical + 0x27Cu,
                                    sizeof(frame_top_action));
                        std::memcpy(&frame_top_timer_bits,
                                    openrac_guest_base + frame_top_physical + 0x254u,
                                    sizeof(frame_top_timer_bits));
                        const std::uint32_t vtable_physical =
                            openrac_guest_ram_address(frame_top_vtable);
                        if (frame_top_vtable != 0 &&
                            vtable_physical <= openrac_guest_ram_size &&
                            0x14u <= openrac_guest_ram_size - vtable_physical) {
                            std::memcpy(&frame_top_callback,
                                        openrac_guest_base + vtable_physical + 0x10u,
                                        sizeof(frame_top_callback));
                        }
                    }
                }
                std::memcpy(&global_state_ptr, openrac_guest_base + 0x002623C0u,
                            sizeof(global_state_ptr));
                const std::uint32_t global_state_physical =
                    openrac_guest_ram_address(global_state_ptr);
                if (global_state_ptr != 0 && global_state_physical <= openrac_guest_ram_size &&
                    0x1A00u <= openrac_guest_ram_size - global_state_physical) {
                    std::memcpy(&global_state[0], openrac_guest_base + global_state_physical + 0x19D8u, 4);
                    std::memcpy(&global_state[1], openrac_guest_base + global_state_physical + 0x19DCu, 4);
                    std::memcpy(&global_state[2], openrac_guest_base + global_state_physical + 0x19E0u, 4);
                    std::memcpy(&global_state[3], openrac_guest_base + global_state_physical + 0x19E4u, 4);
                    std::memcpy(&global_state[4], openrac_guest_base + global_state_physical + 0x19E8u, 4);
                    std::memcpy(&global_state[5], openrac_guest_base + global_state_physical + 0x19ECu, 4);
                    std::memcpy(&global_state[6], openrac_guest_base + global_state_physical + 0x19F0u, 4);
                    std::memcpy(&global_state[7], openrac_guest_base + global_state_physical + 0x19F4u, 4);
                    std::memcpy(&global_state[8], openrac_guest_base + global_state_physical + 0x19F8u, 4);
                    std::memcpy(&global_state[9], openrac_guest_base + global_state_physical + 0x19FCu, 4);
                }
                std::uint32_t menu_state[4]{};
                std::memcpy(&menu_state[1], openrac_guest_base + 0x002701C8u,
                            sizeof(menu_state[1]));
                std::memcpy(&menu_state[2], openrac_guest_base + 0x002701CCu,
                            sizeof(menu_state[2]));
                std::memcpy(&menu_state[3], openrac_guest_base + 0x002701C4u,
                            sizeof(menu_state[3]));
                float menu_blend = 0.0f;
                float menu_since = 0.0f;
                std::uint32_t menu_clock = 0;
                float menu_clock_value = 0.0f;
                std::uint32_t menu_font = 0;
                std::uint32_t menu_title = 0;
                std::uint32_t menu_items = 0;
                std::uint32_t menu_count = 0;
                std::uint32_t menu_glyphs = 0;
                std::uint32_t menu_glyph_count = 0;
                std::uint32_t movie_on = 0;
                std::uint32_t movie_status = 0;
                std::uint32_t pending_resource = 0;
                std::uint32_t resource_state_mode = 0;
                std::uint32_t resource_state_busy = 0;
                std::memcpy(&menu_font, openrac_guest_base + 0x0026FF6Cu,
                            sizeof(menu_font));
                std::memcpy(&menu_title, openrac_guest_base + 0x00269D50u,
                            sizeof(menu_title));
                std::memcpy(&menu_count, openrac_guest_base + 0x00269D60u,
                            sizeof(menu_count));
                std::memcpy(&menu_items, openrac_guest_base + 0x00269D64u,
                            sizeof(menu_items));
                std::memcpy(&movie_on, openrac_guest_base + 0x00269A04u,
                            sizeof(movie_on));
                std::memcpy(&movie_status, openrac_guest_base + 0x00269A10u,
                            sizeof(movie_status));
                std::memcpy(&pending_resource, openrac_guest_base + 0x00275AF0u,
                            sizeof(pending_resource));
                std::memcpy(&resource_state_busy, openrac_guest_base + 0x00275F84u,
                            sizeof(resource_state_busy));
                std::memcpy(&resource_state_mode, openrac_guest_base + 0x00275FA4u,
                            sizeof(resource_state_mode));
                if (menu_font != 0) {
                    const std::uint32_t font_physical = openrac_guest_ram_address(menu_font);
                    if (font_physical <= openrac_guest_ram_size &&
                        0x80u <= openrac_guest_ram_size - font_physical) {
                        std::memcpy(&menu_glyph_count,
                                    openrac_guest_base + font_physical + 0x78u,
                                    sizeof(menu_glyph_count));
                        std::memcpy(&menu_glyphs,
                                    openrac_guest_base + font_physical + 0x7Cu,
                                    sizeof(menu_glyphs));
                    }
                }
                std::memcpy(&menu_blend, openrac_guest_base + 0x002701A4u,
                            sizeof(menu_blend));
                std::memcpy(&menu_since, openrac_guest_base + 0x002701BCu,
                            sizeof(menu_since));
                std::memcpy(&menu_clock, openrac_guest_base + 0x002701C0u,
                            sizeof(menu_clock));
                const std::uint32_t menu_clock_physical =
                    openrac_guest_ram_address(menu_clock);
                if (menu_clock != 0 && menu_clock_physical <= openrac_guest_ram_size &&
                    sizeof(menu_clock_value) <= openrac_guest_ram_size - menu_clock_physical) {
                    std::memcpy(&menu_clock_value, openrac_guest_base + menu_clock_physical,
                                sizeof(menu_clock_value));
                }
                std::uint32_t overlay_state[3]{};
                std::memcpy(&overlay_state[0], openrac_guest_base + 0x00275BC4u,
                            sizeof(overlay_state[0]));
                std::memcpy(&overlay_state[1], openrac_guest_base + 0x00275BC8u,
                            sizeof(overlay_state[1]));
                std::memcpy(&overlay_state[2], openrac_guest_base + 0x00275BECu,
                            sizeof(overlay_state[2]));
                std::uint32_t ui_active_count = 0;
                std::uint32_t ui_visible_count = 0;
                std::uint32_t ui_font_count = 0;
                struct UiSample {
                    std::uint32_t index;
                    std::uint32_t object;
                    std::uint32_t mode;
                    std::uint32_t font;
                    std::uint32_t frame;
                    std::uint32_t draw;
                    std::uint32_t font_vtable;
                    std::uint32_t font_advance;
                    std::uint32_t font_height;
                    std::uint32_t font_image_a;
                    std::uint32_t font_image_b;
                    std::uint64_t font_tex0;
                    std::uint32_t glyph_count;
                    std::uint32_t glyphs;
                    std::uint32_t first_text_byte;
                    std::uint32_t menu_title;
                    std::uint32_t menu_items;
                    std::uint32_t menu_count;
                    std::uint32_t item0_index;
                    std::uint32_t item0_string;
                    std::uint32_t item0_text_byte;
                } ui_samples[8]{};
                std::uint32_t ui_sample_count = 0;
                for (std::uint32_t i = 0; i < 37; ++i) {
                    std::uint32_t object = 0;
                    std::memcpy(&object, openrac_guest_base + 0x002486B0u + i * 4u,
                                sizeof(object));
                    ui_active_count += object != 0;
                    if (object == 0) continue;
                    const std::uint32_t object_physical = openrac_guest_ram_address(object);
                    if (object_physical > openrac_guest_ram_size ||
                        0x25Du > openrac_guest_ram_size - object_physical) continue;
                    std::uint32_t mode = 0, font = 0, frame = 0, vtable = 0;
                    std::memcpy(&mode, openrac_guest_base + object_physical + 0x250u, 4);
                    std::memcpy(&font, openrac_guest_base + object_physical + 0x04u, 4);
                    std::memcpy(&frame, openrac_guest_base + object_physical + 0x210u, 4);
                    std::memcpy(&vtable, openrac_guest_base + object_physical, 4);
                    const std::uint32_t first_text_byte =
                        openrac_guest_base[object_physical + 0x08u];
                    std::uint32_t draw = 0;
                    const std::uint32_t vtable_physical = openrac_guest_ram_address(vtable);
                    if (vtable != 0 && vtable_physical <= openrac_guest_ram_size &&
                        0x28u <= openrac_guest_ram_size - vtable_physical)
                        std::memcpy(&draw, openrac_guest_base + vtable_physical + 9u * 4u, 4);
                    std::uint32_t font_vtable = 0, font_advance = 0;
                    std::uint32_t font_height = 0, glyph_count = 0, glyphs = 0;
                    std::uint32_t font_image_a = 0, font_image_b = 0;
                    std::uint64_t font_tex0 = 0;
                    const bool font_in_port_heap =
                        font >= GUEST_PORT && font - GUEST_PORT < GUEST_PORT_SIZE;
                    const std::uint32_t font_physical = font_in_port_heap
                        ? font : openrac_guest_ram_address(font);
                    const std::uint32_t font_limit = font_in_port_heap
                        ? GUEST_PORT + GUEST_PORT_SIZE : openrac_guest_ram_size;
                    if (font != 0 && font_physical <= font_limit &&
                        0x80u <= font_limit - font_physical) {
                        std::memcpy(&font_advance, openrac_guest_base + font_physical + 4u, 4);
                        std::memcpy(&font_height, openrac_guest_base + font_physical + 8u, 4);
                        std::memcpy(&font_vtable, openrac_guest_base + font_physical + 0x4Cu, 4);
                        std::memcpy(&font_image_a, openrac_guest_base + font_physical + 0x54u, 4);
                        std::memcpy(&font_image_b, openrac_guest_base + font_physical + 0x58u, 4);
                        std::memcpy(&font_tex0, openrac_guest_base + font_physical + 0x60u, 8);
                        std::memcpy(&glyph_count, openrac_guest_base + font_physical + 0x78u, 4);
                        std::memcpy(&glyphs, openrac_guest_base + font_physical + 0x7Cu, 4);
                    }
                    std::uint32_t menu_title = 0, menu_items = 0, menu_count = 0;
                    std::uint32_t item0_index = 0, item0_string = 0, item0_text_byte = 0;
                    std::uint32_t page = 0;
                    std::memcpy(&page, openrac_guest_base + object_physical + 0x260u, 4);
                    if (mode == 2 && page < 33u) {
                        const std::uint32_t menu = 0x00269D50u + page * 24u;
                        std::memcpy(&menu_title, openrac_guest_base + menu, 4);
                        std::memcpy(&menu_count, openrac_guest_base + menu + 0x10u, 4);
                        std::memcpy(&menu_items, openrac_guest_base + menu + 0x14u, 4);
                        const std::uint32_t items_physical = openrac_guest_ram_address(menu_items);
                        if (menu_count != 0 && menu_items != 0 &&
                            items_physical <= openrac_guest_ram_size &&
                            4u <= openrac_guest_ram_size - items_physical) {
                            std::memcpy(&item0_index, openrac_guest_base + items_physical, 4);
                            const std::uint32_t string_slot = 0x0026A068u + item0_index * 4u;
                            if (string_slot <= openrac_guest_ram_size - 4u) {
                                std::memcpy(&item0_string, openrac_guest_base + string_slot, 4);
                                const std::uint32_t string_physical =
                                    openrac_guest_ram_address(item0_string);
                                if (item0_string != 0 && string_physical < openrac_guest_ram_size)
                                    item0_text_byte = openrac_guest_base[string_physical];
                            }
                        }
                    }
                    ui_visible_count += mode != 0 && first_text_byte != 0;
                    ui_font_count += font != 0;
                    if (ui_sample_count < 8 && (mode != 0 || first_text_byte != 0)) {
                        ui_samples[ui_sample_count++] = {
                            i, object, mode, font, frame, draw, font_vtable,
                            font_advance, font_height, font_image_a, font_image_b,
                            font_tex0, glyph_count, glyphs,
                            first_text_byte, menu_title, menu_items, menu_count,
                            item0_index, item0_string, item0_text_byte};
                    }
                }
                std::uint32_t active_world = 0;
                std::memcpy(&active_world, openrac_guest_base + 0x00275710u, 4);
                std::uint32_t font_count = 0, font_array = 0, default_font = 0;
                std::uint32_t font_ids[4]{};
                std::memcpy(&font_count, openrac_guest_base + 0x00262260u, 4);
                std::memcpy(&font_array, openrac_guest_base + 0x00262264u, 4);
                std::memcpy(&default_font, openrac_guest_base + 0x0026227Cu, 4);
                std::memcpy(font_ids, openrac_guest_base + 0x00262268u,
                            sizeof(font_ids));
                std::uint32_t game_mode = 0;
                std::uint32_t current_object = 0;
                std::uint32_t resource_manager = 0;
                std::uint32_t camera_object = 0;
                std::uint32_t camera_stack[4]{};
                std::uint32_t resource_level = 0;
                std::uint32_t resource_record = 0;
                std::uint32_t resource_visual_state[8]{};
                std::uint32_t movie_active_key = 0;
                std::uint32_t movie_pending_key = 0;
                std::int32_t resource_level_index = -1;
                std::int32_t resource_record_index = -1;
                std::int32_t resource_record_flags = 0;
                std::uint32_t menu_mode = 0;
                std::uint32_t world_lists[7]{};
                std::uint32_t world_job_queue[2]{};
                std::uint32_t frame_selector = 0;
                std::int32_t scene_gate = 0;
                std::uint32_t scene_pending = 0;
                std::int32_t scene_pending_state = -1;
                std::uint16_t frame_flags_a = 0;
                std::uint16_t frame_flags_b = 0;
                std::uint32_t pad_report = 0;
                std::uint32_t pad_mode = 0;
                std::uint32_t pad_state = 0;
                std::uint16_t pad_buttons = 0;
                std::uint16_t pad_pressed = 0;
                float frame_elapsed = 0.0f;
                std::memcpy(&game_mode, openrac_guest_base + 0x00264A58u,
                            sizeof(game_mode));
                std::memcpy(&current_object, openrac_guest_base + 0x00275710u,
                            sizeof(current_object));
                std::memcpy(&resource_manager, openrac_guest_base + 0x002623C0u,
                            sizeof(resource_manager));
                std::memcpy(&camera_object, openrac_guest_base + 0x00261990u,
                            sizeof(camera_object));
                const std::uint32_t camera_physical =
                    openrac_guest_ram_address(camera_object);
                if (camera_object != 0 && camera_physical <= openrac_guest_ram_size &&
                    0x3E0u + sizeof(camera_stack) <=
                        openrac_guest_ram_size - camera_physical) {
                    std::memcpy(&camera_stack[0], openrac_guest_base + camera_physical + 0x3D0u,
                                sizeof(camera_stack[0]));
                    std::memcpy(&camera_stack[1], openrac_guest_base + camera_physical + 0x220u,
                                sizeof(camera_stack[1]));
                    std::memcpy(&camera_stack[2], openrac_guest_base + camera_physical + 0x224u,
                                sizeof(camera_stack[2]));
                    std::memcpy(&camera_stack[3], openrac_guest_base + camera_physical + 0x3D8u,
                                sizeof(camera_stack[3]));
                }
                std::memcpy(&resource_level, openrac_guest_base + 0x002623C4u,
                            sizeof(resource_level));
                std::memcpy(&resource_record, openrac_guest_base + 0x002623C8u,
                            sizeof(resource_record));
                std::memcpy(resource_visual_state, openrac_guest_base + 0x00261EF4u,
                            sizeof(resource_visual_state));
                std::memcpy(&movie_active_key, openrac_guest_base + 0x00269A04u,
                            sizeof(movie_active_key));
                std::memcpy(&movie_pending_key, openrac_guest_base + 0x00269A08u,
                            sizeof(movie_pending_key));
                const std::uint32_t resource_manager_physical =
                    openrac_guest_ram_address(resource_manager);
                if (resource_manager != 0 && resource_manager_physical <= openrac_guest_ram_size &&
                    0x19E0u + sizeof(resource_level_index) <=
                        openrac_guest_ram_size - resource_manager_physical) {
                    std::memcpy(&resource_level_index,
                                openrac_guest_base + resource_manager_physical + 0x19D8u,
                                sizeof(resource_level_index));
                    std::memcpy(&resource_record_index,
                                openrac_guest_base + resource_manager_physical + 0x19DCu,
                                sizeof(resource_record_index));
                }
                const std::uint32_t resource_record_physical =
                    openrac_guest_ram_address(resource_record);
                if (resource_record != 0 && resource_record_physical <= openrac_guest_ram_size &&
                    sizeof(resource_record_flags) <=
                        openrac_guest_ram_size - resource_record_physical) {
                    std::memcpy(&resource_record_flags,
                                openrac_guest_base + resource_record_physical,
                                sizeof(resource_record_flags));
                }
                std::memcpy(&menu_mode, openrac_guest_base + 0x002701B8u,
                            sizeof(menu_mode));
                const std::uint32_t world_physical = openrac_guest_ram_address(current_object);
                if (current_object != 0 && world_physical <= openrac_guest_ram_size &&
                    0x1ED0u <=
                        openrac_guest_ram_size - world_physical) {
                    std::memcpy(&world_lists[0], openrac_guest_base + world_physical + 0x48u,
                                sizeof(world_lists[0]));
                    std::memcpy(&world_lists[1], openrac_guest_base + world_physical + 0x54u,
                                sizeof(world_lists[1]));
                    std::memcpy(&world_lists[2], openrac_guest_base + world_physical + 0x60u,
                                sizeof(world_lists[2]));
                    std::memcpy(&world_lists[3], openrac_guest_base + world_physical + 0x6Cu,
                                sizeof(world_lists[3]));
                    std::memcpy(&world_lists[4], openrac_guest_base + world_physical + 0x78u,
                                sizeof(world_lists[4]));
                    std::memcpy(&world_lists[5], openrac_guest_base + world_physical + 0x1B7Cu,
                                sizeof(world_lists[5]));
                    std::memcpy(&world_lists[6], openrac_guest_base + world_physical + 0x1C54u,
                                sizeof(world_lists[4]));
                    std::memcpy(world_job_queue, openrac_guest_base + world_physical + 0x1EC8u,
                                sizeof(world_job_queue));
                }
                std::memcpy(&frame_selector, openrac_guest_base + 0x00262C68u,
                            sizeof(frame_selector));
                std::memcpy(&scene_gate, openrac_guest_base + 0x00275F84u,
                            sizeof(scene_gate));
                std::memcpy(&frame_flags_a, openrac_guest_base + 0x00262D38u + 0xAAu,
                            sizeof(frame_flags_a));
                std::memcpy(&frame_flags_b, openrac_guest_base + 0x00262C70u + 0xAAu,
                            sizeof(frame_flags_b));
                std::memcpy(&pad_report, openrac_guest_base + 0x00262C70u + 0x0Cu,
                            sizeof(pad_report));
                std::memcpy(&pad_mode, openrac_guest_base + 0x00262C70u + 0x14u,
                            sizeof(pad_mode));
                std::memcpy(&pad_state, openrac_guest_base + 0x00262C70u + 0x18u,
                            sizeof(pad_state));
                std::memcpy(&pad_buttons, openrac_guest_base + 0x00262C70u + 0xA8u,
                            sizeof(pad_buttons));
                std::memcpy(&pad_pressed, openrac_guest_base + 0x00262C70u + 0xAAu,
                            sizeof(pad_pressed));
                std::memcpy(&frame_elapsed, openrac_guest_base + 0x0026185Cu,
                            sizeof(frame_elapsed));
                std::memcpy(&scene_pending, openrac_guest_base + 0x0026F808u + 0x260u,
                            sizeof(scene_pending));
                const std::uint32_t scene_pending_physical =
                    openrac_guest_ram_address(scene_pending);
                if (scene_pending != 0 && scene_pending_physical <= openrac_guest_ram_size &&
                    0x2D0u + sizeof(scene_pending_state) <=
                        openrac_guest_ram_size - scene_pending_physical) {
                    std::memcpy(&scene_pending_state,
                                openrac_guest_base + scene_pending_physical + 0x2D0u,
                                sizeof(scene_pending_state));
                }
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
                          << " sly_frame=" << frame_state[0] << ',' << frame_state[1]
                          << ',' << frame_state[2] << " frame_count=" << frame_state[11]
                          << " frame_top=0x" << std::hex << frame_top
                          << ",vtable=0x" << frame_top_vtable
                          << ",callback=0x" << frame_top_callback
                          << ",mode=0x" << frame_top_mode
                          << ",kind=0x" << frame_top_kind
                          << ",state=0x" << frame_top_state << std::dec
                          << std::hex << ",flags=0x" << frame_top_flags
                          << ",action=0x" << frame_top_action
                          << ",pending=0x" << frame_top_pending
                          << ",timer=0x" << frame_top_timer_bits
                          << ",global=0x" << global_state_ptr << ':'
                          << global_state[0] << ',' << global_state[1] << ','
                          << global_state[2] << ',' << global_state[3] << ','
                          << global_state[4] << ',' << global_state[5] << ','
                          << global_state[6] << ',' << global_state[7] << ','
                          << global_state[8] << ',' << global_state[9] << std::dec
                          << " game_mode=" << game_mode << " current_object=0x" << std::hex
                          << current_object << " menu=" << std::dec << menu_mode << ','
                          << menu_state[0] << ',' << menu_state[1] << ',' << menu_state[2]
                          << " menu_anim=" << menu_blend << ',' << menu_since << ','
                          << menu_clock << ':' << menu_clock_value
                          << " menu_data=" << std::hex << menu_font << ',' << menu_title
                          << ',' << menu_count << ',' << menu_items << ',' << menu_glyph_count
                          << ',' << menu_glyphs << std::dec
                          << " load_request=" << std::hex << pending_resource << std::dec
                          << " movie=" << movie_on << ',' << movie_status
                          << " resource_draw=" << resource_state_busy << ','
                          << resource_state_mode
                          << " overlay=" << overlay_state[0] << ',' << overlay_state[1]
                          << ',' << overlay_state[2] << " ui_objects=" << ui_active_count
                          << " camera=0x" << camera_object << " camera_stack="
                          << std::dec << camera_stack[0] << ",0x" << std::hex
                          << camera_stack[3] << " camera_flags=" << camera_stack[1]
                          << ',' << camera_stack[2]
                          << " resource_manager=0x" << resource_manager
                          << " resource_level=0x" << resource_level
                          << " resource_record=0x" << resource_record
                          << " resource_visual_state=";
                for (const std::uint32_t word : resource_visual_state)
                    std::cerr << std::hex << word << ',';
                std::cerr << std::dec
                          << " movie_keys=0x" << std::hex << movie_active_key << ",0x"
                          << movie_pending_key << std::dec
                          << " resource_level_index=" << std::dec << resource_level_index
                          << " resource_record_index=" << resource_record_index
                          << " resource_record_flags=0x" << std::hex
                          << resource_record_flags
                          << " world_lists=" << world_lists[0] << ',' << world_lists[1]
                          << ',' << world_lists[2] << ',' << world_lists[3] << ','
                          << world_lists[4] << ',' << world_lists[5] << ',' << world_lists[6]
                          << " world_job_queue=0x" << std::hex
                          << world_job_queue[0] << ",0x" << world_job_queue[1]
                          << std::dec << " map_selection=0x" << std::hex << scene_pending << std::dec
                          << " state_selector=0x" << std::hex << frame_selector << std::dec
                          << " pad_report=0x" << std::hex << pad_report << std::dec
                          << " pad_mode=" << pad_mode << " pad_state=" << pad_state
                          << " pad_buttons=0x" << std::hex << pad_buttons
                          << " pad_pressed=0x" << pad_pressed << std::dec
                          << " scene_gate=" << scene_gate << " pending_state="
                          << scene_pending_state << " state_flags=" << std::hex
                          << frame_flags_a << ',' << frame_flags_b << std::dec
                          << " elapsed=" << frame_elapsed
                          << " dispfb1=0x" << std::hex << dispfb1
                          << " display1=0x" << display1 << std::dec
                          << " vertices=" << std::dec << openrac_guest_gs_published_vertex_count()
                          << " image_tags=" << openrac_guest_gif_image_tag_count()
                          << " image_qwords=" << openrac_guest_gif_image_qword_count()
                          << " gif_chain_submits=" << openrac_native_renderer_dma_submit_count()
                          << " gif_chain_accepts=" << openrac_native_renderer_dma_accept_count()
                          << " gif_chain_rejects=" << openrac_native_renderer_dma_reject_count()
                          << " gif_chain_bytes=" << openrac_native_renderer_dma_bytes()
                          << " blit_writes=" << openrac_guest_gs_register_write_count(0x50u)
                          << " trxpos_writes=" << openrac_guest_gs_register_write_count(0x51u)
                          << " trxreg_writes=" << openrac_guest_gs_register_write_count(0x52u)
                          << " trxdir_writes=" << openrac_guest_gs_register_write_count(0x53u)
                          << std::dec;
                std::cerr << " active_threads=";
                for (uint32_t id = 0; id < 16; ++id) {
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
                std::cerr << "[ui trace] active_world=0x" << std::hex << active_world
                          << std::dec << " objects=" << ui_active_count
                          << " visible_text=" << ui_visible_count
                          << " fonts=" << ui_font_count << " font_registry="
                          << font_count << ",0x" << std::hex << font_array << ",0x"
                          << default_font << " ids=" << font_ids[0] << ','
                          << font_ids[1] << ',' << font_ids[2] << ',' << font_ids[3]
                          << std::dec << " samples=";
                for (std::uint32_t i = 0; i < ui_sample_count; ++i) {
                    const UiSample& sample = ui_samples[i];
                    if (i != 0) std::cerr << ';';
                    std::cerr << sample.index << ":0x" << std::hex << sample.object
                              << "/mode=" << sample.mode << "/font=" << sample.font
                              << "/frame=" << sample.frame << "/draw=" << sample.draw
                              << "/font_vt=" << sample.font_vtable << "/adv="
                              << sample.font_advance << "/height=" << sample.font_height
                              << "/image=" << sample.font_image_a << ','
                              << sample.font_image_b << "/tex0=" << sample.font_tex0
                              << "/glyphs=" << sample.glyph_count << "/glyph_ptr="
                              << sample.glyphs << "/text_byte=" << sample.first_text_byte
                              << "/menu=" << sample.menu_title << ','
                              << sample.menu_count << ",0x" << sample.menu_items
                              << "/item0=" << sample.item0_index << ",0x"
                              << sample.item0_string << "/item_byte="
                              << sample.item0_text_byte << std::dec;
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
    openrac::platform::FramePacer presenter(
        openrac::platform::field_rate(openrac::platform::VideoStandard::Ntsc));
    while (!game_started.load(std::memory_order_acquire)) {
        openrac_native_window_present();
        if (openrac_native_window_poll_input()) {
            game_thread.detach();
            openrac_native_window_destroy();
            trace_stop.store(true, std::memory_order_relaxed);
            if (trace_thread.joinable()) trace_thread.join();
            return 0;
        }
        presenter.wait();
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
