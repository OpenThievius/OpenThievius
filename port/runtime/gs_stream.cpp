/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Sly's native renderer input: stateful decoding of GIF packet framing. */
#include "openrac/native_gs.h"
#include "openrac/guest.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

namespace {
struct GifParser {
    bool tag_loaded = false;
    uint32_t format = 0;
    uint32_t loops = 0;
    uint32_t register_count = 0;
    uint32_t register_index = 0;
    uint32_t remaining_qwords = 0;
    uint64_t tag_low = 0;
    uint64_t tag_registers = 0;
    uint64_t end_of_packet_count = 0;
    uint64_t image_tag_count = 0;
    uint64_t image_qword_count = 0;
    std::array<uint64_t, 0x80> register_write_counts{};
    uint64_t current_uv = 0;
    uint32_t current_rgba = 0xFFFFFFFFu;
    std::array<openrac_gs_vertex, 3> primitive_vertices{};
    uint32_t primitive_vertex_count = 0;
    uint32_t strip_vertex_count = 0;
    openrac_gs_vertex strip_first{};
    openrac_gs_vertex strip_previous{};
    openrac_gs_vertex fan_first{};
    openrac_gs_vertex fan_previous{};
    std::array<uint64_t, 0x80> gs_registers{};
};

std::mutex g_parser_mutex;
GifParser g_parser;
std::vector<openrac_gs_vertex> g_current_frame;
std::vector<openrac_gs_vertex> g_published_frame;

void append_vertex(const openrac_gs_vertex& vertex) {
    constexpr size_t max_frame_vertices = 1u << 20;
    if (g_current_frame.size() < max_frame_vertices)
        g_current_frame.push_back(vertex);
}

void append_triangle(const openrac_gs_vertex& a, const openrac_gs_vertex& b,
                     const openrac_gs_vertex& c) {
    append_vertex(a);
    append_vertex(b);
    append_vertex(c);
}

openrac_gs_vertex make_vertex(uint64_t value) {
    const uint64_t offset = g_parser.gs_registers[0x18u];
    const int32_t x = static_cast<int32_t>(value & 0xFFFFu) -
                      static_cast<int32_t>(offset & 0xFFFFu);
    const int32_t y = static_cast<int32_t>((value >> 16) & 0xFFFFu) -
                      static_cast<int32_t>((offset >> 32) & 0xFFFFu);
    return {static_cast<float>(x) / 16.0f, static_cast<float>(y) / 16.0f,
            g_parser.current_rgba};
}

void submit_vertex(uint64_t value) {
    const openrac_gs_vertex vertex = make_vertex(value);
    const uint32_t primitive = static_cast<uint32_t>(g_parser.gs_registers[0] & 7u);
    switch (primitive) {
    case 0: // point: represent it as a 1-pixel native quad
        append_triangle(vertex, {vertex.x + 1.0f, vertex.y, vertex.rgba},
                        {vertex.x, vertex.y + 1.0f, vertex.rgba});
        append_triangle({vertex.x + 1.0f, vertex.y, vertex.rgba},
                        {vertex.x + 1.0f, vertex.y + 1.0f, vertex.rgba},
                        {vertex.x, vertex.y + 1.0f, vertex.rgba});
        break;
    case 1: // line: make a one-pixel-wide quad
        if (g_parser.primitive_vertex_count == 0) {
            g_parser.primitive_vertices[0] = vertex;
            g_parser.primitive_vertex_count = 1;
        } else {
            const auto a = g_parser.primitive_vertices[0];
            const auto b = vertex;
            const float dx = b.x - a.x, dy = b.y - a.y;
            const float length = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
            const float ox = -dy / length * 0.5f, oy = dx / length * 0.5f;
            append_triangle({a.x + ox, a.y + oy, a.rgba},
                            {b.x + ox, b.y + oy, b.rgba},
                            {b.x - ox, b.y - oy, b.rgba});
            append_triangle({a.x + ox, a.y + oy, a.rgba},
                            {b.x - ox, b.y - oy, b.rgba},
                            {a.x - ox, a.y - oy, a.rgba});
            g_parser.primitive_vertices[0] = b;
        }
        break;
    case 2: // line strip
        if (g_parser.primitive_vertex_count == 0) {
            g_parser.primitive_vertices[0] = vertex;
            g_parser.primitive_vertex_count = 1;
        } else {
            const auto previous = g_parser.primitive_vertices[0];
            g_parser.primitive_vertex_count = 0;
            const uint64_t saved_prim = g_parser.gs_registers[0];
            g_parser.gs_registers[0] = 1;
            g_parser.primitive_vertices[0] = previous;
            g_parser.primitive_vertex_count = 1;
            submit_vertex(value);
            g_parser.gs_registers[0] = saved_prim;
        }
        break;
    case 3: // triangle list
        g_parser.primitive_vertices[g_parser.primitive_vertex_count++] = vertex;
        if (g_parser.primitive_vertex_count == 3) {
            append_triangle(g_parser.primitive_vertices[0], g_parser.primitive_vertices[1],
                            g_parser.primitive_vertices[2]);
            g_parser.primitive_vertex_count = 0;
        }
        break;
    case 4: // triangle strip
        if (g_parser.strip_vertex_count == 0) g_parser.strip_first = vertex;
        else if (g_parser.strip_vertex_count == 1) g_parser.strip_previous = vertex;
        else if ((g_parser.strip_vertex_count & 1u) == 0)
            append_triangle(g_parser.strip_first, g_parser.strip_previous, vertex);
        else
            append_triangle(g_parser.strip_previous, g_parser.strip_first, vertex);
        g_parser.strip_previous = vertex;
        ++g_parser.strip_vertex_count;
        break;
    case 5: // triangle fan
        if (g_parser.primitive_vertex_count == 0) {
            g_parser.primitive_vertices[0] = vertex;
            g_parser.primitive_vertex_count = 1;
        } else if (g_parser.primitive_vertex_count == 1) {
            g_parser.primitive_vertices[1] = vertex;
            g_parser.primitive_vertex_count = 2;
        } else {
            append_triangle(g_parser.primitive_vertices[0], g_parser.primitive_vertices[1], vertex);
            g_parser.primitive_vertices[1] = vertex;
        }
        break;
    case 6: // sprite: two opposite corners become two native triangles
        if (g_parser.primitive_vertex_count == 0) {
            g_parser.primitive_vertices[0] = vertex;
            g_parser.primitive_vertex_count = 1;
        } else {
            const auto a = g_parser.primitive_vertices[0];
            const auto b = vertex;
            const openrac_gs_vertex top_right{b.x, a.y, b.rgba};
            const openrac_gs_vertex bottom_left{a.x, b.y, a.rgba};
            append_triangle(a, top_right, b);
            append_triangle(a, b, bottom_left);
            g_parser.primitive_vertex_count = 0;
        }
        break;
    default:
        break;
    }
}

void write_gs_register(uint32_t address, uint64_t value) {
    if (address >= g_parser.gs_registers.size()) return;
    ++g_parser.register_write_counts[address];
    g_parser.gs_registers[address] = value;
    if (address == 0x00u) {
        g_parser.primitive_vertex_count = 0;
        g_parser.strip_vertex_count = 0;
    } else if (address == 0x01u) {
        g_parser.current_rgba = static_cast<uint32_t>(value);
    } else if (address == 0x03u) {
        g_parser.current_uv = value;
    } else if (address == 0x04u || address == 0x05u) {
        submit_vertex(value);
    }
}

void finish_tag() {
    if (((g_parser.tag_low >> 15) & 1u) != 0)
        ++g_parser.end_of_packet_count;
    g_parser.tag_loaded = false;
}

void begin_tag(uint64_t low, uint64_t high) {
    g_parser.tag_low = low;
    g_parser.tag_registers = high;
    g_parser.loops = static_cast<uint32_t>(low & 0x7FFFu);
    g_parser.format = static_cast<uint32_t>((low >> 58) & 3u);
    const uint32_t encoded_register_count = static_cast<uint32_t>((low >> 60) & 0xFu);
    g_parser.register_count = encoded_register_count == 0 ? 16u : encoded_register_count;
    g_parser.register_index = 0;
    if (((low >> 46) & 1u) != 0)
        write_gs_register(0x00u, (low >> 47) & 0x7FFu);

    switch (g_parser.format) {
    case 0:
        g_parser.remaining_qwords = g_parser.loops * g_parser.register_count;
        break;
    case 1:
        g_parser.remaining_qwords =
            (g_parser.loops * g_parser.register_count + 1u) / 2u;
        break;
    case 2:
    case 3:
        ++g_parser.image_tag_count;
        g_parser.image_qword_count += g_parser.loops;
        g_parser.remaining_qwords = g_parser.loops;
        break;
    default:
        g_parser.remaining_qwords = 0;
        break;
    }
    g_parser.tag_loaded = true;
    if (g_parser.remaining_qwords == 0) finish_tag();
}

void write_gif_register(uint32_t sequence_index, uint64_t value, uint64_t meta) {
    const uint32_t descriptor = static_cast<uint32_t>(
        (g_parser.tag_registers >> ((sequence_index % g_parser.register_count) * 4u)) & 0xFu);
    if (descriptor == 0xEu) {
        write_gs_register(static_cast<uint32_t>(meta & 0xFFu), value);
    } else if (g_parser.format == 0 && (descriptor == 0x04u || descriptor == 0x05u)) {
        // PACKED XYZ uses a 128-bit layout: Y starts at bit 32, while the
        // register-list form uses the GS register's native 64-bit layout.
        uint64_t native_value = value & 0xFFFFu;
        native_value |= ((value >> 32u) & 0xFFFFu) << 16u;
        const bool xyzf = descriptor == 0x04u;
        const uint64_t z = xyzf ? ((meta >> 4u) & 0xFFFFFFu) : (meta & 0xFFFFFFFFu);
        native_value |= z << 32u;
        if (xyzf) native_value |= ((meta >> 36u) & 0xFFu) << 56u;
        const bool disable_drawing = ((meta >> 47u) & 1u) != 0;
        write_gs_register(descriptor + (disable_drawing ? 8u : 0u), native_value);
    } else if (descriptor != 0xFu) {
        write_gs_register(descriptor, value);
    }
}
}

