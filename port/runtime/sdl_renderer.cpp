// SPDX-License-Identifier: AGPL-3.0-or-later
// Sly's completed DMA packets feed OpenRAC's reusable GS direct renderer.
#include "openrac/native_window.h"
#include "openrac/native_gs.h"
#include "openrac/guest.h"

#include "../platform/window.h"
#include "../platform/sly_input.h"
#include "renderer/direct.h"
#include "renderer/framebuffer.h"
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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
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
std::atomic<unsigned> g_dma_reject_notices{0};

std::uint32_t guest_address(std::uint32_t address) {
    if (address >= GUEST_PORT && address - GUEST_PORT < GUEST_PORT_SIZE) return address;
#if defined(OPENRAC_SLY1_DIRECT_RETAIL_MEMORY)
    if (address >= 0x70000000u && address < 0x70004000u) return address;
    if (address >= 0x20000000u && address < 0x22000000u) return address - 0x20000000u;
    if (address >= 0x30000000u && address < 0x32000000u) return address - 0x30000000u;
    // The native C++ build keeps addressed locals on the Linux stack and in
    // its own static storage, rather than in a translated guest RAM arena.
    if ((address >= 0x08000000u && address < 0x10000000u) ||
        address >= 0xf0000000u) return address;
    return address & 0x01ffffffu;
#else
    return openrac_guest_ram_address(address);
#endif
}

bool guest_span(std::uint32_t address, std::size_t size, const std::uint8_t*& out) {
#if defined(OPENRAC_SLY1_DIRECT_RETAIL_MEMORY)
    const std::uint32_t physical = guest_address(address);
    if (size == 0 && physical == 0) {
        out = nullptr;
        return true;
    }
    const bool in_ram = physical >= 0x00010000u && physical <= 0x02000000u &&
        size <= static_cast<std::size_t>(0x02000000u - physical);
    const bool in_scratchpad = physical >= 0x70000000u && physical <= 0x70004000u &&
        size <= static_cast<std::size_t>(0x70004000u - physical);
    const bool in_port = physical >= GUEST_PORT && physical <= GUEST_PORT + GUEST_PORT_SIZE &&
        size <= static_cast<std::size_t>(GUEST_PORT + GUEST_PORT_SIZE - physical);
    const bool native_address =
        (physical >= 0x08000000u && physical < 0x10000000u) ||
        physical >= 0xf0000000u;
    const bool in_native_storage = in_port ||
        (native_address &&
         size <= static_cast<std::size_t>(0x100000000ull - physical));
    if (!in_ram && !in_scratchpad && !in_native_storage) return false;
    if (in_native_storage && size != 0) {
        const long page_size = ::sysconf(_SC_PAGESIZE);
        if (page_size <= 0) return false;
        const std::uintptr_t begin = static_cast<std::uintptr_t>(physical);
        const std::uintptr_t page_begin = begin - begin % static_cast<std::uintptr_t>(page_size);
        const std::uintptr_t end = begin + size;
        const std::size_t page_count =
            (end - page_begin + static_cast<std::uintptr_t>(page_size) - 1) /
            static_cast<std::uintptr_t>(page_size);
        std::vector<unsigned char> resident(page_count);
        if (::mincore(reinterpret_cast<void*>(page_begin), page_count *
                          static_cast<std::size_t>(page_size), resident.data()) != 0)
            return false;
    }
    out = reinterpret_cast<const std::uint8_t*>(physical);
    return true;
#else
    if (openrac_guest_base != nullptr && address >= GUEST_PORT &&
        address - GUEST_PORT <= GUEST_PORT_SIZE &&
        size <= static_cast<std::size_t>(GUEST_PORT_SIZE - (address - GUEST_PORT))) {
        out = openrac_guest_base + address;
        return true;
    }
    const std::uint32_t physical = guest_address(address);
    if (!openrac_guest_base || physical > openrac_guest_ram_size
        || size > static_cast<std::size_t>(openrac_guest_ram_size - physical)) return false;
    out = openrac_guest_base + physical;
    return true;
#endif
}

}

