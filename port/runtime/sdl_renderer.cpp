// SPDX-License-Identifier: AGPL-3.0-or-later
// Sly's completed DMA packets feed OpenRAC's reusable GS direct renderer.
#include "openrac/native_window.h"
#include "openrac/native_gs.h"
#include "openrac/guest.h"

#include "../platform/window.h"
#include "../platform/sly_input.h"
#include "renderer/direct.h"
#include "renderer/gl.h"
#include "renderer/renderer.h"
#include "renderer/texture_pool.h"
#include "common/log.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
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
std::unique_ptr<sly1::platform::Input> g_input;
std::unique_ptr<openrac::platform::Window> g_window;
std::unique_ptr<openrac::renderer::TexturePool> g_textures;
std::unique_ptr<openrac::renderer::DirectRenderer> g_direct;
ClearColor g_clear_color = nullptr;
Clear g_clear = nullptr;
Viewport g_viewport = nullptr;
std::mutex g_packets_mutex;
std::mutex g_input_mutex;
std::vector<std::uint8_t> g_packets;
std::atomic<std::uint64_t> g_dma_submit_count{0};
std::atomic<std::uint64_t> g_dma_accept_count{0};
std::atomic<std::uint64_t> g_dma_reject_count{0};
std::atomic<std::uint64_t> g_dma_bytes{0};
std::atomic_flag g_dma_reject_notice = ATOMIC_FLAG_INIT;

bool guest_span(std::uint32_t address, std::size_t size, const std::uint8_t*& out) {
    const std::uint32_t physical = openrac_guest_ram_address(address);
    if (!openrac_guest_base || physical > openrac_guest_ram_size
        || size > static_cast<std::size_t>(openrac_guest_ram_size - physical)) return false;
    out = openrac_guest_base + physical;
    return true;
}

}

extern "C" void openrac_native_renderer_submit_dma_chain(std::uint32_t source_address) {
    constexpr std::size_t kMaxTags = 65536;
    constexpr std::size_t kMaxBytes = 16 * 1024 * 1024;
    std::vector<std::uint8_t> stream;
    std::array<std::uint32_t, 16> returns{};
    std::size_t return_count = 0;
    std::uint32_t tag_address = openrac_guest_ram_address(source_address);
    g_dma_submit_count.fetch_add(1, std::memory_order_relaxed);
    bool ok = false;
    for (std::size_t tags = 0; tags < kMaxTags; ++tags) {
        const std::uint8_t* tag = nullptr;
        if (!guest_span(tag_address, 16, tag)) break;
        std::uint64_t low;
        std::memcpy(&low, tag, 8);
        const std::size_t qwc = low & 0xffffu;
        const unsigned id = (low >> 28) & 7u;
        const std::uint32_t inline_data = tag_address + 16;
        const std::uint32_t referenced = openrac_guest_ram_address(
            static_cast<std::uint32_t>(low >> 32) & 0x7fffffffu);
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
    if (!ok) {
        g_dma_reject_count.fetch_add(1, std::memory_order_relaxed);
        if (!g_dma_reject_notice.test_and_set(std::memory_order_relaxed)) {
            const std::uint32_t first = stream.size() >= 4
                ? (static_cast<std::uint32_t>(stream[0]) |
                   (static_cast<std::uint32_t>(stream[1]) << 8) |
                   (static_cast<std::uint32_t>(stream[2]) << 16) |
                   (static_cast<std::uint32_t>(stream[3]) << 24))
                : 0;
            openrac::log::warn(
                "native GIF chain rejected: source={:#010x} chain_ok={} bytes={} first_word={:#010x}",
                source_address, ok, stream.size(), first);
        }
        return;
    }
    {
        std::scoped_lock lock(g_packets_mutex);
        if (stream.size() > kMaxBytes - g_packets.size()) {
            g_dma_reject_count.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        g_packets.insert(g_packets.end(), stream.begin(), stream.end());
    }
    g_dma_accept_count.fetch_add(1, std::memory_order_relaxed);
    g_dma_bytes.fetch_add(stream.size(), std::memory_order_relaxed);
}

extern "C" std::uint64_t openrac_native_renderer_dma_submit_count(void) {
    return g_dma_submit_count.load(std::memory_order_relaxed);
}

extern "C" std::uint64_t openrac_native_renderer_dma_accept_count(void) {
    return g_dma_accept_count.load(std::memory_order_relaxed);
}

extern "C" std::uint64_t openrac_native_renderer_dma_reject_count(void) {
    return g_dma_reject_count.load(std::memory_order_relaxed);
}

extern "C" std::uint64_t openrac_native_renderer_dma_bytes(void) {
    return g_dma_bytes.load(std::memory_order_relaxed);
}

extern "C" int openrac_native_window_create(void) {
    openrac::platform::set_app_metadata("OpenThievius - Sly 1", "open-thievius-sly1");
    g_platform = std::make_unique<openrac::platform::Platform>(
        openrac::platform::kVideo | openrac::platform::kGamepad);
    if (!g_platform->ok()) { g_platform.reset(); return 0; }
    g_input = std::make_unique<sly1::platform::Input>();
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
        openrac::renderer::DirectRenderer::Input::Gif);
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

extern "C" int openrac_native_window_poll_input(void) {
    if (!g_input) return 1;
    SDL_Event event;
    bool quit = false;
    std::scoped_lock lock(g_input_mutex);
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT ||
            (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) quit = true;
        g_input->handle_event(event);
    }
    g_input->update();
    return quit ? 1 : 0;
}

extern "C" int openrac_native_pad_read(int port, unsigned char* report, unsigned int size) {
    std::scoped_lock lock(g_input_mutex);
    if (!g_input || report == nullptr || port < 0 || port >= 2 || size < 32) return 0;
    std::span<unsigned char, 32> output(report, 32);
    g_input->pad(port).write_read_buffer(output);
    return 1;
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
    g_direct.reset(); g_textures.reset(); g_window.reset();
    { std::scoped_lock lock(g_input_mutex); g_input.reset(); }
    g_platform.reset();
    g_clear_color = nullptr; g_clear = nullptr; g_viewport = nullptr;
    std::scoped_lock lock(g_packets_mutex); g_packets.clear();
}