extern "C" void openrac_guest_gif_stream_write(const void* source, size_t size) {
    if (source == nullptr || size < 16u) return;
    const auto* bytes = static_cast<const uint8_t*>(source);
    std::lock_guard lock(g_parser_mutex);
    size_t cursor = 0;
    while (cursor + 16u <= size) {
        if (!g_parser.tag_loaded) {
            uint64_t low = 0, high = 0;
            std::memcpy(&low, bytes + cursor, sizeof(low));
            std::memcpy(&high, bytes + cursor + 8u, sizeof(high));
            begin_tag(low, high);
            cursor += 16u;
            continue;
        }

        const uint8_t* qword = bytes + cursor;
        if (g_parser.format == 0) {
            uint64_t value = 0, meta = 0;
            std::memcpy(&value, qword, sizeof(value));
            std::memcpy(&meta, qword + 8u, sizeof(meta));
            write_gif_register(g_parser.register_index, value, meta);
            ++g_parser.register_index;
        } else if (g_parser.format == 1) {
            const uint32_t total_registers = g_parser.loops * g_parser.register_count;
            for (uint32_t half = 0; half < 2; ++half) {
                if (g_parser.register_index >= total_registers) break;
                uint64_t value = 0;
                std::memcpy(&value, qword + half * sizeof(value), sizeof(value));
                write_gif_register(g_parser.register_index++, value, 0);
            }
        }

        ++cursor;
        cursor += 15u;
        if (g_parser.remaining_qwords > 0) --g_parser.remaining_qwords;
        if (g_parser.remaining_qwords == 0) finish_tag();
    }
}