extern "C" void openrac_native_renderer_submit_dma_chain(std::uint32_t source_address) {
    constexpr std::size_t kMaxTags = 65536;
    constexpr std::size_t kMaxBytes = 16 * 1024 * 1024;
    std::vector<std::uint8_t> stream;
    std::array<std::uint32_t, 16> returns{};
    std::size_t return_count = 0;
    std::uint32_t tag_address = guest_address(source_address);
    std::uint32_t failed_tag_address = tag_address;
    std::uint64_t failed_tag_low = 0;
    std::uint32_t failed_payload_address = 0;
    std::size_t failed_payload_size = 0;
    std::size_t tags_consumed = 0;
    const char* failure = "tag limit";
    g_dma_submit_count.fetch_add(1, std::memory_order_relaxed);
    bool ok = false;
    for (std::size_t tags = 0; tags < kMaxTags; ++tags) {
        tags_consumed = tags + 1;
        const std::uint8_t* tag = nullptr;
        failed_tag_address = tag_address;
        if (!guest_span(tag_address, 16, tag)) { failure = "tag span"; break; }
        std::uint64_t low;
        std::memcpy(&low, tag, 8);
        failed_tag_low = low;
        const std::size_t qwc = low & 0xffffu;
        const unsigned id = (low >> 28) & 7u;
        const std::uint32_t inline_data = tag_address + 16;
        const std::uint32_t raw_reference = static_cast<std::uint32_t>(low >> 32);
        // Port pointers use bit 31, which overlaps the DMAtag SPR flag.
        const std::uint32_t referenced =
            raw_reference >= GUEST_PORT && raw_reference - GUEST_PORT < GUEST_PORT_SIZE
                ? raw_reference
                : guest_address(raw_reference & 0x7fffffffu);
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
        if (!valid) { failure = "call depth"; break; }
        const std::uint8_t* payload = nullptr;
        failed_payload_address = payload_address;
        failed_payload_size = payload_size;
        if (payload_size > kMaxBytes - stream.size()) {
            failure = "byte limit";
            break;
        }
        if (!guest_span(payload_address, payload_size, payload)) {
            failure = "payload span";
            break;
        }
        static const std::uint32_t trace_target = [] {
            const char* value = std::getenv("OPENRAC_TRACE_DMA_TARGET");
            if (value == nullptr || *value == '\0') return 0u;
            char* end = nullptr;
            const unsigned long parsed = std::strtoul(value, &end, 0);
            if (end == value || *end != '\0' || parsed > UINT32_MAX) return 0u;
            return static_cast<std::uint32_t>(parsed);
        }();
        if (trace_target != 0 && payload_size != 0 && trace_target >= payload_address &&
            trace_target - payload_address < payload_size) {
            const std::size_t target_offset = trace_target - payload_address;
            const std::uint8_t* target = payload + target_offset;
            const std::size_t preceding_size = std::min<std::size_t>(stream.size(), 80);
            std::array<char, 161> preceding{};
            for (std::size_t i = 0; i < preceding_size; ++i) {
                std::snprintf(preceding.data() + i * 2, preceding.size() - i * 2,
                              "%02x", stream[stream.size() - preceding_size + i]);
            }
            std::fprintf(stderr,
                         "[native dma target] chain=%#010x tag=%#010x id=%u qwc=%zu raw_ref=%#010x payload=%#010x bytes=%zu stream_offset=%zu target=%#010x target_offset=%zu bytes_at_target=%02x%02x%02x%02x preceding=%s\n",
                         source_address, tag_address, id, qwc, raw_reference,
                         payload_address, payload_size, stream.size(), trace_target,
                         target_offset, target[0], target_offset + 1 < payload_size ? target[1] : 0,
                         target_offset + 2 < payload_size ? target[2] : 0,
                         target_offset + 3 < payload_size ? target[3] : 0,
                         preceding.data());
        }
        if (payload_size >= 4096u && std::getenv("OPENRAC_TRACE_DMA_PAYLOAD") != nullptr) {
            static std::atomic<unsigned> payload_traces{0};
            const unsigned index = payload_traces.fetch_add(1, std::memory_order_relaxed);
            if (index < 64) {
                std::fprintf(stderr,
                             "[native dma payload] #%u tag=%#010x id=%u qwc=%zu raw_ref=%#010x source=%#010x bytes=%zu stream_offset=%zu first=%02x%02x%02x%02x\n",
                             index + 1, tag_address, id, qwc, raw_reference, payload_address,
                             payload_size, stream.size(), payload_size > 0 ? payload[0] : 0,
                             payload_size > 1 ? payload[1] : 0,
                             payload_size > 2 ? payload[2] : 0,
                             payload_size > 3 ? payload[3] : 0);
            }
        }
        if (payload_size != 0) stream.insert(stream.end(), payload, payload + payload_size);
        if (stop) {
            ok = true;
            break;
        }
        tag_address = next;
    }
    if (!ok) {
        g_dma_reject_count.fetch_add(1, std::memory_order_relaxed);
        const unsigned notice = g_dma_reject_notices.fetch_add(1, std::memory_order_relaxed);
        if (notice < 12) {
            const std::uint32_t first = stream.size() >= 4
                ? (static_cast<std::uint32_t>(stream[0]) |
                   (static_cast<std::uint32_t>(stream[1]) << 8) |
                   (static_cast<std::uint32_t>(stream[2]) << 16) |
                   (static_cast<std::uint32_t>(stream[3]) << 24))
                : 0;
            std::uint64_t adjacent = 0;
            std::uint64_t preceding = 0;
            const std::uint8_t* adjacent_bytes = nullptr;
            if (guest_span(failed_tag_address + 4, 8, adjacent_bytes)) {
                std::memcpy(&adjacent, adjacent_bytes, 8);
            }
            const std::uint8_t* preceding_bytes = nullptr;
            if (failed_tag_address >= 4 &&
                guest_span(failed_tag_address - 4, 8, preceding_bytes)) {
                std::memcpy(&preceding, preceding_bytes, 8);
            }
            openrac::log::warn(
                "native GIF chain rejected: source={:#010x} tag={:#010x} preceding={:#018x} low={:#018x} adjacent={:#018x} id={} qwc={} payload={:#010x}/{} reason={} bytes={} first_word={:#010x}",
                source_address, failed_tag_address, preceding, failed_tag_low,
                adjacent,
                static_cast<unsigned>((failed_tag_low >> 28) & 7u),
                static_cast<unsigned>(failed_tag_low & 0xffffu),
                failed_payload_address, failed_payload_size, failure, stream.size(), first);
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
    if (std::getenv("OPENRAC_TRACE_DMA_CALL") != nullptr) {
        static std::atomic<unsigned> chain_trace_count{0};
        const unsigned index = chain_trace_count.fetch_add(1, std::memory_order_relaxed);
        if (index < 64) {
            std::uint64_t first = 0;
            std::uint64_t second = 0;
            if (stream.size() >= sizeof(first))
                std::memcpy(&first, stream.data(), sizeof(first));
            if (stream.size() >= sizeof(first) + sizeof(second))
                std::memcpy(&second, stream.data() + sizeof(first), sizeof(second));
            std::fprintf(stderr,
                         "[native dma chain] #%u source=%#010x tags=%zu bytes=%zu first=%#018llx second=%#018llx\n",
                         index + 1, source_address, tags_consumed, stream.size(),
                         static_cast<unsigned long long>(first),
                         static_cast<unsigned long long>(second));
        }
    }
    g_dma_accept_count.fetch_add(1, std::memory_order_relaxed);
    g_dma_bytes.fetch_add(stream.size(), std::memory_order_relaxed);
    // Native GIF DMA completes synchronously. Latch the GS SIGNAL interrupt
    // before dispatching it to the game's registered callback.
    openrac_guest_gs_signal_interrupt();
}

extern "C" void openrac_native_renderer_trace_dma_call(std::uint32_t channel_address,
                                                          std::uint32_t source_address) {
    if (std::getenv("OPENRAC_TRACE_DMA_CALL") == nullptr) return;
    static std::atomic<unsigned> trace_count{0};
    const unsigned index = trace_count.fetch_add(1, std::memory_order_relaxed);
    if (index < 64) {
        std::fprintf(stderr, "[native dma call] #%u channel=%#010x source=%#010x\n",
                     index + 1, channel_address, source_address);
    }
    if (channel_address == 0x10008000u && source_address != 0) {
        static std::atomic<unsigned> vif0_trace_count{0};
        const unsigned vif0_index = vif0_trace_count.fetch_add(1, std::memory_order_relaxed);
        if (vif0_index < 8) {
            std::uint32_t tag_address = guest_address(source_address);
            for (unsigned tag_index = 0; tag_index < 8; ++tag_index) {
                const std::uint8_t* tag = nullptr;
                std::uint64_t low = 0;
                if (!guest_span(tag_address, 16, tag)) break;
                std::memcpy(&low, tag, sizeof(low));
                const std::uint32_t qwc = static_cast<std::uint32_t>(low & 0xFFFFu);
                const unsigned id = static_cast<unsigned>((low >> 28) & 7u);
                const std::uint32_t reference = static_cast<std::uint32_t>(
                    (low >> 32) & 0x7FFFFFFFu);
                std::fprintf(stderr,
                             "[native vif0 chain] #%u tag=%u address=%#010x id=%u qwc=%u reference=%#010x\n",
                             vif0_index + 1, tag_index, tag_address, id, qwc,
                             reference);
                if (tag_index == 0 && id == 1u) {
                    const std::uint8_t* payload = nullptr;
                    const std::size_t payload_size = static_cast<std::size_t>(qwc) * 16u;
                    if (guest_span(tag_address + 16u, payload_size, payload)) {
                        for (std::size_t word = 0; word < 16u && word * 4u < payload_size; ++word) {
                            std::uint32_t value = 0;
                            std::memcpy(&value, payload + word * 4u, sizeof(value));
                            std::fprintf(stderr, "[native vif0 data] #%u word=%zu value=%#010x\n",
                                         vif0_index + 1, word, value);
                        }
                    }
                }
                if (id == 1u) {
                    tag_address += 16u + qwc * 16u;
                } else if (id == 2u || id == 5u) {
                    tag_address = guest_address(reference);
                } else if (id == 3u || id == 4u) {
                    tag_address += 16u;
                } else {
                    break;
                }
            }
        }
    }
}

/* Receive the GIF stream after the native VIF1/VU1 path has expanded the
 * display list. The direct renderer consumes ordered GIF bytes at present. */
extern "C" void openrac_native_renderer_append_gif_stream(const void* source,
                                                           std::size_t size) {
    if (source == nullptr || size == 0) return;
    std::scoped_lock lock(g_packets_mutex);
    g_packets.insert(g_packets.end(), static_cast<const std::uint8_t*>(source),
                     static_cast<const std::uint8_t*>(source) + size);
}

extern "C" void openrac_native_renderer_submit_gif_stream(std::uint32_t source_address) {
    constexpr std::size_t kMaxBytes = 16 * 1024 * 1024;
    g_dma_submit_count.fetch_add(1, std::memory_order_relaxed);
    const char* failure = "unknown";
    const std::uint8_t* first_tag = nullptr;
    std::uint64_t first_lo = 0;
    std::uint64_t first_hi = 0;
    if (!guest_span(source_address, 16, first_tag)) {
        failure = "tag span";
    } else {
        std::memcpy(&first_lo, first_tag, 8);
        std::memcpy(&first_hi, first_tag + 8, 8);
    }
    const std::size_t qwc = static_cast<std::size_t>(first_lo & 0x7fffu) + 1;
    const std::size_t gif_size = qwc * 16;
    const std::uint8_t* gif = nullptr;
    if (failure == "unknown" && gif_size > kMaxBytes) failure = "byte limit";
    if (failure == "unknown" && !guest_span(source_address, gif_size, gif))
        failure = "packet span";
    if (failure != "unknown") {
        g_dma_reject_count.fetch_add(1, std::memory_order_relaxed);
        const unsigned notice = g_dma_reject_notices.fetch_add(1, std::memory_order_relaxed);
        if (notice < 12)
            openrac::log::warn("native GIF stream rejected: source={:#010x} tag={:#018x}/{:#018x} reason={}",
                               source_address, first_lo, first_hi, failure);
        return;
    }
    {
        std::scoped_lock lock(g_packets_mutex);
        if (gif_size > kMaxBytes - g_packets.size()) {
            g_dma_reject_count.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        g_packets.insert(g_packets.end(), gif, gif + gif_size);
    }
    g_dma_accept_count.fetch_add(1, std::memory_order_relaxed);
    g_dma_bytes.fetch_add(gif_size, std::memory_order_relaxed);
    openrac_guest_gs_signal_interrupt();
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
    static unsigned traced_key_events = 0;
    std::scoped_lock lock(g_input_mutex);
    while (SDL_PollEvent(&event)) {
        if (std::getenv("OPENRAC_TRACE_PAD") != nullptr &&
            (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) &&
            traced_key_events < 64u) {
            std::fprintf(stderr,
                         "[key] type=%s key=%#x scancode=%u repeat=%u focus=%p\n",
                         event.type == SDL_EVENT_KEY_DOWN ? "down" : "up",
                         static_cast<unsigned>(event.key.key),
                         static_cast<unsigned>(event.key.scancode),
                         event.key.repeat ? 1u : 0u,
                         static_cast<void*>(SDL_GetKeyboardFocus()));
            ++traced_key_events;
        }
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
    if (std::getenv("OPENRAC_TRACE_PAD") != nullptr) {
        static std::array<std::uint16_t, 2> previous_buttons{0xFFFFu, 0xFFFFu};
        static std::array<unsigned char, 2> previous_status{0xFFu, 0xFFu};
        const std::uint16_t buttons = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(report[2]) << 8u) |
            static_cast<std::uint16_t>(report[3]));
        if (report[0] != previous_status[static_cast<std::size_t>(port)] ||
            buttons != previous_buttons[static_cast<std::size_t>(port)]) {
            std::fprintf(stderr,
                         "[pad] port=%d status=%02x held=%04x axes=%02x,%02x,%02x,%02x\n",
                         port, report[0], static_cast<std::uint16_t>(~buttons), report[4],
                         report[5], report[6], report[7]);
            previous_status[static_cast<std::size_t>(port)] = report[0];
            previous_buttons[static_cast<std::size_t>(port)] = buttons;
        }
    }
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
    if (packets.size() >= 512u && packets.size() <= 16u * 1024u * 1024u &&
        std::getenv("OPENRAC_DUMP_NATIVE_PACKETS") != nullptr) {
        static unsigned dump_index = 0;
        if (dump_index < 16) {
            char path[80];
            std::snprintf(path, sizeof(path), "/tmp/sly1-native-render-packets-%02u.bin",
                          dump_index++);
            if (FILE* file = std::fopen(path, "wb")) {
                std::fwrite(packets.data(), 1, packets.size(), file);
                std::fclose(file);
                std::fprintf(stderr, "[native packet dump] bytes=%zu path=%s\n",
                             packets.size(), path);
            }
        }
    }
    if (!packets.empty() && g_direct && g_textures) {
        openrac::renderer::FrameInput input;
        input.packets[static_cast<std::size_t>(openrac::renderer::Bucket::Hud)] = packets;
        openrac::renderer::RenderState state{*g_textures, {0, width, height}};
        g_direct->render(input, state);
        g_textures->end_frame();
        if (std::getenv("OPENRAC_DUMP_NATIVE_FRAMES") != nullptr) {
            static unsigned frame_dump = 0;
            static unsigned frame_seen = 0;
            std::size_t minimum_bytes = 512;
            if (const char* setting = std::getenv("OPENRAC_DUMP_NATIVE_FRAME_MIN_BYTES")) {
                char* end = nullptr;
                const unsigned long parsed = std::strtoul(setting, &end, 10);
                if (end != setting && *end == '\0' && parsed <= 16u * 1024u * 1024u)
                    minimum_bytes = static_cast<std::size_t>(parsed);
            }
            unsigned skip = 0;
            if (const char* setting = std::getenv("OPENRAC_DUMP_NATIVE_FRAME_SKIP")) {
                char* end = nullptr;
                const unsigned long parsed = std::strtoul(setting, &end, 10);
                if (end != setting && *end == '\0' && parsed <= 100000u)
                    skip = static_cast<unsigned>(parsed);
            }
            if (packets.size() >= minimum_bytes && frame_seen++ >= skip && frame_dump < 8) {
                const auto pixels = openrac::renderer::read_rgba(0, width, height);
                char path[96];
                std::snprintf(path, sizeof(path), "/tmp/sly1-native-frame-%02u.ppm", frame_dump++);
                if (FILE* file = std::fopen(path, "wb")) {
                    std::fprintf(file, "P6 %d %d 255\n", width, height);
                    for (std::size_t i = 0; i + 3 < pixels.size(); i += 4) {
                        std::fwrite(pixels.data() + i, 1, 3, file);
                    }
                    std::fclose(file);
                    std::fprintf(stderr, "[native frame dump] path=%s\n", path);
                }
            }
        }
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
