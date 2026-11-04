// SPDX-License-Identifier: AGPL-3.0-or-later
// Sly's completed DMA packets feed OpenRAC's reusable GS direct renderer.
#include "openrac/native_window.h"
#include "openrac/native_gs.h"
#include "openrac/guest.h"

#include "../platform/window.h"
#include "renderer/direct.h"
#include "renderer/gl.h"
#include "renderer/renderer.h"
#include "renderer/texture_pool.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

// The standalone Sly window renders straight to framebuffer 0 without MSAA.
// OpenRAC's DirectRenderer uses this adapter only when a packet samples the
// current frame back into itself.
namespace openrac::renderer {
unsigned readable(const RenderTarget& target) { return target.framebuffer; }
}

namespace {
using ClearColor = void (*)(float, float, float, float);
using Clear = void (*)(unsigned int);
using Viewport = void (*)(int, int, int, int);

std::unique_ptr<openrac::platform::Platform> g_platform;
std::unique_ptr<openrac::platform::Window> g_window;
std::unique_ptr<openrac::renderer::TexturePool> g_textures;
std::unique_ptr<openrac::renderer::DirectRenderer> g_direct;
ClearColor g_clear_color = nullptr;
Clear g_clear = nullptr;
Viewport g_viewport = nullptr;
std::mutex g_packets_mutex;
std::vector<std::uint8_t> g_packets;

bool guest_span(std::uint32_t address, std::size_t size, const std::uint8_t*& out) {
    const std::uint32_t physical = openrac_guest_ram_address(address);
    if (!openrac_guest_base || physical > openrac_guest_ram_size
        || size > static_cast<std::size_t>(openrac_guest_ram_size - physical)) return false;
    out = openrac_guest_base + physical;
    return true;
}

bool is_direct_vif(std::span<const std::uint8_t> stream) {
    bool found_direct = false;
    std::size_t at = 0;
    while (at + 4 <= stream.size()) {
        const std::uint32_t word = static_cast<std::uint32_t>(stream[at])
            | (static_cast<std::uint32_t>(stream[at + 1]) << 8)
            | (static_cast<std::uint32_t>(stream[at + 2]) << 16)
            | (static_cast<std::uint32_t>(stream[at + 3]) << 24);
        const std::uint8_t cmd = static_cast<std::uint8_t>(word >> 24);
        const std::size_t immediate = word & 0xffffu;
        at += 4;
        std::size_t extra = 0;
        switch (cmd) {
        case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05:
        case 0x06: case 0x07: case 0x10: case 0x11: case 0x13: break;
        case 0x20: extra = 4; break;
        case 0x30: case 0x31: extra = 16; break;
        case 0x50: case 0x51:
            found_direct = true;
            extra = (immediate ? immediate : 65536u) * 16u;
            break;
        default: return false; // VU or unsupported operation: not a 2D display list.
        }
        if (extra > stream.size() - at) return false;
        at += extra;
    }
    return found_direct && at == stream.size();
}
}

extern "C" void openrac_native_renderer_submit_dma_chain(std::uint32_t source_address) {
    constexpr std::size_t kMaxTags = 65536;
    constexpr std::size_t kMaxBytes = 16 * 1024 * 1024;
    std::vector<std::uint8_t> stream;
    std::array<std::uint32_t, 16> returns{};
    std::size_t return_count = 0;
    std::uint32_t tag_address = openrac_guest_ram_address(source_address);
    bool ok = false;
    for (std::size_t tags = 0; tags < kMaxTags; ++tags) {
        const std::uint8_t* tag = nullptr;
        if (!guest_span(tag_address, 16, tag)) break;
        std::uint32_t lo, hi;
        std::memcpy(&lo, tag, 4);
        std::memcpy(&hi, tag + 4, 4);
        const std::size_t qwc = lo & 0xffffu;
        const unsigned id = (lo >> 28) & 7u;
        const std::uint32_t inline_data = tag_address + 16;
        const std::uint32_t referenced = openrac_guest_ram_address(hi & 0x7fffffffu);
        std::uint32_t payload_address = inline_data;
        std::uint32_t next = inline_data;
        const std::size_t payload_size = qwc * 16;
        bool stop = false;
        bool valid = true;
        switch (id) {
        case 0: // REFE
            payload_address = referenced;
            stop = true;
            break;
        case 1: // CNT
            next += static_cast<std::uint32_t>(payload_size);
            break;
        case 2: // NEXT
            next = referenced;
            break;
        case 3: // REF
        case 4: // REFS
            payload_address = referenced;
            break;
        case 5: // CALL
            if (return_count == returns.size()) { valid = false; break; }
            returns[return_count++] = inline_data + static_cast<std::uint32_t>(payload_size);
            next = referenced;
            break;
        case 6: // RET
            if (return_count == 0) stop = true;
            else next = returns[--return_count];
            break;
        case 7: // END
            stop = true;
            break;
        }
        if (!valid) break;
        const std::uint8_t* payload = nullptr;
        if (payload_size > kMaxBytes - stream.size()
            || !guest_span(payload_address, payload_size, payload)) break;
        stream.insert(stream.end(), payload, payload + payload_size);
        if (stop) {
            ok = true;
            break;
        }
        tag_address = next;
    }
    if (!ok || !is_direct_vif(stream)) return;
    std::scoped_lock lock(g_packets_mutex);
    if (stream.size() <= kMaxBytes - g_packets.size())
        g_packets.insert(g_packets.end(), stream.begin(), stream.end());
}