extern "C" int openrac_guest_gs_read_register(uint32_t address, uint64_t* value) {
    if (value == nullptr || address >= g_parser.gs_registers.size()) return 0;
    std::lock_guard lock(g_parser_mutex);
    *value = g_parser.gs_registers[address];
    return 1;
}

extern "C" uint64_t openrac_guest_gs_register_write_count(uint32_t address) {
    if (address >= g_parser.register_write_counts.size()) return 0;
    std::lock_guard lock(g_parser_mutex);
    return g_parser.register_write_counts[address];
}

extern "C" uint64_t openrac_guest_gif_eop_count(void) {
    std::lock_guard lock(g_parser_mutex);
    return g_parser.end_of_packet_count;
}

extern "C" uint64_t openrac_guest_gif_image_tag_count(void) {
    std::lock_guard lock(g_parser_mutex);
    return g_parser.image_tag_count;
}

extern "C" uint64_t openrac_guest_gif_image_qword_count(void) {
    std::lock_guard lock(g_parser_mutex);
    return g_parser.image_qword_count;
}

extern "C" void openrac_guest_gs_publish_frame(void) {
    std::lock_guard lock(g_parser_mutex);
    g_published_frame.swap(g_current_frame);
    g_current_frame.clear();
    g_parser.primitive_vertex_count = 0;
    g_parser.strip_vertex_count = 0;
}

extern "C" size_t openrac_guest_gs_read_vertices(openrac_gs_vertex* destination,
                                                   size_t capacity) {
    if (destination == nullptr || capacity == 0) return 0;
    std::lock_guard lock(g_parser_mutex);
    const size_t count = std::min(capacity, g_published_frame.size());
    std::memcpy(destination, g_published_frame.data(), count * sizeof(*destination));
    return count;
}

extern "C" size_t openrac_guest_gs_published_vertex_count(void) {
    std::lock_guard lock(g_parser_mutex);
    return g_published_frame.size();
}

extern "C" int openrac_guest_gs_read_privileged_register(uint32_t offset,
                                                            uint64_t* value) {
    if (openrac_guest_base == nullptr || value == nullptr || offset > GUEST_GS_REGS_SIZE - 8u)
        return 0;
    std::memcpy(value, openrac_guest_base + GUEST_GS_REGS + offset, sizeof(*value));
    return 1;
}
