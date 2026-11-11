// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only implementation of the worker-start callback at retail address 0x0018E5E8.
#include "openrac/guest.h"
#include "openrac/ee.h"
#include "openrac/disc_image.h"
#include "openrac/native_gs.h"
#include "openrac/native_window.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <time.h>

namespace {

constexpr gaddr kNativeTempBase = GUEST_PORT + 0x00400000u;
constexpr gaddr kNativeTempSize = 0x00010000u;
constexpr uint32_t kNativeTempThreadSlots = 128;
constexpr uint32_t kNativeTempMarkSlots = 64;

struct NativeTempArena {
    gaddr cursor = 0;
    gaddr marks[kNativeTempMarkSlots]{};
    uint32_t depth = 0;
};

thread_local NativeTempArena g_native_temp_arena;
thread_local int g_current_lz_chunk = -1;
std::atomic<uint64_t> g_model_trace_sequence{0};

uint64_t next_model_trace_sequence() {
    return g_model_trace_sequence.fetch_add(1, std::memory_order_relaxed) + 1u;
}

gaddr native_temp_floor() {
    const uint32_t thread_id = openrac_guest_current_thread_id();
    if (thread_id >= kNativeTempThreadSlots) return 0;
    return kNativeTempBase + thread_id * kNativeTempSize;
}

gaddr native_temp_top(gaddr floor) {
    return floor + kNativeTempSize;
}

}  // namespace

extern "C" uint64_t game_sly1_native_monotonic_ticks(void) {
    // The retail frame loop is paced by PS2 display interrupts. The native
    // loop has no interrupt source, so pace this sole frame-clock caller at
    // 60 Hz instead of letting the game clock run at host CPU speed.
    struct timespec deadline{};
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_nsec += 16666667L;
    if (deadline.tv_nsec >= 1000000000L) {
        ++deadline.tv_sec;
        deadline.tv_nsec -= 1000000000L;
    }
    while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, nullptr) == EINTR) {
    }

    struct timespec now{};
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0;
    constexpr uint64_t ticks_per_second = 294912000u;
    return static_cast<uint64_t>(now.tv_sec) * ticks_per_second +
           static_cast<uint64_t>(now.tv_nsec) * ticks_per_second / 1000000000u;
}

extern "C" int game_sly1_native_pad_read(uint32_t port, uint32_t report,
                                           uint32_t size) {
    if (report == 0) return 0;
    return openrac_native_pad_read(static_cast<int>(port),
                                   reinterpret_cast<unsigned char*>(G(report)), size);
}

extern "C" uint32_t game_sly1_native_vu0_mac(void) {
    return OPENRAC_VU0->mac;
}

