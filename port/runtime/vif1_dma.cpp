/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Native handling for the VIF1 DMA subset used by Sly's display lists. */
#include "openrac/guest.h"
#include "openrac/native_gs.h"
#include "openrac/native_vu1.h"

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
constexpr uint32_t kVif1Cycle = 0x10003C40u;
constexpr uint32_t kVif1Base = 0x10003CA0u;
constexpr uint32_t kVif1Offset = 0x10003CB0u;
constexpr uint32_t kVif1Tops = 0x10003CC0u;
constexpr uint32_t kVif1Status = 0x10003C00u;
constexpr uint32_t kVu1Data = 0x11008000u;
constexpr uint32_t kVu1Micro = 0x1100C000u;
constexpr size_t kVu1MemoryBytes = 16u * 1024u;
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

uint32_t read_bits(const uint8_t* source, size_t bit_offset, uint32_t bit_count) {
    uint32_t value = 0;
    for (uint32_t bit = 0; bit < bit_count; ++bit) {
        const size_t source_bit = bit_offset + bit;
        value |= uint32_t((source[source_bit / 8u] >> (source_bit & 7u)) & 1u) << bit;
    }
    return value;
}

bool unpack_vu1(uint32_t command, uint32_t count, uint32_t immediate,
                const uint8_t* source, size_t source_size) {
    if ((command & 0x10u) != 0) return false; // VIF write masking is not observed in Sly's stream.
    const uint32_t vectors = count == 0 ? 256u : count;
    const uint32_t format = command & 0xFu;
    const uint32_t components = ((format >> 2u) & 3u) + 1u;
    constexpr uint32_t bits_per_component[] = {32u, 16u, 8u, 5u};
    const uint32_t component_bits = bits_per_component[format & 3u];
    const uint32_t vector_bits = components * component_bits;
    const size_t input_bytes_per_vector = ((vector_bits + 31u) / 32u) * 4u;
    if (vectors > source_size / input_bytes_per_vector) return false;

    const uint32_t cycle = load_mmio(kVif1Cycle);
    const uint32_t cycle_length = (cycle & 0xFFu) == 0 ? 256u : cycle & 0xFFu;
    const uint32_t write_length = ((cycle >> 8u) & 0xFFu) == 0
                                      ? 256u
                                      : (cycle >> 8u) & 0xFFu;
    if (write_length > cycle_length ||
        (write_length != cycle_length && write_length != 1u)) return false;
    const uint32_t destination_stride = write_length == cycle_length ? 1u : cycle_length;

    uint32_t destination = immediate & 0x3FFu;
    if ((immediate & 0x8000u) != 0)
        destination = (destination + (load_mmio(kVif1Tops) & 0x3FFu)) & 0x3FFu;
    const bool zero_extend = (immediate & 0x4000u) != 0;
    const uint8_t* input = source;

    for (uint32_t vector_index = 0; vector_index < vectors; ++vector_index) {
        const uint32_t destination_index =
            (destination + vector_index * destination_stride) & 0x3FFu;
        uint8_t* output = openrac_guest_base + kVu1Data + destination_index * 16u;
        uint32_t lanes[4];
        std::memcpy(lanes, output, sizeof(lanes));

        for (uint32_t component = 0; component < components; ++component) {
            uint32_t value = read_bits(input, component * component_bits, component_bits);
            if (!zero_extend && component_bits < 32u &&
                (value & (1u << (component_bits - 1u))) != 0) {
                value |= ~((1u << component_bits) - 1u);
            }
            lanes[component] = value;
        }
        if (components == 1u) {
            lanes[1] = lanes[0];
            lanes[2] = lanes[0];
            lanes[3] = lanes[0];
        }
        std::memcpy(output, lanes, sizeof(lanes));
        input += input_bytes_per_vector;
    }
    return true;
}

struct VifTrace {
    std::array<uint32_t, 0x80> command_counts{};
    std::array<std::pair<uint32_t, uint32_t>, 32> mpg{};
    std::array<std::pair<uint32_t, uint32_t>, 32> mscal{};
    struct Operand { uint32_t command; uint32_t count; uint32_t immediate; };
    std::array<Operand, 48> operands{};
    struct Program {
        uint32_t address = 0;
        uint32_t instruction_count = 0;
        std::array<uint32_t, 64> upper_opcodes{};
    };
    std::array<Program, 32> programs{};
    uint32_t mpg_total = 0;
    uint32_t mscal_total = 0;
    size_t mpg_count = 0;
    size_t mscal_count = 0;
    size_t operand_count = 0;
    size_t program_count = 0;
};

std::atomic_flag g_vu_unsupported_notice = ATOMIC_FLAG_INIT;

