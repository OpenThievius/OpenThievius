/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Sly's native renderer input: stateful decoding of GIF packet framing. */
#include "openrac/native_gs.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <mutex>

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
    std::array<uint64_t, 0x80> gs_registers{};
};

std::mutex g_parser_mutex;
GifParser g_parser;

void write_gs_register(uint32_t address, uint64_t value) {
    if (address < g_parser.gs_registers.size())
        g_parser.gs_registers[address] = value;
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

extern "C" uint64_t openrac_guest_gif_eop_count(void) {
    std::lock_guard lock(g_parser_mutex);
    return g_parser.end_of_packet_count;
}
