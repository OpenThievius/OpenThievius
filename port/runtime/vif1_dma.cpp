/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Native handling for the VIF1 DMA subset used by Sly's display lists. */
#include "openrac/guest.h"
#include "openrac/native_gs.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

#include "common/log.h"

namespace {
constexpr uint32_t kVif1Chcr = 0x10009000u;
constexpr uint32_t kDmacCtrl = 0x1000E000u;
constexpr uint32_t kDmacStat = 0x1000E010u;
constexpr size_t kMaxTransferBytes = 64u * 1024u * 1024u;
constexpr uint32_t kMaxTags = 1u << 16;

struct DmaAddress {
    uint32_t address;
    bool scratchpad;
};

uint32_t encode_address(DmaAddress value) {
    return (value.address & 0x7FFFFFFFu) | (value.scratchpad ? 0x80000000u : 0u);
}

uint32_t load_mmio(uint32_t address) {
    uint32_t value = 0;
    std::memcpy(&value, openrac_guest_base + address, sizeof(value));
    return value;
}

void store_mmio(uint32_t address, uint32_t value) {
    std::memcpy(openrac_guest_base + address, &value, sizeof(value));
}

bool guest_span(DmaAddress address, size_t size, const uint8_t** source) {
    if (openrac_guest_base == nullptr) return false;
    if (address.scratchpad) {
        if (address.address > GUEST_SCRATCHPAD_SIZE ||
            size > GUEST_SCRATCHPAD_SIZE - address.address) return false;
        *source = openrac_guest_base + GUEST_SCRATCHPAD + address.address;
        return true;
    }
    const uint32_t normalized = openrac_guest_ram_address(address.address);
    if (normalized > openrac_guest_ram_size || size > openrac_guest_ram_size - normalized)
        return false;
    *source = openrac_guest_base + normalized;
    return true;
}

bool append_span(DmaAddress address, uint32_t qwc, std::vector<uint8_t>* stream) {
    const size_t bytes = static_cast<size_t>(qwc) * 16u;
    if (bytes > kMaxTransferBytes - stream->size()) return false;
    const uint8_t* source = nullptr;
    if (!guest_span(address, bytes, &source)) return false;
    stream->insert(stream->end(), source, source + bytes);
    return true;
}

bool append_tag_data(const uint8_t* tag, std::vector<uint8_t>* stream) {
    constexpr size_t tag_data_bytes = 8;
    if (tag_data_bytes > kMaxTransferBytes - stream->size()) return false;
    stream->insert(stream->end(), tag + 8, tag + 16);
    return true;
}

bool execute_chain(uint32_t chcr, std::vector<uint8_t>* stream) {
    if ((chcr & 1u) == 0 || ((chcr >> 2) & 3u) != 1u)
        return false;

    const uint32_t tag_address = load_mmio(kVif1Chcr + 0x30u);
    DmaAddress tadr{tag_address & 0x7FFFFFFFu, (tag_address >> 31) != 0};
    std::array<DmaAddress, 2> return_addresses{};
    size_t depth = 0;

    for (uint32_t index = 0; index < kMaxTags; ++index) {
        const uint8_t* tag = nullptr;
        if (!guest_span(tadr, 16, &tag)) return false;
        uint64_t low = 0;
        std::memcpy(&low, tag, sizeof(low));
        const uint32_t qwc = static_cast<uint32_t>(low & 0xFFFFu);
        const uint32_t id = static_cast<uint32_t>((low >> 28) & 7u);
        const DmaAddress data_address{static_cast<uint32_t>((low >> 32) & 0x7FFFFFFFu),
                                      (low >> 63) != 0};
        DmaAddress payload{tadr.address + 16u, tadr.scratchpad};
        DmaAddress next = payload;
        bool stop = false;

        switch (id) {
        case 0: // REFE
            payload = data_address;
            stop = true;
            break;
        case 1: // CNT
            next.address += qwc * 16u;
            break;
        case 2: // NEXT
            next = data_address;
            break;
        case 3: // REF
        case 4: // REFS
            payload = data_address;
            break;
        case 5: // CALL
            if (depth >= return_addresses.size()) return false;
            return_addresses[depth++] = DmaAddress{tadr.address + 16u + qwc * 16u,
                                                   tadr.scratchpad};
            next = data_address;
            break;
        case 6: // RET
            if (depth == 0) stop = true;
            else next = return_addresses[--depth];
            break;
        case 7: // END
            stop = true;
            break;
        }

        if ((chcr & (1u << 6)) != 0 && !append_tag_data(tag, stream)) return false;
        if (!append_span(payload, qwc, stream)) return false;
        store_mmio(kVif1Chcr + 0x10u,
                   encode_address(DmaAddress{payload.address + qwc * 16u,
                                             payload.scratchpad}));
        store_mmio(kVif1Chcr + 0x20u, 0);
        store_mmio(kVif1Chcr + 0x30u, encode_address(next));
        if (stop) return true;
        tadr = next;
    }
    return false;
}

bool execute_dma(uint32_t chcr, std::vector<uint8_t>* stream) {
    if ((load_mmio(kDmacCtrl) & 1u) == 0 || (chcr & 1u) == 0) return false;
    const uint32_t mode = (chcr >> 2) & 3u;
    if (mode == 1u) return execute_chain(chcr, stream);
    if (mode != 0u) return false;

    const uint32_t madr = load_mmio(kVif1Chcr + 0x10u);
    const uint32_t qwc = load_mmio(kVif1Chcr + 0x20u) & 0xFFFFu;
    const DmaAddress source{madr & 0x7FFFFFFFu, (madr >> 31) != 0};
    if (!append_span(source, qwc, stream)) return false;
    store_mmio(kVif1Chcr + 0x10u,
               encode_address(DmaAddress{source.address + qwc * 16u, source.scratchpad}));
    store_mmio(kVif1Chcr + 0x20u, 0);
    return true;
}

bool skip_bytes(size_t* cursor, size_t amount, size_t size) {
    if (amount > size - *cursor) return false;
    *cursor += amount;
    return true;
}

struct VifTrace {
    std::array<uint32_t, 0x80> command_counts{};
    std::array<std::pair<uint32_t, uint32_t>, 32> mpg{};
    std::array<std::pair<uint32_t, uint32_t>, 32> mscal{};
    uint32_t mpg_total = 0;
    uint32_t mscal_total = 0;
    size_t mpg_count = 0;
    size_t mscal_count = 0;
};

bool execute_vif_stream(const std::vector<uint8_t>& stream, size_t* direct_bytes,
                        uint32_t* unsupported_command, VifTrace* trace) {
    size_t cursor = 0;
    while (cursor + sizeof(uint32_t) <= stream.size()) {
        uint32_t code = 0;
        std::memcpy(&code, stream.data() + cursor, sizeof(code));
        cursor += sizeof(code);
        const uint32_t command = (code >> 24) & 0x7Fu;
        const uint32_t count = (code >> 16) & 0xFFu;
        const uint32_t immediate = code & 0xFFFFu;
        ++trace->command_counts[command];

        if (command == 0x50u || command == 0x51u) { // DIRECT / DIRECTHL
            const uint32_t qwc = immediate == 0 ? 0x10000u : immediate;
            const size_t bytes = static_cast<size_t>(qwc) * 16u;
            if (bytes > stream.size() - cursor) return false;
            openrac_guest_gif_stream_write(stream.data() + cursor, bytes);
            cursor += bytes;
            *direct_bytes += bytes;
            continue;
        }

        if (command >= 0x60u && command <= 0x7Fu) { // UNPACK
            const uint32_t loops = count == 0 ? 256u : count;
            const uint32_t format = command & 0xFu;
            const uint32_t components = ((format >> 2) & 3u) + 1u;
            constexpr uint32_t bits_per_component[] = {32u, 16u, 8u, 5u};
            const uint64_t bits = uint64_t{components} * bits_per_component[format & 3u];
            const uint64_t bytes_per_vector = ((bits + 31u) / 32u) * 4u;
            const uint64_t bytes = bytes_per_vector * loops;
            if (bytes > stream.size() - cursor) return false;
            cursor += static_cast<size_t>(bytes);
            continue;
        }

        size_t immediate_bytes = 0;
        switch (command) {
        case 0x20: // STMASK
            immediate_bytes = 4;
            break;
        case 0x30: // STROW
        case 0x31: // STCOL
            immediate_bytes = 16;
            break;
        case 0x4A: // MPG
            ++trace->mpg_total;
            if (trace->mpg_count < trace->mpg.size())
                trace->mpg[trace->mpg_count++] = {immediate,
                                                   count == 0 ? 256u : count};
            immediate_bytes = static_cast<size_t>(count == 0 ? 256u : count) * 8u;
            break;
        case 0x14: case 0x15: case 0x17: // MSCAL / MSCALF / MSCNT
            ++trace->mscal_total;
            if (trace->mscal_count < trace->mscal.size())
                trace->mscal[trace->mscal_count++] = {command, immediate & 0xFFFFu};
            break;
        case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05:
        case 0x06: case 0x07: case 0x10: case 0x11: case 0x13:
            break;
        default:
            *unsupported_command = command;
            return false;
        }
        if (!skip_bytes(&cursor, immediate_bytes, stream.size())) return false;
    }
    return cursor == stream.size();
}

std::atomic<uint32_t> g_transfer_count{0};
std::atomic<uint32_t> g_traced_transfer_count{0};
std::atomic_flag g_unsupported_notice = ATOMIC_FLAG_INIT;
}