extern "C" int openrac_native_window_create(void) {
    openrac::platform::set_app_metadata("OpenThievius - Sly 1", "open-thievius-sly1");
    g_platform = std::make_unique<openrac::platform::Platform>(openrac::platform::kVideo);
    if (!g_platform->ok()) { g_platform.reset(); return 0; }
    openrac::platform::WindowConfig config;
    config.title = "OpenThievius - Sly 1";
    config.width = 960;
    config.height = 720;
    config.vsync = false;
    config.software = openrac::platform::SoftwareRenderer::Allow;
    std::string error;
    g_window = openrac::platform::Window::open(config, error);
    if (!g_window) { g_platform.reset(); return 0; }

    const auto load = openrac::platform::Window::gl_loader();
    if (!openrac::gl::load(load, error)) { g_window.reset(); g_platform.reset(); return 0; }
    g_clear_color = reinterpret_cast<ClearColor>(load("glClearColor"));
    g_clear = reinterpret_cast<Clear>(load("glClear"));
    g_viewport = reinterpret_cast<Viewport>(load("glViewport"));
    if (!g_clear_color || !g_clear || !g_viewport) { g_window.reset(); g_platform.reset(); return 0; }

    g_textures = std::make_unique<openrac::renderer::TexturePool>();
    openrac::renderer::RenderState state{*g_textures, {0, 960, 720}};
    g_direct = std::make_unique<openrac::renderer::DirectRenderer>(
        "sly1-direct", openrac::renderer::Bucket::Hud,
        openrac::renderer::DirectConfig{512, 448, 0},
        openrac::renderer::DirectRenderer::Input::Vif);
    if (!g_direct->init(state, error)) {
        g_direct.reset(); g_textures.reset(); g_window.reset(); g_platform.reset(); return 0;
    }
    return 1;
}

extern "C" int openrac_native_window_poll_quit(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) return 1;
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) return 1;
    }
    return 0;
}

extern "C" void openrac_native_window_present(void) {
    if (!g_window) return;
    int width = 0, height = 0;
    g_window->drawable_size(width, height);
    g_viewport(0, 0, width, height);
    g_clear_color(0, 0, 0, 1);
    g_clear(0x00004000u);
    std::vector<std::uint8_t> packets;
    { std::scoped_lock lock(g_packets_mutex); packets.swap(g_packets); }
    if (!packets.empty() && g_direct && g_textures) {
        openrac::renderer::FrameInput input;
        input.packets[static_cast<std::size_t>(openrac::renderer::Bucket::Hud)] = packets;
        openrac::renderer::RenderState state{*g_textures, {0, width, height}};
        g_direct->render(input, state);
        g_textures->end_frame();
    }
    g_window->swap();
}

extern "C" void openrac_native_window_destroy(void) {
    if (g_direct) g_direct->release();
    if (g_textures) g_textures->release_gl();
    g_direct.reset(); g_textures.reset(); g_window.reset(); g_platform.reset();
    g_clear_color = nullptr; g_clear = nullptr; g_viewport = nullptr;
    std::scoped_lock lock(g_packets_mutex); g_packets.clear();
}