extern "C" uint32_t game_sly1_native_vu0_component(uint32_t index) {
    float value = 0.0f;
    if (index < 128u) {
        value = OPENRAC_VU0->vf[index / 4u][index % 4u];
    } else if (index < 132u) {
        value = OPENRAC_VU0->acc[index - 128u];
    }
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

extern "C" void game_sly1_native_trace_model_reader(uint32_t model,
                                                      uint32_t reader,
                                                      uint32_t owner) {
    const bool dump_entry = std::getenv("OPENRAC_DUMP_MODEL_ENTRY") != nullptr;
    const bool trace_fields = std::getenv("OPENRAC_TRACE_MODEL_FIELDS") != nullptr;
    if ((!dump_entry && !trace_fields) || openrac_guest_base == nullptr) return;
    static unsigned dumps = 0;
    if (dumps >= (dump_entry ? 32u : 256u)) return;
    if (reader > openrac_guest_ram_size - 0x24u) return;
    if (reader != 0x0064c4c0u) return;
    const uint32_t cursor = GREF(uint32_t, reader + 0x18u);
    const int32_t remaining = static_cast<int32_t>(GREF(uint32_t, reader + 0x1Cu));
    const uint32_t model_offset = openrac_guest_ram_address(model);
    const uint32_t owner_offset = openrac_guest_ram_address(owner);
    if (cursor < GUEST_SCRATCHPAD || cursor >= GUEST_SCRATCHPAD + 0x2000u ||
        model_offset >= openrac_guest_ram_size || owner_offset >= openrac_guest_ram_size)
        return;
    if (trace_fields) {
        std::fprintf(stderr,
            "[model-reader] seq=%llu thread=%u model=%#010x owner=%#010x "
            "reader=%#010x cursor=%#010x remaining=%d\n",
            static_cast<unsigned long long>(next_model_trace_sequence()),
            openrac_guest_current_thread_id(), model, owner, reader, cursor, remaining);
    }
    if (!dump_entry) return;
    char path[80];
    std::snprintf(path, sizeof(path), "/tmp/sly1-model-entry-ring-%02u.bin", dumps);
    if (FILE* file = std::fopen(path, "wb")) {
        std::fwrite(openrac_guest_base + GUEST_SCRATCHPAD, 1, 0x2000u, file);
        std::fclose(file);
    }
    std::fprintf(stderr,
        "[model-entry-dump] model=%#010x owner=%#010x reader=%#010x "
        "cursor=%#010x remaining=%d ring=%s bytes=",
        model, owner, reader, cursor, remaining, path);
    const uint32_t input_offset = cursor >= GUEST_SCRATCHPAD &&
                                  cursor < GUEST_SCRATCHPAD + GUEST_SCRATCHPAD_SIZE
        ? cursor : openrac_guest_ram_address(cursor);
    if (remaining > 0 && input_offset >= GUEST_SCRATCHPAD &&
        input_offset < GUEST_SCRATCHPAD + GUEST_SCRATCHPAD_SIZE) {
        const uint32_t bytes = std::min<uint32_t>(32u, static_cast<uint32_t>(remaining));
        for (uint32_t i = 0; i < bytes &&
             input_offset + i < GUEST_SCRATCHPAD + GUEST_SCRATCHPAD_SIZE; ++i)
            std::fprintf(stderr, "%02x", openrac_guest_base[input_offset + i]);
    }
    std::fputc('\n', stderr);
    ++dumps;
}

namespace {
void trace_model_read(const char* kind, gaddr reader, uint32_t width) {
    if (std::getenv("OPENRAC_TRACE_MODEL_FIELDS") == nullptr ||
        openrac_guest_base == nullptr || reader != 0x0064c4c0u) return;
    if (reader > openrac_guest_ram_size - 0x20u) return;
    const uint32_t cursor = GREF(uint32_t, reader + 0x18u);
    const int32_t remaining = static_cast<int32_t>(GREF(uint32_t, reader + 0x1Cu));
    uint32_t offset = cursor;
    const bool scratchpad = cursor >= GUEST_SCRATCHPAD &&
                            cursor < GUEST_SCRATCHPAD + GUEST_SCRATCHPAD_SIZE;
    if (!scratchpad)
        offset = openrac_guest_ram_address(cursor);
    if ((scratchpad && offset >= GUEST_SCRATCHPAD + GUEST_SCRATCHPAD_SIZE) ||
        (!scratchpad && offset >= openrac_guest_ram_size)) return;
    const uint32_t available = scratchpad
        ? GUEST_SCRATCHPAD + GUEST_SCRATCHPAD_SIZE - offset
        : openrac_guest_ram_size - offset;
    const uint32_t bytes = std::min<uint32_t>(available,
        static_cast<uint32_t>(std::max(remaining, 0)));
    uint32_t value = 0;
    for (uint32_t i = 0; i < std::min<uint32_t>(width, std::min<uint32_t>(bytes, 4u)); ++i)
        value |= static_cast<uint32_t>(openrac_guest_base[offset + i]) << (8u * i);
    if (value < 0x1000u) return;
    std::fprintf(stderr, "[model-field] seq=%llu thread=%u kind=%s reader=%#010x cursor=%#010x remaining=%d width=%u value=",
                 static_cast<unsigned long long>(next_model_trace_sequence()),
                 openrac_guest_current_thread_id(), kind, reader, cursor, remaining, width);
    std::fprintf(stderr, "%#x", value);
    std::fprintf(stderr, " next=");
    for (uint32_t i = 0; i < std::min<uint32_t>(width, std::min<uint32_t>(bytes, 8u)); ++i)
        std::fprintf(stderr, "%02x", openrac_guest_base[offset + i]);
    std::fputc('\n', stderr);
}
}

extern "C" void game_sly1_native_trace_model_read_u16(uint32_t reader) {
    trace_model_read("u16", reader, 2u);
}
extern "C" void game_sly1_native_trace_model_read_s16(uint32_t reader) {
    trace_model_read("s16", reader, 2u);
}
extern "C" void game_sly1_native_trace_model_strip_count(uint32_t reader,
                                                           uint32_t frame,
                                                           uint32_t scratch_index) {
    if (std::getenv("OPENRAC_TRACE_MODEL_FIELDS") == nullptr ||
        openrac_guest_base == nullptr || reader != 0x0064c4c0u ||
        reader > openrac_guest_ram_size - 0x20u) return;
    const uint32_t cursor = GREF(uint32_t, reader + 0x18u);
    const bool scratchpad = cursor >= GUEST_SCRATCHPAD &&
                            cursor < GUEST_SCRATCHPAD + GUEST_SCRATCHPAD_SIZE;
    const uint32_t offset = scratchpad ? cursor : openrac_guest_ram_address(cursor);
    if ((scratchpad && offset > GUEST_SCRATCHPAD + GUEST_SCRATCHPAD_SIZE - 2u) ||
        (!scratchpad && (offset >= openrac_guest_ram_size ||
                         offset > openrac_guest_ram_size - 2u))) return;
    const uint32_t value = openrac_guest_base[offset] |
                           (static_cast<uint32_t>(openrac_guest_base[offset + 1u]) << 8u);
    if (frame > openrac_guest_ram_size - 336u) return;
    const uint32_t part_index = GREF(uint32_t, frame + 332u);
    if (value < 0x1000u && (part_index < 500u || part_index > 520u)) return;
    std::fprintf(stderr,
        "[model-strip-count] seq=%llu thread=%u part=%u scratch=%u frame=%#010x reader=%#010x cursor=%#010x remaining=%d value=%u (%#06x)\n",
        static_cast<unsigned long long>(next_model_trace_sequence()),
        openrac_guest_current_thread_id(), part_index, scratch_index, frame, reader, cursor,
        static_cast<int32_t>(GREF(uint32_t, reader + 0x1Cu)), value, value);
    char path[96];
    std::snprintf(path, sizeof(path), "/tmp/sly1-model-strip-part-%u.bin", part_index);
    if (FILE* file = std::fopen(path, "wb")) {
        std::fwrite(openrac_guest_base + GUEST_SCRATCHPAD, 1,
                    GUEST_SCRATCHPAD_SIZE, file);
        std::fclose(file);
        std::fprintf(stderr, "[model-strip-count] ring=%s\n", path);
    }
}
extern "C" void game_sly1_native_trace_model_part(uint32_t reader, uint32_t part_index) {
    const bool near_first_window = part_index >= 500u && part_index <= 520u;
    const bool near_fault_window = part_index >= 1088u && part_index <= 1100u;
    if (std::getenv("OPENRAC_TRACE_MODEL_FIELDS") == nullptr ||
        openrac_guest_base == nullptr || reader != 0x0064c4c0u ||
        (!near_first_window && !near_fault_window) ||
        reader > openrac_guest_ram_size - 0x20u) return;
    const uint32_t cursor = GREF(uint32_t, reader + 0x18u);
    const bool scratchpad = cursor >= GUEST_SCRATCHPAD &&
                            cursor < GUEST_SCRATCHPAD + GUEST_SCRATCHPAD_SIZE;
    const uint32_t offset = scratchpad ? cursor : openrac_guest_ram_address(cursor);
    if ((scratchpad && offset > GUEST_SCRATCHPAD + GUEST_SCRATCHPAD_SIZE - 2u) ||
        (!scratchpad && (offset >= openrac_guest_ram_size ||
                         offset > openrac_guest_ram_size - 2u))) return;
    const uint32_t flags = openrac_guest_base[offset] |
                           (static_cast<uint32_t>(openrac_guest_base[offset + 1u]) << 8u);
    std::fprintf(stderr,
        "[model-part] seq=%llu thread=%u index=%u reader=%#010x cursor=%#010x remaining=%d flags=%#06x next=",
        static_cast<unsigned long long>(next_model_trace_sequence()),
        openrac_guest_current_thread_id(), part_index, reader, cursor,
        static_cast<int32_t>(GREF(uint32_t, reader + 0x1Cu)), flags);
    for (uint32_t i = 0; i < 16u && offset + i < GUEST_SCRATCHPAD + GUEST_SCRATCHPAD_SIZE; ++i)
        std::fprintf(stderr, "%s%02x", i == 0 ? "" : " ", openrac_guest_base[offset + i]);
    std::fputc('\n', stderr);
}

extern "C" void game_sly1_native_trace_lz_entry(uint32_t reader) {
    if (std::getenv("OPENRAC_TRACE_MODEL_FIELDS") == nullptr ||
        openrac_guest_base == nullptr || reader != 0x0064c4c0u ||
        reader > openrac_guest_ram_size - 0x48u) return;
    static std::atomic<unsigned> entry_counter{0};
    const unsigned entry = entry_counter.fetch_add(1, std::memory_order_relaxed);
    if (entry >= 2048u) return;
    g_current_lz_chunk = static_cast<int>(entry);
    char path[96];
    std::snprintf(path, sizeof(path), "/tmp/sly1-lz-entry-%04u.bin", entry);
    if (FILE* file = std::fopen(path, "wb")) {
        std::fwrite(openrac_guest_base + GUEST_SCRATCHPAD, 1,
                    GUEST_SCRATCHPAD_SIZE, file);
        std::fclose(file);
    }
    const uint32_t input_cursor = GREF(uint32_t, reader + 0x20u);
    const uint32_t input_offset = openrac_guest_ram_address(input_cursor);
    if (entry == 228u && input_offset < openrac_guest_ram_size) {
        char input_path[96];
        std::snprintf(input_path, sizeof(input_path), "/tmp/sly1-lz-input-%04u.bin", entry);
        const uint32_t available = std::min<uint32_t>(
            GREF(uint32_t, reader + 0x24u), openrac_guest_ram_size - input_offset);
        if (FILE* file = std::fopen(input_path, "wb")) {
            std::fwrite(openrac_guest_base + input_offset, 1, available, file);
            std::fclose(file);
        }
        std::fprintf(stderr, "[lz-input-capture] chunk=%u address=%#010x bytes=%u file=%s\n",
                     entry, input_cursor, available, input_path);
    }
    std::fprintf(stderr,
        "[lz-entry] chunk=%u thread=%u reader=%#010x input=%#010x input_remaining=%d "
        "output=%#010x output_remaining=%d dictionary=%d flags=%#010x input_bytes=",
        entry, openrac_guest_current_thread_id(), reader,
        input_cursor,
        static_cast<int32_t>(GREF(uint32_t, reader + 0x24u)),
        GREF(uint32_t, reader + 0x18u),
        static_cast<int32_t>(GREF(uint32_t, reader + 0x1Cu)),
        static_cast<int32_t>(GREF(uint32_t, reader + 0x2Cu)),
        GREF(uint32_t, reader + 0x28u));
    if (input_offset < openrac_guest_ram_size) {
        const uint32_t input_bytes = std::min<uint32_t>(16u,
            openrac_guest_ram_size - input_offset);
        for (uint32_t i = 0; i < input_bytes; ++i)
            std::fprintf(stderr, "%s%02x", i == 0 ? "" : " ",
                         openrac_guest_base[input_offset + i]);
    }
    std::fprintf(stderr, " file=%s\n", path);
}

extern "C" void game_sly1_native_trace_lz_token(uint32_t reader,
                                                   uint32_t output_position,
                                                   uint32_t input_cursor,
                                                   uint32_t input_remaining,
                                                   uint32_t flag_word) {
    if (std::getenv("OPENRAC_TRACE_MODEL_FIELDS") == nullptr ||
        openrac_guest_base == nullptr || reader != 0x0064c4c0u ||
        g_current_lz_chunk != 228 || output_position > 128u) return;
    const uint32_t start = output_position > 12u ? output_position - 12u : 0u;
    std::fprintf(stderr,
        "[lz-token] chunk=%d thread=%u out=%u input=%#010x remaining=%u flags=%#010x bytes=",
        g_current_lz_chunk, openrac_guest_current_thread_id(), output_position,
        input_cursor, input_remaining, flag_word);
    for (uint32_t i = start; i < output_position; ++i)
        std::fprintf(stderr, "%s%02x", i == start ? "" : " ",
                     openrac_guest_base[GUEST_SCRATCHPAD + i]);
    std::fputc('\n', stderr);
}

extern "C" void game_sly1_native_trace_lz_branch(uint32_t reader,
                                                    uint32_t input_cursor,
                                                    uint32_t first_byte,
                                                    uint32_t flag_word,
                                                    uint32_t output_position) {
    if (std::getenv("OPENRAC_TRACE_MODEL_FIELDS") == nullptr ||
        openrac_guest_base == nullptr || reader != 0x0064c4c0u ||
        g_current_lz_chunk != 228 || output_position > 60u) return;
    const uint32_t input_offset = openrac_guest_ram_address(input_cursor);
    const uint32_t second_byte = input_offset < openrac_guest_ram_size
        ? openrac_guest_base[input_offset] : 0u;
    std::fprintf(stderr,
        "[lz-branch] out=%u input=%#010x flag=%u word=%#06x loaded=%#04x next=%#04x\n",
        output_position, input_cursor, flag_word & 1u, flag_word,
        first_byte, second_byte);
}

extern "C" void game_sly1_native_trace_lz_output(uint32_t reader,
                                                   uint32_t output_size,
                                                   uint32_t input_cursor,
                                                   uint32_t input_remaining) {
    if (std::getenv("OPENRAC_TRACE_MODEL_FIELDS") == nullptr ||
        openrac_guest_base == nullptr || reader != 0x0064c4c0u || output_size == 0)
        return;
    static std::atomic<unsigned> chunk_counter{0};
    const unsigned chunk = chunk_counter.fetch_add(1, std::memory_order_relaxed);
    if (chunk >= 2048u) return;
    const uint32_t bytes = std::min<uint32_t>(output_size, 0x2000u);
    char path[96];
    std::snprintf(path, sizeof(path), "/tmp/sly1-lz-output-%04u.bin", chunk);
    if (FILE* file = std::fopen(path, "wb")) {
        std::fwrite(openrac_guest_base + GUEST_SCRATCHPAD, 1, bytes, file);
        std::fclose(file);
    }
    std::fprintf(stderr,
        "[lz-output] chunk=%u thread=%u reader=%#010x size=%u input=%#010x input_remaining=%u file=%s\n",
        chunk, openrac_guest_current_thread_id(), reader, bytes, input_cursor,
        input_remaining, path);
}

extern "C" void game_sly1_native_trace_frame_lists(uint32_t context) {
    if (std::getenv("OPENRAC_TRACE_FRAME_LISTS") == nullptr ||
        openrac_guest_base == nullptr) return;
    const uint32_t offset = openrac_guest_ram_address(context);
    if (offset > openrac_guest_ram_size - 0x50u) return;
    const uint32_t active = GREF(uint32_t, 0x0027b314u);
    const bool active_in_ram = active <= openrac_guest_ram_size - 16u;
    const bool active_in_port = active >= GUEST_PORT &&
        active <= GUEST_PORT + GUEST_PORT_SIZE - 16u;
    std::fprintf(stderr,
        "[frame-lists] thread=%u context=%#010x active=%#010x\n",
        openrac_guest_current_thread_id(), context, active);
    uint32_t frame = active;
    for (uint32_t depth = 0; depth < 4u; ++depth) {
        const bool frame_in_ram = frame <= openrac_guest_ram_size - 16u;
        const bool frame_in_port = frame >= GUEST_PORT &&
            frame <= GUEST_PORT + GUEST_PORT_SIZE - 16u;
        if (!frame_in_ram && !frame_in_port) {
            if (depth == 0) std::fprintf(stderr, "[list-frame] invalid=%#010x\n", frame);
            break;
        }
        const uint32_t list = GREF(uint32_t, frame);
        const uint32_t link = GREF(uint32_t, frame + 4u);
        const int32_t frame_offset = GREF(int32_t, frame + 8u);
        const uint32_t previous = GREF(uint32_t, frame + 12u);
        std::fprintf(stderr,
            "[list-frame] depth=%u frame=%#010x list=%#010x link=%#010x offset=%d previous=%#010x\n",
            depth, frame, list, link, frame_offset, previous);
        frame = previous;
    }
    for (uint32_t i = 0; i < 3u; ++i) {
        const uint32_t list = context + 0x20u + i * 0x0Cu;
        const uint32_t list_offset = openrac_guest_ram_address(list);
        if (list_offset > openrac_guest_ram_size - 12u) continue;
        const uint32_t head = GREF(uint32_t, list);
        const uint32_t tail = GREF(uint32_t, list + 4u);
        const int32_t node_offset = GREF(int32_t, list + 8u);
        uint32_t head_next = 0, head_prev = 0, tail_next = 0, tail_prev = 0;
        if (head != 0) {
            const uint32_t head_offset = openrac_guest_ram_address(head +
                static_cast<uint32_t>(std::max(node_offset, 0)));
            if (node_offset >= 0 && head_offset <= openrac_guest_ram_size - 8u) {
                head_next = GREF(uint32_t, head + static_cast<uint32_t>(node_offset));
                head_prev = GREF(uint32_t, head + static_cast<uint32_t>(node_offset) + 4u);
            }
        }
        if (tail != 0 && node_offset >= 0) {
            const uint32_t tail_links = openrac_guest_ram_address(tail +
                static_cast<uint32_t>(node_offset));
            if (tail_links <= openrac_guest_ram_size - 8u) {
                tail_next = GREF(uint32_t, tail + static_cast<uint32_t>(node_offset));
                tail_prev = GREF(uint32_t, tail + static_cast<uint32_t>(node_offset) + 4u);
            }
        }
        std::fprintf(stderr,
            "[frame-lists] slot=%u list=%#010x head=%#010x tail=%#010x offset=%d head-links={%#010x,%#010x} tail-links={%#010x,%#010x}\n",
            i, list, head, tail, node_offset, head_next, head_prev,
            tail_next, tail_prev);
    }
}

extern "C" void game_sly1_native_trace_list_remove(uint32_t list,
                                                       uint32_t item,
                                                       int32_t offset,
                                                       uint32_t next,
                                                       uint32_t previous,
                                                       uint32_t phase) {
    if (std::getenv("OPENRAC_TRACE_FRAME_LISTS") == nullptr ||
        openrac_guest_base == nullptr) return;
    constexpr uint32_t target_link = 0x00830060u;
    if (item + static_cast<uint32_t>(offset) != target_link &&
        (next == 0 || next + static_cast<uint32_t>(offset) != target_link) &&
        (previous == 0 || previous + static_cast<uint32_t>(offset) != target_link))
        return;
    static std::atomic<unsigned> calls{0};
    const unsigned call = calls.fetch_add(1, std::memory_order_relaxed);
    if (call >= 512u) return;
    std::fprintf(stderr,
        "[frame-list-remove] call=%u thread=%u caller=%#010x list=%#010x item=%#010x offset=%d phase=%u next=%#010x prev=%#010x\n",
        call, openrac_guest_current_thread_id(),
        openrac_guest_last_called_address_for_thread(openrac_guest_current_thread_id()),
        list, item, offset, phase, next, previous);
}

extern "C" void game_sly1_native_trace_list_append(uint32_t list,
                                                       uint32_t node,
                                                       int32_t offset,
                                                       uint32_t head,
                                                       uint32_t tail,
                                                       uint32_t phase) {
    if (std::getenv("OPENRAC_TRACE_FRAME_LISTS") == nullptr ||
        openrac_guest_base == nullptr) return;
    constexpr uint32_t target_link = 0x00830060u;
    const bool node_hits = node + static_cast<uint32_t>(offset) == target_link;
    bool tail_hits = tail != 0 && tail + static_cast<uint32_t>(offset) == target_link;
    if (!tail_hits && phase != 0) {
        const uint32_t node_links = node + static_cast<uint32_t>(offset);
        const uint32_t node_link_offset = openrac_guest_ram_address(node_links);
        if (node_link_offset <= openrac_guest_ram_size - 8u) {
            const uint32_t previous = GREF(uint32_t, node_links + 4u);
            tail_hits = previous != 0 &&
                previous + static_cast<uint32_t>(offset) == target_link;
        }
    }
    if (!node_hits && !tail_hits) return;
    static std::atomic<unsigned> calls{0};
    const unsigned call = calls.fetch_add(1, std::memory_order_relaxed);
    if (call >= 256u) return;
    const uint32_t link_offset = openrac_guest_ram_address(target_link);
    uint32_t next = 0, previous = 0;
    if (link_offset <= openrac_guest_ram_size - 8u) {
        next = GREF(uint32_t, target_link);
        previous = GREF(uint32_t, target_link + 4u);
    }
    std::fprintf(stderr,
        "[frame-list-append] call=%u thread=%u caller=%#010x phase=%u list=%#010x node=%#010x offset=%d head=%#010x tail=%#010x target={%u,%u} links={%#010x,%#010x}\n",
        call, openrac_guest_current_thread_id(),
        openrac_guest_last_called_address_for_thread(openrac_guest_current_thread_id()),
        phase, list, node, offset, head, tail, node_hits ? 1u : 0u,
        tail_hits ? 1u : 0u, next, previous);
}

extern "C" void sly1_native_start_worker(void) {
    const gaddr saved_sp = openrac_guest_sp;
    const gaddr params = openrac_guest_frame(0x48u);
    std::memset(G(params), 0, 0x48u);

    GREF(uint32_t, params + 4u) = 1;
    const uint32_t semaphore = static_cast<uint32_t>(openrac_guest_kernel_syscall(
        0x40u, params, 0, 0, 0, 0, 0, 0, 0));
    GREF(uint32_t, 0x00279B30u) = semaphore;
    GREF(uint32_t, 0x00279A0Cu) = 0;

    const gaddr thread_params = params + 0x18u;
    GREF(uint32_t, thread_params + 0x04u) = 0x0018E558u;
    GREF(uint32_t, thread_params + 0x08u) = 0x002C2B00u;
    GREF(uint32_t, thread_params + 0x0Cu) = 0x00001000u;
    GREF(uint32_t, thread_params + 0x10u) = 0x002832F0u;
    GREF(uint32_t, thread_params + 0x14u) = 2u;
    const uint32_t thread = static_cast<uint32_t>(openrac_guest_kernel_syscall(
        0x20u, thread_params, 0, 0, 0, 0, 0, 0, 0));
    GREF(uint32_t, 0x00279A0Cu) = thread;
    (void)openrac_guest_kernel_syscall(0x22u, thread, 0, 0, 0, 0, 0, 0, 0);
    openrac_guest_sp = saved_sp;
}

extern "C" int game_sly1_native_display_kernel_call(uint32_t service, uint32_t a0,
                                                      uint32_t a1, uint32_t a2) {
    return static_cast<int>(openrac_guest_kernel_syscall(service, a0, a1, a2, 0, 0, 0, 0, 0));
}

extern "C" int game_sly1_native_gs_privileged_store64(uint32_t address,
                                                        uint64_t value) {
    return openrac_guest_gs_privileged_store64(address, value);
}

// Object constructors in retail descriptors are guest code addresses. Route
// them through the host-generated C function map so the native factory never
// casts a 32-bit guest address into a host function pointer.
extern "C" void game_sly1_native_call_constructor(uint32_t address, uint32_t object) {
    const uint64_t integer_registers[8] = {object};
    const float float_registers[8]{};
    float float_result = 0.0f;
    (void)openrac_guest_call_regs(address, integer_registers, float_registers,
                                  &float_result, 0);
}

extern "C" void game_sly1_native_call_method(uint32_t address, uint32_t object) {
    const uint64_t integer_registers[8] = {object};
    const float float_registers[8]{};
    float float_result = 0.0f;
    (void)openrac_guest_call_regs(address, integer_registers, float_registers,
                                  &float_result, 0);
}

extern "C" uint32_t game_sly1_native_call_regs4(uint32_t address, uint32_t a0,
                                                  uint32_t a1, uint32_t a2,
                                                  uint32_t a3) {
    if (address == 0 || openrac_guest_function_name(address) == nullptr) return 0;
    if (std::getenv("OPENRAC_TRACE_BAD_THUNK") != nullptr) {
        static thread_local uint32_t last_address;
        static thread_local unsigned trace_count;
        if (address != last_address && trace_count < 4096) {
            const auto target = openrac_guest_function(address);
            std::fprintf(stderr,
                         "[native callback] guest=%#010x host=%p name=%s args=%#010x,%#010x,%#010x,%#010x\n",
                         address, reinterpret_cast<void*>(target),
                         openrac_guest_function_name(address), a0, a1, a2, a3);
            ++trace_count;
            if (trace_count == 4096)
                std::fprintf(stderr, "[native callback] transition trace limit reached\n");
        }
        last_address = address;
    }
    if (std::getenv("OPENRAC_TRACE_UI_CALLS") != nullptr &&
        (address == 0x00194F80u || address == 0x001AB198u ||
         address == 0x001AA790u || address == 0x0015E318u)) {
        std::fprintf(stderr,
                     "[native ui call] address=%#010x args=%#010x,%#010x,%#010x,%#010x\n",
                     address, a0, a1, a2, a3);
    }
    const uint64_t integer_registers[8] = {a0, a1, a2, a3};
    float float_registers[8]{};
    const uint32_t arguments[4] = {a0, a1, a2, a3};
    for (unsigned i = 0; i < 4; ++i) {
        std::memcpy(&float_registers[i], &arguments[i], sizeof(float));
    }
    float float_result = 0.0f;
    return static_cast<uint32_t>(openrac_guest_call_regs(
        address, integer_registers, float_registers, &float_result, 0));
}

extern "C" float game_sly1_native_call_f32(uint32_t address, uint32_t a0,
                                              uint32_t a1, uint32_t a2,
                                              uint32_t a3, float f12, float f14) {
    if (address == 0 || openrac_guest_function_name(address) == nullptr) return 0.0f;
    const uint64_t integer_registers[8] = {a0, a1, a2, a3};
    const float float_registers[8] = {f12, f14};
    float result = 0.0f;
    (void)openrac_guest_call_regs(address, integer_registers, float_registers,
                                  &result, 1);
    return result;
}

extern "C" int game_sly1_native_read_sectors(uint32_t sector, uint32_t count,
                                              uint32_t destination) {
    const int success = openrac_guest_disc_read(sector, count, destination);
    if (std::getenv("OPENRAC_TRACE_CD") != nullptr) {
        std::fprintf(stderr,
                     "[info] native Sly resource read sector=%u count=%u destination=%#010x result=%d\n",
                     sector, count, destination, success);
    }
    return success;
}

extern "C" void game_sly1_native_temp_begin(void) {
    const gaddr floor = native_temp_floor();
    if (floor == 0 || g_native_temp_arena.depth >= kNativeTempMarkSlots) return;
    if (g_native_temp_arena.cursor < floor ||
        g_native_temp_arena.cursor > native_temp_top(floor)) {
        g_native_temp_arena.cursor = native_temp_top(floor);
    }
    g_native_temp_arena.marks[g_native_temp_arena.depth++] = g_native_temp_arena.cursor;
}

extern "C" uint32_t game_sly1_native_temp_alloc(int32_t size) {
    const gaddr floor = native_temp_floor();
    if (floor == 0 || size <= 0) return 0;
    if (g_native_temp_arena.cursor < floor ||
        g_native_temp_arena.cursor > native_temp_top(floor)) {
        g_native_temp_arena.cursor = native_temp_top(floor);
    }
    const uint32_t aligned = (static_cast<uint32_t>(size) + 15u) & ~15u;
    if (aligned > g_native_temp_arena.cursor - floor) return 0;
    g_native_temp_arena.cursor -= aligned;
    return g_native_temp_arena.cursor;
}

extern "C" void game_sly1_native_temp_end(void) {
    if (g_native_temp_arena.depth == 0) return;
    g_native_temp_arena.cursor = g_native_temp_arena.marks[--g_native_temp_arena.depth];
}

extern "C" void game_sly1_native_loader_stage(uint32_t stage, uint32_t object,
                                               uint32_t reader) {
    static bool direct_level_dump_active = false;
    static unsigned direct_level_window_index = 0;
    if (std::getenv("OPENRAC_TRACE_LOADER") == nullptr) return;
    if (stage == 150u && object == 0x0100u &&
        std::getenv("OPENRAC_DUMP_OBJECT_WINDOWS") != nullptr)
        direct_level_dump_active = true;
    if (stage == 160u) {
        static unsigned heap_failures_logged = 0;
        if (heap_failures_logged++ >= 4u) return;
        std::fprintf(stderr,
                     "[heap] exhausted request=%u current=%#010x limit=%#010x\n",
                     object, reader, GREF(uint32_t, 0x002699A8u));
        for (int depth = 0; depth < 8; ++depth) {
            char caller[256];
            openrac_host_caller(depth, caller, sizeof(caller));
            std::fprintf(stderr, "[heap] stack[%d]=%s\n", depth, caller);
        }
        return;
    }
    if (stage == 163u) {
        static unsigned large_allocations_logged = 0;
        if (large_allocations_logged++ >= 12u) return;
        std::fprintf(stderr,
                     "[heap] large request=%u front=%#010x back=%#010x mark=%#010x\n",
                     object, reader, GREF(uint32_t, 0x002c2a24u),
                     GREF(uint32_t, 0x002c2a18u));
        for (int depth = 1; depth < 5; ++depth) {
            char caller[256];
            openrac_host_caller(depth, caller, sizeof(caller));
            std::fprintf(stderr, "[heap] large stack[%d]=%s\n", depth, caller);
        }
        return;
    }
    if (stage == 171u) {
        int32_t size = 0;
        switch (object) {
        case 1u: case 2u: size = 0x4C; break;
        case 3u: case 4u: size = 0x38; break;
        case 5u: size = 0x78; break;
        case 6u: size = 0x44; break;
        case 7u: size = 0x3C; break;
        case 8u: size = 0x50; break;
        default: break;
        }
        std::fprintf(stderr,
                     "[animator] type=%u reader=%#010x cursor=%#010x remaining=%d size=%d\n",
                     object, reader, GREF(uint32_t, reader + 0x18u),
                     static_cast<int32_t>(GREF(uint32_t, reader + 0x1Cu)),
                     size);
        return;
    }
    if (stage == 172u) {
        std::fprintf(stderr,
                     "[animator] methods=%#010x object=%#010x load=%#010x bind=%#010x\n",
                     object, reader, object != 0 ? GREF(uint32_t, object) : 0u,
                     object != 0 ? GREF(uint32_t, object + 4u) : 0u);
        return;
    }
    if (stage == 173u) {
        std::fprintf(stderr,
                     "[loader-group] index=%u value_count=%u reader=%#010x cursor=%#010x remaining=%d\n",
                     object & 0xFFFFu, object >> 16, reader,
                     GREF(uint32_t, reader + 0x18u),
                     static_cast<int32_t>(GREF(uint32_t, reader + 0x1Cu)));
        return;
    }
    if (stage == 154u) {
        uint32_t pending[16]{};
        unsigned pending_count = 0;
        pending[pending_count++] = object;
        std::fprintf(stderr, "[resource-value] root=%#010x cursor=%#010x",
                     object, GREF(uint32_t, reader + 0x18u));
        for (unsigned depth = 0; pending_count != 0 && depth < 8u; ++depth) {
            const uint32_t node = pending[--pending_count];
            if (node == 0) continue;
            const uint32_t type = GREF(uint32_t, node);
            const uint32_t payload = GREF(uint32_t, node + 4u);
            const uint32_t child = GREF(uint32_t, node + 8u);
            std::fprintf(stderr,
                         " d%u=%#010x:%#x/%#010x/%#010x",
                         depth, node, type, payload, child);
            if (child != 0 && pending_count < 16u)
                pending[pending_count++] = child;
            if (type == 0x1Eu && payload != 0 && pending_count < 16u)
                pending[pending_count++] = payload;
        }
        std::fputc('\n', stderr);
        return;
    }
    if ((stage == 133u || stage == 134u) && openrac_guest_base != nullptr) {
        const uint32_t cursor = GREF(uint32_t, reader + 0x18u);
        std::fprintf(stderr, "[loader-group] stage=%u value=%u cursor=%#010x next=",
                     stage, object, cursor);
        for (uint32_t i = 0; i < 16u; ++i)
            std::fprintf(stderr, "%s%02x", i == 0 ? "" : " ",
                         openrac_guest_base[cursor + i]);
        std::fputc('\n', stderr);
        return;
    }
    if (stage == 162u) {
        if (object == 0x0021B4D8u) {
            const uint32_t descriptor = reader;
            std::fprintf(stderr,
                         "[loader] unresolved resource dispatch address=%#010x "
                         "descriptor=%#010x fields=%08x/%08x/%08x/%08x/%08x/%08x\n",
                         object, descriptor,
                         GREF(uint32_t, descriptor), GREF(uint32_t, descriptor + 4u),
                         GREF(uint32_t, descriptor + 8u), GREF(uint32_t, descriptor + 12u),
                         GREF(uint32_t, descriptor + 20u), GREF(uint32_t, descriptor + 24u));
        }
        return;
    }
    if (stage == 87u && std::getenv("OPENRAC_DUMP_LZ_WINDOWS") != nullptr &&
        openrac_guest_base != nullptr) {
        static unsigned dump_index = 0;
        char path[64];
        std::snprintf(path, sizeof(path), "/tmp/sly1-lz-window-%03u.bin", dump_index++);
        if (FILE* file = std::fopen(path, "wb")) {
            const uint32_t byte_count = object > 0x2000u ? 0x2000u : object;
            std::fwrite(openrac_guest_base + 0x70000000u, 1, byte_count, file);
            std::fclose(file);
        }
    }
    if (stage == 70u) {
        static unsigned dump_index = 0;
        if (dump_index < 2 && openrac_guest_base != nullptr) {
            char path[64];
            std::snprintf(path, sizeof(path), "/tmp/sly1-collision-window-%u.bin", dump_index++);
            if (FILE* file = std::fopen(path, "wb")) {
                std::fwrite(openrac_guest_base + 0x70000000u, 1, 0x2000u, file);
                std::fclose(file);
            }
        }
    }
    if ((stage == 147u || stage == 149u ||
         (stage == 148u && (object >> 16) >= 0x3500u)) &&
        reader != 0u) {
        std::fprintf(stderr,
                     "[loader-stream] stage=%u value=%#010x flags=%#x mode=%d "
                     "ptr=%#010x left=%d next=%#010x next_count=%d f28=%#x "
                     "f2c=%#x f30=%#x f34=%#x f38=%d size=%d left_file=%d "
                     "start=%d sector=%d chunks=%d cur=%d read=%d write=%d\n",
                     stage, object, GREF(uint32_t, reader),
                     static_cast<int32_t>(GREF(uint32_t, reader + 4u)),
                     GREF(uint32_t, reader + 0x18u),
                     static_cast<int32_t>(GREF(uint32_t, reader + 0x1Cu)),
                     GREF(uint32_t, reader + 0x20u),
                     static_cast<int32_t>(GREF(uint32_t, reader + 0x24u)),
                     GREF(uint32_t, reader + 0x28u), GREF(uint32_t, reader + 0x2Cu),
                     GREF(uint32_t, reader + 0x30u), GREF(uint32_t, reader + 0x34u),
                     static_cast<int32_t>(GREF(uint32_t, reader + 0x38u)),
                     static_cast<int32_t>(GREF(uint32_t, reader + 0x3Cu)),
                     static_cast<int32_t>(GREF(uint32_t, reader + 0x40u)),
                     static_cast<int32_t>(GREF(uint32_t, reader + 0x44u)),
                     static_cast<int32_t>(GREF(uint32_t, reader + 0x48u)),
                     static_cast<int32_t>(GREF(uint32_t, reader + 0x4Cu)),
                     static_cast<int32_t>(GREF(uint32_t, reader + 0x50u)),
                     static_cast<int32_t>(GREF(uint32_t, reader + 0x54u)),
                     static_cast<int32_t>(GREF(uint32_t, reader + 0x58u)));
    }
    std::fprintf(stderr,
                 "[loader] thread=%u stage=%u object=%#010x reader=%#010x "
                 "cursor=%#010x remaining=%d heap=%#010x/%#010x\n",
                 openrac_guest_current_thread_id(),
                 stage, object, reader,
                 GREF(uint32_t, reader + 0x18u),
                 static_cast<int32_t>(GREF(uint32_t, reader + 0x1Cu)),
                 GREF(uint32_t, 0x002C2A1Cu), GREF(uint32_t, 0x002699A8u));
    if (stage == 146u) {
        const uint32_t count = GREF(uint32_t, 0x002744F8u);
        const uint32_t table = GREF(uint32_t, 0x002744FCu);
        const uint64_t bytes = static_cast<uint64_t>(count) * 40u;
        std::fprintf(stderr,
                     "[resource-key-table] count=%u table=%#010x bytes=%llu\n",
                     count, table, static_cast<unsigned long long>(bytes));
        if (count != 0u && count <= 4096u && table < openrac_guest_ram_size &&
            bytes <= static_cast<uint64_t>(openrac_guest_ram_size - table)) {
            const uint32_t shown = std::min(count, 1024u);
            for (uint32_t i = 0; i < shown; ++i) {
                const uint32_t entry = table + i * 40u;
                std::fprintf(stderr,
                             "[resource-key-table] row=%u key=%d type=%u "
                             "payload=%#010x subcount=%u\n",
                             i, static_cast<int16_t>(GREF(uint16_t, entry + 2u)),
                             GREF(uint8_t, entry), GREF(uint32_t, entry + 0x24u),
                             GREF(uint8_t, entry + 0x13u));
            }
            if (shown != count)
                std::fprintf(stderr, "[resource-key-table] omitted=%u\n", count - shown);
        } else if (count != 0u) {
            std::fprintf(stderr,
                         "[resource-key-table] entries outside guest RAM or count invalid\n");
        }
    }
    if ((stage == 145u || stage == 146u) &&
        std::getenv("OPENRAC_DUMP_ANIMATION_WINDOW") != nullptr &&
        openrac_guest_base != nullptr) {
        static unsigned dump_index = 0;
        char path[80];
        std::snprintf(path, sizeof(path),
                      "/tmp/sly1-animation-window-%u-%u.bin",
                      dump_index++, stage);
        const uint32_t cursor = GREF(uint32_t, reader + 0x18u);
        const uint32_t start = cursor >= 0x40u ? cursor - 0x40u : cursor;
        const uint32_t offset = start;
        if (offset >= 0x70000000u && offset < 0x70004000u) {
            const size_t size = std::min<size_t>(0x1000u,
                0x70004000u - offset);
            if (FILE* file = std::fopen(path, "wb")) {
                std::fwrite(openrac_guest_base + offset, 1, size, file);
                std::fclose(file);
                std::fprintf(stderr,
                             "[loader] dumped animation window stage=%u "
                             "cursor=%#010x offset=%#010x bytes=%zu path=%s\n",
                             stage, cursor, offset, size, path);
            }
        }
    }
    if (direct_level_dump_active && (stage == 130u || stage == 131u) &&
        direct_level_window_index < 2u && openrac_guest_base != nullptr) {
        char path[64];
        std::snprintf(path, sizeof(path), "/tmp/sly1-direct-object-%u.bin",
                      direct_level_window_index++);
        if (FILE* file = std::fopen(path, "wb")) {
            std::fwrite(openrac_guest_base + 0x70000000u, 1, 0x2000u, file);
            std::fclose(file);
            std::fprintf(stderr,
                         "[loader-group] dumped stage=%u reader=%#010x cursor=%#010x to %s\n",
                         stage, reader, GREF(uint32_t, reader + 0x18u), path);
        }
    }
    if (stage >= 140u && stage <= 142u) {
        const uint32_t cursor = GREF(uint32_t, reader + 0x18u);
        if (stage == 140u && cursor == 0x700008EAu &&
            std::getenv("OPENRAC_DUMP_COLLISION_WINDOW") != nullptr) {
            if (FILE* file = std::fopen("/tmp/sly1-collision-reader-window.bin", "wb")) {
                std::fwrite(openrac_guest_base + 0x70000000u, 1, 0x2000u, file);
                std::fclose(file);
            }
        }
        std::fprintf(stderr, "[loader] collision bytes at %#010x:", cursor - 8u);
        for (uint32_t i = 0; i < 24u; ++i) {
            std::fprintf(stderr, " %02x",
                         openrac_guest_base[cursor - 8u + i]);
        }
        std::fputc('\n', stderr);
    }
    if (stage >= 70u && stage <= 86u) {
        std::fprintf(stderr,
                     "[loader] state flags=%#x mode=%#x read=%#x pending=%#x "
                     "source=%#x source_left=%#x control=%#x write=%#x magic=%#x\n",
                     GREF(uint32_t, reader), GREF(uint32_t, reader + 4u),
                     GREF(uint32_t, reader + 0x10u), GREF(uint32_t, reader + 0x14u),
                     GREF(uint32_t, reader + 0x20u), GREF(uint32_t, reader + 0x24u),
                     GREF(uint32_t, reader + 0x28u), GREF(uint32_t, reader + 0x3Cu),
                     GREF(uint32_t, reader + 0x38u));
    }
    if (stage == 17u) {
        uint8_t prefix[8]{};
        std::memcpy(prefix, openrac_guest_base + 0x70000000u, sizeof(prefix));
        std::fprintf(stderr,
                     "[loader] decoded_prefix=%02x%02x%02x%02x%02x%02x%02x%02x splash_tag=%#010x\n",
                     prefix[0], prefix[1], prefix[2], prefix[3],
                     prefix[4], prefix[5], prefix[6], prefix[7],
                     GREF(uint32_t, 0x00275714u));
    }
}

extern "C" void sly1_native_start_loader(void) {
    const gaddr saved_sp = openrac_guest_sp;
    const gaddr params = openrac_guest_frame(0x30u);
    std::memset(G(params), 0, 0x30u);

    const uint32_t current = static_cast<uint32_t>(openrac_guest_kernel_syscall(
        0x2Fu, 0, 0, 0, 0, 0, 0, 0, 0));
    (void)openrac_guest_kernel_syscall(0x29u, current, 4, 0, 0, 0, 0, 0, 0);

    GREF(uint32_t, params + 0x04u) = 0x0015EE80u;
    GREF(uint32_t, params + 0x08u) = 0x006056D0u;
    GREF(uint32_t, params + 0x0Cu) = 0x00020000u;
    GREF(uint32_t, params + 0x10u) = 0x002832F0u;
    GREF(uint32_t, params + 0x14u) = 2u;
    const uint32_t thread = static_cast<uint32_t>(openrac_guest_kernel_syscall(
        0x20u, params, 0, 0, 0, 0, 0, 0, 0));
    GREF(uint32_t, 0x0027595Cu) = thread;
    openrac_guest_sp = saved_sp;
}