extern "C" int openrac_guest_vif1_mmio_store32(uint32_t address, uint32_t value) {
    if (address != kVif1Chcr || (value & 0x100u) == 0) return 0;
    if (openrac_guest_base == nullptr) return 1;

    store_mmio(kVif1Chcr, value);
    std::vector<uint8_t> stream;
    size_t direct_bytes = 0;
    uint32_t unsupported = 0xFFFFFFFFu;
    VifTrace trace;
    const bool dma_ok = execute_dma(value, &stream);
    const bool vif_ok = dma_ok && execute_vif_stream(stream, &direct_bytes, &unsupported,
                                                       &trace);
    const uint32_t status = load_mmio(kDmacStat);
    if (dma_ok) {
        store_mmio(kDmacStat, status | (1u << 1));
        store_mmio(kVif1Chcr, value & ~0x100u);
        openrac_guest_raise_dmac(1);
        const uint32_t transfer = g_transfer_count.fetch_add(1, std::memory_order_relaxed);
        uint32_t trace_index = 8;
        const char* trace_setting = std::getenv("OPENRAC_VIF_TRACE");
        if (trace_setting != nullptr && trace_setting[0] != '\0' && trace_setting[0] != '0') {
            const bool has_commands = std::any_of(trace.command_counts.begin(),
                                                   trace.command_counts.end(),
                                                   [](uint32_t count) { return count != 0; });
            if (has_commands || direct_bytes != 0)
                trace_index = g_traced_transfer_count.fetch_add(1,
                                                               std::memory_order_relaxed);
        }
        if (trace_index < 2) {
            openrac::log::info("native VIF1 DMA transfer={} bytes={} DIRECT_bytes={} MPG={} MSCAL={}",
                               transfer + 1, stream.size(), direct_bytes, trace.mpg_total,
                               trace.mscal_total);
            for (size_t command = 0; command < trace.command_counts.size(); ++command) {
                if (trace.command_counts[command] != 0)
                    openrac::log::info("native VIF1 command transfer={} opcode={:#x} count={}",
                                       transfer + 1, command,
                                       trace.command_counts[command]);
            }
            for (size_t i = 0; i < trace.mpg_count; ++i)
                openrac::log::info("native VIF1 MPG transfer={} address={} doublewords={}",
                                   transfer + 1, trace.mpg[i].first, trace.mpg[i].second);
            for (size_t i = 0; i < trace.mscal_count; ++i)
                openrac::log::info("native VIF1 microcall transfer={} command={:#x} address={}",
                                   transfer + 1, trace.mscal[i].first, trace.mscal[i].second);
        }
    } else {
        store_mmio(kDmacStat, (status & ~(1u << 1)) | (1u << 15));
        store_mmio(kVif1Chcr, value & ~0x100u);
        openrac::log::warn("native VIF1 DMA rejected transfer: CHCR={:#010x} TADR={:#010x}",
                           value, load_mmio(kVif1Chcr + 0x30u));
    }

    if (!vif_ok && !g_unsupported_notice.test_and_set(std::memory_order_relaxed)) {
        openrac::log::warn("native VIF1 command stream is unsupported or malformed: command={:#04x}",
                           unsupported);
    }
    return 1;
}