void trace_vu1_program(uint32_t address, VifTrace* trace) {
    address &= 0x7FFu;
    for (size_t i = 0; i < trace->program_count; ++i) {
        if (trace->programs[i].address == address) return;
    }
    if (trace->program_count == trace->programs.size()) return;

    VifTrace::Program program;
    program.address = address;
    for (uint32_t index = address; index < kVu1MemoryBytes / 8u; ++index) {
        uint32_t upper = 0;
        const uint8_t* instruction = openrac_guest_base + kVu1Micro + index * 8u;
        std::memcpy(&upper, instruction + 4u, sizeof(upper));
        ++program.instruction_count;
        ++program.upper_opcodes[upper & 0x3Fu];
        if ((upper & (1u << 30u)) != 0) break; // E-bit ends the microprogram.
    }
    trace->programs[trace->program_count++] = program;
}

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
        if (((command >= 1u && command <= 3u) ||
             (command >= 0x60u && command <= 0x7Fu)) &&
            trace->operand_count < trace->operands.size()) {
            trace->operands[trace->operand_count++] = {command, count, immediate};
        }

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
            if (!unpack_vu1(command, count, immediate, stream.data() + cursor,
                            static_cast<size_t>(bytes))) {
                *unsupported_command = command;
                return false;
            }
            cursor += static_cast<size_t>(bytes);
            continue;
        }

        size_t immediate_bytes = 0;
        switch (command) {
        case 0x01: // STCYCL
            store_mmio(kVif1Cycle, immediate);
            break;
        case 0x02: { // OFFSET (VIF1)
            const uint32_t tops = load_mmio(kVif1Tops) & 0x3FFu;
            store_mmio(kVif1Offset, immediate & 0x3FFu);
            store_mmio(kVif1Base, tops);
            store_mmio(kVif1Status, load_mmio(kVif1Status) & ~(1u << 7));
            store_mmio(kVif1Tops, tops);
            break;
        }
        case 0x03: { // BASE (VIF1)
            store_mmio(kVif1Base, immediate & 0x3FFu);
            const bool double_buffer = (load_mmio(kVif1Status) & (1u << 7)) != 0;
            const uint32_t tops = (immediate +
                                   (double_buffer ? load_mmio(kVif1Offset) : 0u)) & 0x3FFu;
            store_mmio(kVif1Tops, tops);
            break;
        }
        case 0x04: // ITOP
            openrac_guest_vu1_set_itop(immediate & 0x3FFu);
            break;
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
            if (immediate_bytes > stream.size() - cursor) return false;
            if (immediate > kVu1MemoryBytes / 8u ||
                immediate_bytes > kVu1MemoryBytes - static_cast<size_t>(immediate) * 8u) {
                *unsupported_command = command;
                return false;
            }
            std::memcpy(openrac_guest_base + kVu1Micro + static_cast<size_t>(immediate) * 8u,
                        stream.data() + cursor, immediate_bytes);
            break;
        case 0x14: case 0x15: case 0x17: // MSCAL / MSCALF / MSCNT
            ++trace->mscal_total;
            if (trace->mscal_count < trace->mscal.size())
                trace->mscal[trace->mscal_count++] = {command, immediate & 0xFFFFu};
            if (command != 0x17u) trace_vu1_program(immediate, trace);
            {
                uint32_t vu_unsupported = 0;
                const uint32_t entry = command == 0x17u ? 0xFFFFFFFFu : immediate;
                if (!openrac_guest_vu1_execute(entry, &vu_unsupported) &&
                    !g_vu_unsupported_notice.test_and_set(std::memory_order_relaxed)) {
                    openrac::log::warn(
                        "native VU1 translation stopped at instruction {:#010x} from entry {}",
                        vu_unsupported, entry == 0xFFFFFFFFu ? "MSCNT" : "MSCAL");
                }
            }
            break;
        case 0x00: case 0x05: case 0x06: case 0x07:
        case 0x10: case 0x11: case 0x13:
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
            for (size_t i = 0; i < trace.operand_count; ++i)
                openrac::log::info("native VIF1 operand transfer={} opcode={:#x} num={} immediate={:#x}",
                                   transfer + 1, trace.operands[i].command,
                                   trace.operands[i].count, trace.operands[i].immediate);
            for (size_t i = 0; i < trace.program_count; ++i) {
                const VifTrace::Program& program = trace.programs[i];
                openrac::log::info("native VIF1 microprogram transfer={} address={} instructions={}",
                                   transfer + 1, program.address, program.instruction_count);
                for (size_t opcode = 0; opcode < program.upper_opcodes.size(); ++opcode) {
                    if (program.upper_opcodes[opcode] != 0)
                        openrac::log::info("native VIF1 upper-op transfer={} address={} opcode={:#x} count={}",
                                           transfer + 1, program.address, opcode,
                                           program.upper_opcodes[opcode]);
                }
            }
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
