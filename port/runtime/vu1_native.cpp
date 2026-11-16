/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Host translation of the VU1 display-list subset submitted by Sly. */
#include "openrac/native_vu1.h"

#include "openrac/guest.h"
#include "openrac/native_gs.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <sstream>
#include <vector>

#include "common/log.h"

namespace {
constexpr uint32_t kVu1Data = 0x1100C000u;
constexpr uint32_t kVu1Micro = 0x11008000u;
constexpr uint32_t kVif1Top = 0x10003CE0u;
constexpr size_t kVu1Doublewords = 2048;
constexpr size_t kVu1Quadwords = 1024;
constexpr uint32_t kMaxInstructions = 4096;

uint8_t* guest_address(uint32_t address) {
#if defined(OPENRAC_SLY1_DIRECT_RETAIL_MEMORY)
    return reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(address));
#else
    return openrac_guest_base == nullptr ? nullptr : openrac_guest_base + address;
#endif
}

bool guest_memory_available() {
#if defined(OPENRAC_SLY1_DIRECT_RETAIL_MEMORY)
    return true;
#else
    return openrac_guest_base != nullptr;
#endif
}

using Vector = std::array<float, 4>;

struct State {
    std::array<Vector, 32> vf{};
    std::array<uint16_t, 16> vi{};
    Vector acc{};
    float q = 0.0f;
    float p = 0.0f;
    float i = 0.0f;
    uint16_t itop = 0;
    uint16_t mac_flags = 0;
    uint16_t status_flags = 0;
    uint32_t clip_flags = 0;
    uint32_t pc = 0;
};

struct PendingKick {
    bool active = false;
    uint32_t address = 0;
    uint32_t pc = 0;
    uint32_t vi4 = 0;
    uint32_t vi3 = 0;
    uint32_t top = 0;
    uint32_t tops = 0;
    Vector vf7{};
    uint32_t delay = 0;
};

std::mutex g_vu_mutex;
State g_state;
std::atomic_flag g_vu_context_notice = ATOMIC_FLAG_INIT;
std::atomic<uint32_t> g_vu_kick_notice_count{0};
std::atomic<uint32_t> g_vu_kick_success_notice_count{0};

Vector vf_read(const State& state, uint32_t index) {
    if (index == 0) return {0.0f, 0.0f, 0.0f, 1.0f};
    return state.vf[index & 31u];
}

void vf_write(State* state, uint32_t index, const Vector& value, uint32_t mask) {
    if ((index & 31u) == 0) return;
    Vector& destination = state->vf[index & 31u];
    for (uint32_t lane = 0; lane < 4; ++lane)
        if ((mask & (8u >> lane)) != 0) destination[lane] = value[lane];
}

void update_float_flags(State* state, const Vector& value, uint32_t mask) {
    uint16_t overflow = 0, underflow = 0, sign = 0, zero = 0;
    for (uint32_t lane = 0; lane < 4; ++lane) {
        if ((mask & (8u >> lane)) == 0) continue;
        const uint16_t bit = static_cast<uint16_t>(1u << (3u - lane));
        if (std::signbit(value[lane])) sign |= bit;
        if (value[lane] == 0.0f) zero |= bit;
        if (std::isinf(value[lane])) overflow |= bit;
        if (value[lane] != 0.0f && std::fpclassify(value[lane]) == FP_SUBNORMAL)
            underflow |= bit;
    }
    state->mac_flags = static_cast<uint16_t>((overflow << 12u) | (underflow << 8u) |
                                             (sign << 4u) | zero);
    const uint16_t current = static_cast<uint16_t>(
        (zero != 0 ? 1u : 0u) | (sign != 0 ? 2u : 0u) |
        (underflow != 0 ? 4u : 0u) | (overflow != 0 ? 8u : 0u));
    const uint16_t sticky = static_cast<uint16_t>((state->status_flags >> 6u) | current);
    state->status_flags = static_cast<uint16_t>(current | ((sticky & 0x3Fu) << 6u));
}

Vector data_read(uint32_t address) {
    Vector value{};
    address &= static_cast<uint32_t>(kVu1Quadwords - 1u);
    std::memcpy(value.data(), guest_address(kVu1Data + address * 16u), 16u);
    static std::atomic<uint32_t> qword11_notice{0};
    if (address == 11u && std::getenv("OPENRAC_TRACE_VU_QWORD11") != nullptr &&
        qword11_notice.fetch_add(1, std::memory_order_relaxed) < 512u) {
        openrac::log::info(
            "native VU1 data read qword=11 pc={:#x} words={:#010x},{:#010x},{:#010x},{:#010x}",
            g_state.pc, std::bit_cast<uint32_t>(value[0]),
            std::bit_cast<uint32_t>(value[1]), std::bit_cast<uint32_t>(value[2]),
            std::bit_cast<uint32_t>(value[3]));
    }
    return value;
}

void data_write(uint32_t address, const Vector& value) {
    address &= static_cast<uint32_t>(kVu1Quadwords - 1u);
    uint32_t previous[4]{};
    static const bool setup_trace_enabled = [] {
        const char* setting = std::getenv("OPENRAC_TRACE_VU1_SETUP");
        return setting != nullptr && setting[0] != '\0' && setting[0] != '0';
    }();
    const bool trace_target =
        (address >= 32u && address <= 38u) ||
        (address >= 464u && address <= 476u);
    if (trace_target)
        std::memcpy(previous, guest_address(kVu1Data + address * 16u), sizeof(previous));
    std::memcpy(guest_address(kVu1Data + address * 16u), value.data(), 16u);
    static std::atomic<uint32_t> qword11_notice{0};
    if (address == 11u && std::getenv("OPENRAC_TRACE_VU_QWORD11") != nullptr &&
        qword11_notice.fetch_add(1, std::memory_order_relaxed) < 512u) {
        uint32_t instruction[2]{};
        const uint8_t* instruction_address =
            guest_address(kVu1Micro + (g_state.pc & 0x7FFu) * 8u);
        std::memcpy(instruction, instruction_address, sizeof(instruction));
        openrac::log::info(
            "native VU1 data write qword=11 pc={:#x} instruction={:#010x}/{:#010x} words={:#010x},{:#010x},{:#010x},{:#010x}",
            g_state.pc, instruction[0], instruction[1], std::bit_cast<uint32_t>(value[0]),
            std::bit_cast<uint32_t>(value[1]), std::bit_cast<uint32_t>(value[2]),
            std::bit_cast<uint32_t>(value[3]));
    }
    static std::atomic<uint32_t> target_write_notice{0};
    if (trace_target && setup_trace_enabled &&
        target_write_notice.fetch_add(1, std::memory_order_relaxed) < 32u) {
        uint32_t next[4]{};
        std::memcpy(next, value.data(), sizeof(next));
        openrac::log::info(
            "native VU1 store wrote qword={} old={:#010x},{:#010x},{:#010x},{:#010x} new={:#010x},{:#010x},{:#010x},{:#010x}",
            address,
            previous[0], previous[1], previous[2], previous[3],
            next[0], next[1], next[2], next[3]);
    }
}

int32_t sign_extend(uint32_t value, uint32_t bits) {
    const uint32_t sign = 1u << (bits - 1u);
    return static_cast<int32_t>((value ^ sign) - sign);
}

uint16_t vi_read(const State& state, uint32_t index) {
    return index == 0 ? 0 : state.vi[index & 15u];
}

void vi_write(State* state, uint32_t index, uint16_t value) {
    if (index != 0) state->vi[index & 15u] = value;
}

bool kick_gif(uint32_t address, uint32_t* failure, uint64_t* first_tag,
              uint32_t* failed_address, uint64_t* failed_tag,
              uint64_t* failed_tag_high) {
    std::vector<uint8_t> packet;
    size_t cursor = address & 0x3FFu;
    constexpr size_t kMaxKickQwords = 1u << 20;
    size_t consumed_qwords = 0;
    if (failure != nullptr) *failure = 0;
    if (first_tag != nullptr) *first_tag = 0;
    if (failed_address != nullptr) *failed_address = 0;
    if (failed_tag != nullptr) *failed_tag = 0;
    if (failed_tag_high != nullptr) *failed_tag_high = 0;
    while (consumed_qwords < kMaxKickQwords) {
        const uint8_t* tag = guest_address(kVu1Data + cursor * 16u);
        uint64_t low = 0;
        std::memcpy(&low, tag, sizeof(low));
        uint64_t high = 0;
        std::memcpy(&high, tag + sizeof(low), sizeof(high));
        if (first_tag != nullptr && consumed_qwords == 0)
            *first_tag = low;
        constexpr uint64_t kGifReservedMask = 0x00003FFFFFFF0000ull;
        if ((low & kGifReservedMask) != 0) {
            static std::atomic<uint32_t> bad_tag_dump_count{0};
            if (bad_tag_dump_count.fetch_add(1, std::memory_order_relaxed) < 4u) {
                for (int32_t relative = -2; relative <= 2; ++relative) {
                    const size_t nearby = (cursor + kVu1Quadwords + relative) &
                                          (kVu1Quadwords - 1u);
                    const uint8_t* nearby_tag =
                        guest_address(kVu1Data + nearby * 16u);
                    uint64_t nearby_low = 0;
                    uint64_t nearby_high = 0;
                    std::memcpy(&nearby_low, nearby_tag, sizeof(nearby_low));
                    std::memcpy(&nearby_high, nearby_tag + sizeof(nearby_low),
                                sizeof(nearby_high));
                    openrac::log::warn(
                        "native VU1 XGKICK nearby qword={} low={:#018x} high={:#018x}",
                        nearby, nearby_low, nearby_high);
                }
            }
            if (failure != nullptr) *failure = 3;
            if (failed_address != nullptr) *failed_address = static_cast<uint32_t>(cursor);
            if (failed_tag != nullptr) *failed_tag = low;
            if (failed_tag_high != nullptr) *failed_tag_high = high;
            return false;
        }
        const uint32_t loops = static_cast<uint32_t>(low & 0x7FFFu);
        const uint32_t format = static_cast<uint32_t>((low >> 58u) & 3u);
        uint32_t registers = static_cast<uint32_t>((low >> 60u) & 0xFu);
        if (registers == 0) registers = 16;
        size_t payload_qwords = 0;
        if (format == 0) payload_qwords = static_cast<size_t>(loops) * registers;
        else if (format == 1)
            payload_qwords = (static_cast<size_t>(loops) * registers + 1u) / 2u;
        else payload_qwords = loops;
        if (payload_qwords + 1u > kMaxKickQwords - consumed_qwords) {
            if (failure != nullptr) *failure = 1;
            if (failed_address != nullptr) *failed_address = static_cast<uint32_t>(cursor);
            if (failed_tag != nullptr) *failed_tag = low;
            return false;
        }
        for (size_t qword = 0; qword <= payload_qwords; ++qword) {
            const size_t memory_qword = (cursor + qword) & (kVu1Quadwords - 1u);
            const uint8_t* data = guest_address(kVu1Data + memory_qword * 16u);
            packet.insert(packet.end(), data, data + 16u);
        }
        consumed_qwords += payload_qwords + 1u;
        cursor = (cursor + payload_qwords + 1u) & (kVu1Quadwords - 1u);
        if (((low >> 15u) & 1u) != 0) {
            openrac_guest_gif_stream_write_from(packet.data(), packet.size(),
                                                "vu1-xgkick");
            return true;
        }
    }
    if (failure != nullptr) *failure = 2;
    if (failed_address != nullptr) *failed_address = static_cast<uint32_t>(cursor);
    return false;
}

void dispatch_pending_kick(PendingKick* pending) {
    if (pending == nullptr || !pending->active) return;

    const char* trace_state = std::getenv("OPENRAC_TRACE_XGKICK_STATE");
    if (trace_state != nullptr && trace_state[0] != '\0' && trace_state[0] != '0') {
        static std::atomic<uint32_t> state_trace_count{0};
        if (state_trace_count.fetch_add(1, std::memory_order_relaxed) < 32u) {
            uint32_t tag_loaded = 0;
            uint32_t remaining_qwords = 0;
            uint64_t tag_low = 0;
            openrac_guest_gif_parser_probe(&tag_loaded, &remaining_qwords, &tag_low);
            openrac::log::info(
                "native VU1 XGKICK parser state PC={:#x} address={} active={} remaining={} tag={:#018x}",
                pending->pc, pending->address, tag_loaded, remaining_qwords, tag_low);
        }
    }

    uint32_t failure = 0;
    uint32_t failed_address = 0;
    uint64_t first_tag = 0;
    uint64_t failed_tag = 0;
    uint64_t failed_tag_high = 0;
    const bool accepted = kick_gif(pending->address, &failure, &first_tag,
                                   &failed_address, &failed_tag,
                                   &failed_tag_high);
    if (!accepted) {
        const uint32_t notice = g_vu_kick_notice_count.fetch_add(
            1, std::memory_order_relaxed);
        if (notice < 4u)
            openrac::log::warn(
                "native VU1 XGKICK packet rejected at PC {:#x}: VI4={} VI3={} address={} VF7={:#010x},{:#010x},{:#010x},{:#010x} TOP={} TOPS={} first_tag={:#018x} failed_tag@{}={:#018x}:{:#018x} reason={}",
                pending->pc, pending->vi4, pending->vi3, pending->address,
                std::bit_cast<uint32_t>(pending->vf7[0]),
                std::bit_cast<uint32_t>(pending->vf7[1]),
                std::bit_cast<uint32_t>(pending->vf7[2]),
                std::bit_cast<uint32_t>(pending->vf7[3]), pending->top,
                pending->tops, first_tag, failed_address, failed_tag_high,
                failed_tag, failure == 1 ? "packet exceeds native limit" :
                failure == 3 ? "GIFtag has reserved bits set" :
                "missing EOP");
    } else {
        const uint32_t notice = g_vu_kick_success_notice_count.fetch_add(
            1, std::memory_order_relaxed);
        if (notice < 8u)
            openrac::log::info(
                "native VU1 XGKICK submitted at PC {:#x}: address={} first_tag={:#018x}",
                pending->pc, pending->address, first_tag);
    }
    pending->active = false;
    pending->delay = 0;
}

bool translate_upper(State* state, uint32_t word, uint32_t* unsupported) {
    const uint32_t opcode = word & 0x3Fu;
    const uint32_t fd = (word >> 6u) & 31u;
    const uint32_t fs = (word >> 11u) & 31u;
    const uint32_t ft = (word >> 16u) & 31u;
    const uint32_t mask = (word >> 21u) & 15u;
    const Vector source = vf_read(*state, fs);
    const Vector target = vf_read(*state, ft);
    Vector result{};

    if (opcode < 0x1Cu) {
        const uint32_t group = opcode >> 2u;
        const uint32_t broadcast = opcode & 3u;
        for (uint32_t lane = 0; lane < 4; ++lane) {
            const float a = source[lane];
            const float b = target[broadcast];
            switch (group) {
            case 0: result[lane] = a + b; break;
            case 1: result[lane] = a - b; break;
            case 2: result[lane] = state->acc[lane] + a * b; break;
            case 3: result[lane] = state->acc[lane] - a * b; break;
            case 4: result[lane] = std::max(a, b); break;
            case 5: result[lane] = std::min(a, b); break;
            case 6: result[lane] = a * b; break;
            default: *unsupported = word; return false;
            }
        }
        if (group == 2 || group == 3) state->acc = result;
        update_float_flags(state, result, mask);
        vf_write(state, fd, result, mask);
        return true;
    }

    switch (opcode) {
    case 0x1C: // MULQ
    case 0x20: // ADDQ
    case 0x22: // ADDI
    case 0x24: // SUBQ
    case 0x26: // SUBI
    case 0x28: // ADD
    case 0x2A: // MUL
    case 0x2B: // MAX
    case 0x2C: // SUB
    case 0x2F: // MINI
    case 0x29: // MADD
    case 0x2D: // MSUB
    case 0x21: // MADDQ
    case 0x23: // MADDI
    case 0x25: // MSUBQ
    case 0x27: // MSUBI
    case 0x1D: // MAXI
    case 0x1E: // MULI
    case 0x1F: // MINII
    case 0x18: // MULx/y/z/w
    case 0x19:
    case 0x1A:
    case 0x1B: {
        const float scalar = (opcode == 0x1C || opcode == 0x20 || opcode == 0x24 ||
                              opcode == 0x21 || opcode == 0x25)
                                 ? state->q
                             : (opcode == 0x22 || opcode == 0x26 || opcode == 0x23 ||
                                opcode == 0x27 || opcode == 0x1D || opcode == 0x1E ||
                                opcode == 0x1F)
                                 ? state->i
                                 : target[opcode & 3u];
        const bool use_scalar = opcode <= 0x27 || opcode == 0x1D || opcode == 0x1E ||
                                opcode == 0x1F;
        const bool multiply = opcode == 0x1C || opcode == 0x1E || opcode == 0x1A ||
                              opcode == 0x1B || opcode == 0x2A || opcode == 0x21 ||
                              opcode == 0x23 || opcode == 0x29 || opcode == 0x2D ||
                              opcode == 0x25 || opcode == 0x27;
        const bool add = opcode == 0x20 || opcode == 0x22 || opcode == 0x28 ||
                         opcode == 0x2B || opcode == 0x29 || opcode == 0x21 ||
                         opcode == 0x23;
        for (uint32_t lane = 0; lane < 4; ++lane) {
            const float a = source[lane];
            const float b = use_scalar ? scalar : target[lane];
            if (opcode == 0x2B || opcode == 0x1D) result[lane] = std::max(a, b);
            else if (opcode == 0x2F || opcode == 0x1F) result[lane] = std::min(a, b);
            else if (multiply && add) result[lane] = state->acc[lane] + a * b;
            else if (multiply) result[lane] = opcode == 0x2D || opcode == 0x25 ||
                                                   opcode == 0x27
                                               ? state->acc[lane] - a * b
                                               : a * b;
            else if (add) result[lane] = a + b;
            else result[lane] = a - b;
        }
        if (opcode == 0x21 || opcode == 0x23 || opcode == 0x25 || opcode == 0x27 ||
            opcode == 0x29 || opcode == 0x2D)
            state->acc = result;
        update_float_flags(state, result, mask);
        vf_write(state, fd, result, mask);
        return true;
    }
    case 0x2E: // OPM-SUB
        result = {state->acc[0] - (source[1] * target[2] - source[2] * target[1]),
                  state->acc[1] - (source[2] * target[0] - source[0] * target[2]),
                  state->acc[2] - (source[0] * target[1] - source[1] * target[0]),
                  state->acc[3]};
        update_float_flags(state, result, mask);
        vf_write(state, fd, result, mask);
        return true;
    default:
        break;
    }

    if (opcode >= 0x3Cu) {
        const uint32_t special = (((word >> 6u) & 31u) << 2u) | (opcode & 3u);
        switch (special) {
        case 0x00: case 0x01: case 0x02: case 0x03: { // ADDAbc
            const uint32_t broadcast = special & 3u;
            for (uint32_t lane = 0; lane < 4; ++lane)
                if ((mask & (8u >> lane)) != 0)
                    state->acc[lane] = source[lane] + target[broadcast];
            return true;
        }
        case 0x04: case 0x05: case 0x06: case 0x07: { // SUBAbc
            const uint32_t broadcast = special & 3u;
            for (uint32_t lane = 0; lane < 4; ++lane)
                if ((mask & (8u >> lane)) != 0)
                    state->acc[lane] = source[lane] - target[broadcast];
            return true;
        }
        case 0x1F: // CLIPw.xyz vf[fs], vf[ft].w
            if (opcode == 0x3Fu && mask == 0x0Eu) {
                const float limit = std::fabs(target[3]);
                uint32_t current_clip = 0;
                for (uint32_t lane = 0; lane < 3; ++lane) {
                    if (source[lane] > limit) current_clip |= 1u << (lane * 2u);
                    if (source[lane] < -limit) current_clip |= 1u << (lane * 2u + 1u);
                }
                state->clip_flags = ((state->clip_flags << 6u) | current_clip) &
                                    0x00FFFFFFu;
                return true;
            }
            break;
        case 0x2E: // OPMULA: first half of the vector cross product.
            for (uint32_t lane = 0; lane < 3; ++lane) {
                const uint32_t a = (lane + 1u) % 3u;
                const uint32_t b = (lane + 2u) % 3u;
                state->acc[lane] = source[a] * target[b];
            }
            return true;
        case 0x08: case 0x09: case 0x0A: case 0x0B: { // MADDAbc
            const uint32_t broadcast = special & 3u;
            for (uint32_t lane = 0; lane < 4; ++lane)
                if ((mask & (8u >> lane)) != 0)
                    state->acc[lane] += source[lane] * target[broadcast];
            return true;
        }
        case 0x0C: case 0x0D: case 0x0E: case 0x0F: { // MSUBAbc
            const uint32_t broadcast = special & 3u;
            for (uint32_t lane = 0; lane < 4; ++lane)
                if ((mask & (8u >> lane)) != 0)
                    state->acc[lane] -= source[lane] * target[broadcast];
            return true;
        }
        case 0x18: case 0x19: case 0x1A: case 0x1B: { // MULAx/y/z/w
            const uint32_t broadcast = special & 3u;
            for (uint32_t lane = 0; lane < 4; ++lane)
                if ((mask & (8u >> lane)) != 0)
                    state->acc[lane] = source[lane] * target[broadcast];
            return true;
        }
        case 0x10: case 0x11: case 0x12: case 0x13:
        case 0x14: case 0x15: case 0x16: case 0x17: {
            const bool to_int = special >= 0x14;
            const uint32_t shift = to_int ? (special - 0x14u) * 4u
                                          : (special - 0x10u) * 4u;
            for (uint32_t lane = 0; lane < 4; ++lane) {
                if (to_int) {
                    const float scaled = std::ldexp(source[lane], static_cast<int>(shift));
                    const int32_t value = std::clamp(static_cast<int32_t>(scaled), -32768, 32767);
                    result[lane] = std::bit_cast<float>(static_cast<uint32_t>(value));
                } else {
                    const int32_t value = std::bit_cast<int32_t>(
                        std::bit_cast<uint32_t>(source[lane]));
                    result[lane] = static_cast<float>(value) / std::ldexp(1.0f, shift);
                }
            }
            vf_write(state, ft, result, mask);
            return true;
        }
        case 0x1D: // ABS
            for (uint32_t lane = 0; lane < 4; ++lane) result[lane] = std::fabs(source[lane]);
            vf_write(state, ft, result, mask);
            return true;
        case 0x28: { // ADDA
            for (uint32_t lane = 0; lane < 4; ++lane)
                if ((mask & (8u >> lane)) != 0)
                    state->acc[lane] = source[lane] + target[lane];
            return true;
        }
        case 0x29: { // MADDA
            for (uint32_t lane = 0; lane < 4; ++lane)
                if ((mask & (8u >> lane)) != 0)
                    state->acc[lane] += source[lane] * target[lane];
            return true;
        }
        case 0x2A: { // MULA
            for (uint32_t lane = 0; lane < 4; ++lane)
                if ((mask & (8u >> lane)) != 0)
                    state->acc[lane] = source[lane] * target[lane];
            return true;
        }
        case 0x2C: { // SUBA
            for (uint32_t lane = 0; lane < 4; ++lane)
                if ((mask & (8u >> lane)) != 0)
                    state->acc[lane] = source[lane] - target[lane];
            return true;
        }
        case 0x2D: { // MSUBA
            for (uint32_t lane = 0; lane < 4; ++lane)
                if ((mask & (8u >> lane)) != 0)
                    state->acc[lane] -= source[lane] * target[lane];
            return true;
        }
        case 0x30: // MOVE
            vf_write(state, ft, source, mask);
            return true;
        case 0x31: // MR32
            result = {source[1], source[2], source[3], source[0]};
            vf_write(state, ft, result, mask);
            return true;
        case 0x2F: // NOP
            return true;
        default:
            break;
        }
    }
    *unsupported = word;
    return false;
}

bool translate_lower(State* state, uint32_t word, uint32_t* next_pc,
                     uint32_t* unsupported, PendingKick* pending_kick) {
    if (word == 0 || word == 0x8000033Cu) return true;
    const uint32_t primary = word >> 25u;
    const uint32_t target = (word >> 16u) & 31u;
    const uint32_t source = (word >> 11u) & 31u;
    const uint32_t destination = (word >> 6u) & 31u;
    if (primary == 0x00u || primary == 0x01u) { // LQ / SQ
        const int32_t offset = sign_extend(word & 0x7FFu, 11);
        if (primary == 0x00u) {
            const uint32_t address = (vi_read(*state, source & 15u) + offset) & 0x3FFu;
            vf_write(state, target, data_read(address), (word >> 21u) & 15u);
        } else {
            // SQ uses the it field for the VI base and the is field for VF source;
            // LQ uses those fields in the opposite roles.
            const uint32_t address = (vi_read(*state, target & 15u) + offset) & 0x3FFu;
            const Vector value = vf_read(*state, source);
            const uint32_t mask = (word >> 21u) & 15u;
            Vector previous = data_read(address);
            for (uint32_t lane = 0; lane < 4; ++lane)
                if ((mask & (8u >> lane)) != 0) previous[lane] = value[lane];
            data_write(address, previous);
        }
        return true;
    }

    if (primary >= 0x04u && primary <= 0x07u) { // ILW / ISW / ILWR / ISWR
        const uint32_t base = source & 15u;
        const int32_t offset = primary < 0x06u ? sign_extend(word & 0x7FFu, 11) : 0;
        const uint32_t address = (vi_read(*state, base) + offset) & 0x3FFu;
        const uint32_t mask = (word >> 21u) & 15u;
        uint8_t* data = guest_address(kVu1Data + address * 16u);
        const bool load = primary == 0x04u || primary == 0x06u;
        for (uint32_t lane = 0; lane < 4; ++lane) {
            if ((mask & (8u >> lane)) == 0) continue;
            uint16_t value = 0;
            if (load) {
                std::memcpy(&value, data + lane * 4u, sizeof(value));
                vi_write(state, target, value);
            } else {
                value = vi_read(*state, target);
                std::memcpy(data + lane * 4u, &value, sizeof(value));
                const uint16_t zero = 0;
                std::memcpy(data + lane * 4u + 2u, &zero, sizeof(zero));
            }
        }
        return true;
    }

    if ((word >> 25u) == 0x40u) {
        const uint32_t function = word & 0x3Fu;
        const uint32_t field = (word >> 6u) & 31u;
        if ((word & 0x7FFu) == 0x67Cu) { // MFP.dest VF[ft], P
            const float value = state->p;
            vf_write(state, target, {value, value, value, value},
                     (word >> 21u) & 15u);
            return true;
        }
        if ((word & 0x7FFu) == 0x73Fu) { // ERLENG P, VF[fs]
            const Vector value = vf_read(*state, source);
            const float length_squared = value[0] * value[0] +
                                         value[1] * value[1] +
                                         value[2] * value[2];
            state->p = 1.0f / std::sqrt(length_squared);
            return true;
        }
        if ((word & 0x7FFu) == 0x7BDu) { // ERSQRT P, VF[fs][fsf]
            const uint32_t field = (word >> 21u) & 3u;
            const float value = vf_read(*state, source)[field];
            state->p = 1.0f / std::sqrt(std::fabs(value));
            return true;
        }
        if ((word & 0x7FFu) == 0x3BFu) { // WAITQ; scalar operations complete synchronously here
            return true;
        }
        if ((word & 0x7FFu) == 0x7BFu) { // WAITP; P results are synchronous in this translator
            return true;
        }
        if ((word & 0x7FFu) == 0x3BCu) { // DIV Q, VF[fs]fsf, VF[ft]ftf
            const uint32_t ftf = (word >> 23u) & 3u;
            const uint32_t fsf = (word >> 21u) & 3u;
            const float numerator = vf_read(*state, source)[fsf];
            const float denominator = vf_read(*state, target)[ftf];
            const bool numerator_zero = numerator == 0.0f;
            const bool denominator_zero = denominator == 0.0f;
            uint16_t divide_flags = 0;
            if (denominator_zero)
                divide_flags = numerator_zero ? 0x10u : 0x20u;
            state->status_flags = static_cast<uint16_t>(
                (state->status_flags & ~0x30u) | divide_flags |
                (divide_flags << 6u));
            state->q = numerator / denominator;
            return true;
        }
        switch (function) {
        case 0x30: // IADD
            if (destination != 0)
                vi_write(state, destination, static_cast<uint16_t>(vi_read(*state, source) + vi_read(*state, target)));
            return true;
        case 0x31: // ISUB
            if (destination != 0)
                vi_write(state, destination, static_cast<uint16_t>(vi_read(*state, source) - vi_read(*state, target)));
            return true;
        case 0x32: { // IADDI
            const uint32_t it = target;
            const int32_t immediate = sign_extend(field, 5);
            vi_write(state, it, static_cast<uint16_t>(vi_read(*state, source) + immediate));
            return true;
        }
        case 0x34: // IAND
            vi_write(state, destination, static_cast<uint16_t>(vi_read(*state, source) & vi_read(*state, target)));
            return true;
        case 0x35: // IOR
            vi_write(state, destination, static_cast<uint16_t>(vi_read(*state, source) | vi_read(*state, target)));
            return true;
        case 0x3C: case 0x3D: case 0x3E: case 0x3F:
            if (function == 0x3Cu && field == 0x0Cu) { // MOVE.dest ft, fs
                vf_write(state, target, vf_read(*state, source),
                         (word >> 21u) & 15u);
                return true;
            }
            if (function == 0x3Du && field == 0x0Cu) { // MR32.dest ft, fs
                const Vector input = vf_read(*state, source);
                vf_write(state, target,
                         {input[1], input[2], input[3], input[0]},
                         (word >> 21u) & 15u);
                return true;
            }
            if (field == 0x0Fu || field == 0x0Du) {
                const bool load = function == 0x3Cu || function == 0x3Eu;
                const uint32_t base = (load ? source : target) & 15u;
                const int32_t step = field == 0x0Fu ? 1 : -1;
                const uint32_t address = (vi_read(*state, base) + (step < 0 ? -1 : 0)) & 0x3FFu;
                if (load)
                    vf_write(state, target, data_read(address), (word >> 21u) & 15u);
                else {
                    const Vector value = vf_read(*state, source);
                    Vector previous = data_read(address);
                    const uint32_t mask = (word >> 21u) & 15u;
                    for (uint32_t lane = 0; lane < 4; ++lane)
                        if ((mask & (8u >> lane)) != 0) previous[lane] = value[lane];
                    data_write(address, previous);
                }
                vi_write(state, base, static_cast<uint16_t>(vi_read(*state, base) + step));
                return true;
            }
            if (function == 0x3Cu && field == 0x1Bu) { // XGKICK
                const uint32_t address = vi_read(*state, source);
                if (pending_kick != nullptr && pending_kick->active)
                    dispatch_pending_kick(pending_kick);
                if (pending_kick != nullptr) {
                    pending_kick->active = true;
                    pending_kick->address = address;
                    pending_kick->pc = state->pc;
                    pending_kick->vi4 = vi_read(*state, 4u);
                    pending_kick->vi3 = vi_read(*state, 3u);
                    pending_kick->top = *reinterpret_cast<const uint32_t*>(
                        guest_address(kVif1Top));
                    pending_kick->tops = *reinterpret_cast<const uint32_t*>(
                        guest_address(0x10003CC0u));
                    pending_kick->vf7 = vf_read(*state, 7u);
                    pending_kick->delay = 2;
                }
                return true;
            }
            if ((function == 0x3Cu || function == 0x3Du) && field == 0x1Au) {
                uint32_t value = state->itop;
                if (function == 0x3Cu) {
                    std::memcpy(&value, guest_address(kVif1Top), sizeof(value));
                    value &= 0x3FFu;
                }
                vi_write(state, target, static_cast<uint16_t>(value));
                return true;
            }
            break;
        default:
            break;
        }
    }

    if (primary >= 0x10u && primary <= 0x1Cu) {
        const uint32_t immediate12 = (((word >> 21u) & 1u) << 11u) | (word & 0x7FFu);
        const uint32_t immediate24 = word & 0x00FFFFFFu;
        switch (primary) {
        case 0x10: // FCEQ
            vi_write(state, 1, static_cast<uint16_t>(state->clip_flags == immediate24));
            return true;
        case 0x11: // FCSET
            state->clip_flags = immediate24;
            return true;
        case 0x12: // FCAND
            vi_write(state, 1, static_cast<uint16_t>((state->clip_flags & immediate24) != 0));
            return true;
        case 0x13: // FCOR
            vi_write(state, 1, static_cast<uint16_t>(
                ((state->clip_flags | immediate24) & 0x00FFFFFFu) == 0x00FFFFFFu));
            return true;
        case 0x14: // FSEQ
            vi_write(state, target, static_cast<uint16_t>(state->status_flags == immediate12));
            return true;
        case 0x15: // FSSET: set sticky flags
            state->status_flags = static_cast<uint16_t>(
                (state->status_flags & 0x003Fu) | (immediate12 & 0x0FC0u));
            return true;
        case 0x16: // FSAND
            vi_write(state, target, static_cast<uint16_t>(state->status_flags & immediate12));
            return true;
        case 0x17: // FSOR
            vi_write(state, target, static_cast<uint16_t>(state->status_flags | immediate12));
            return true;
        case 0x18: // FMEQ
            vi_write(state, target, static_cast<uint16_t>(state->mac_flags == vi_read(*state, source)));
            return true;
        case 0x1A: // FMAND
            vi_write(state, target, static_cast<uint16_t>(state->mac_flags & vi_read(*state, source)));
            return true;
        case 0x1B: // FMOR
            vi_write(state, target, static_cast<uint16_t>(state->mac_flags | vi_read(*state, source)));
            return true;
        case 0x1C: // FCGET
            vi_write(state, target, static_cast<uint16_t>(state->clip_flags & 0xFFFu));
            return true;
        default:
            break;
        }
    }

    if (primary == 0x08u || primary == 0x09u) { // IADDIU / ISUBIU
        const uint32_t immediate = (((word >> 21u) & 15u) << 11u) | (word & 0x7FFu);
        if (target != 0)
            vi_write(state, target, static_cast<uint16_t>(vi_read(*state, source) +
                (primary == 0x08u ? static_cast<int32_t>(immediate)
                                  : -static_cast<int32_t>(immediate))));
        return true;
    }
    if (primary == 0x20u || primary == 0x21u) { // B / BAL
        const int32_t offset = sign_extend(word & 0x7FFu, 11);
        if (primary == 0x21u)
            vi_write(state, target, static_cast<uint16_t>(state->pc + 2u));
        *next_pc = static_cast<uint32_t>(static_cast<int32_t>(*next_pc) + offset) & 0x7FFu;
        return true;
    }
    if (primary == 0x24u || primary == 0x25u) { // JR / JALR
        const uint16_t jump_target = vi_read(*state, source);
        if (primary == 0x25u)
            vi_write(state, target, static_cast<uint16_t>(state->pc + 2u));
        *next_pc = jump_target & 0x7FFu;
        return true;
    }
    if (primary >= 0x28u && primary <= 0x2Fu) {
        const int32_t offset = sign_extend(word & 0x7FFu, 11);
        bool taken = false;
        switch (primary) {
        case 0x28: taken = vi_read(*state, source) == vi_read(*state, target); break; // IBEQ
        case 0x29: taken = vi_read(*state, source) != vi_read(*state, target); break; // IBNE
        case 0x2C: taken = static_cast<int16_t>(vi_read(*state, source)) < 0; break; // IBLTZ
        case 0x2D: taken = static_cast<int16_t>(vi_read(*state, source)) > 0; break; // IBGTZ
        case 0x2E: taken = static_cast<int16_t>(vi_read(*state, source)) <= 0; break; // IBLEZ
        case 0x2F: taken = static_cast<int16_t>(vi_read(*state, source)) >= 0; break; // IBGEZ
        default: break;
        }
        if (taken) *next_pc = static_cast<uint32_t>(static_cast<int32_t>(*next_pc) +
                                                    offset) & 0x7FFu;
        return true;
    }

    *unsupported = word;
    return false;
}

} // namespace

extern "C" void openrac_guest_vu1_set_itop(uint32_t value) {
    std::lock_guard lock(g_vu_mutex);
    g_state.itop = static_cast<uint16_t>(value & 0x3FFu);
}

extern "C" int openrac_guest_vu1_execute(uint32_t entry, uint32_t* unsupported_word,
                                           uint32_t* stopped_pc,
                                           uint32_t* stopped_pipeline,
                                           uint32_t* paired_word) {
    if (!guest_memory_available() || unsupported_word == nullptr ||
        stopped_pc == nullptr || stopped_pipeline == nullptr || paired_word == nullptr) return 0;
    std::lock_guard lock(g_vu_mutex);
    uint32_t pc = entry == 0xFFFFFFFFu ? g_state.pc : (entry & 0x7FFu);
    bool end_delay = false;
    bool branch_delay = false;
    uint32_t branch_target = 0;
    PendingKick pending_kick{};
    static std::array<std::atomic<uint32_t>, 2> loop_trace_count{};
    const char* loop_trace_setting = std::getenv("OPENRAC_TRACE_VU1_LOOP");
    const bool loop_trace_enabled = loop_trace_setting != nullptr &&
        loop_trace_setting[0] != '\0' && loop_trace_setting[0] != '0';
    static std::atomic<uint32_t> world_iteration_notice{0};
    bool trace_world_iteration = false;
    uint32_t instruction_limit = kMaxInstructions;
    if (const char* setting = std::getenv("OPENRAC_VU1_CYCLE_LIMIT")) {
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(setting, &end, 10);
        if (end != setting && *end == '\0' && parsed > 0 && parsed <= 4'000'000ul)
            instruction_limit = static_cast<uint32_t>(parsed);
    }
    if (entry == 0x26Du) {
        if (const char* setting = std::getenv("OPENRAC_VU1_CYCLE_LIMIT_26D")) {
            char* end = nullptr;
            const unsigned long parsed = std::strtoul(setting, &end, 10);
            if (end != setting && *end == '\0' && parsed > 0 && parsed <= 4'000'000ul)
                instruction_limit = static_cast<uint32_t>(parsed);
        }
    }
    struct RecentInstruction { uint32_t pc; uint32_t lower; uint32_t upper; };
    std::array<RecentInstruction, 16> recent{};
    for (uint32_t cycle = 0; cycle < instruction_limit; ++cycle) {
        if (pc >= kVu1Doublewords) {
            *unsupported_word = 0xFFFFFFFFu;
            *paired_word = 0xFFFFFFFFu;
            *stopped_pc = pc;
            *stopped_pipeline = 0;
            return 0;
        }
        uint32_t lower = 0, upper = 0;
        const uint8_t* instruction = guest_address(kVu1Micro + pc * 8u);
        std::memcpy(&lower, instruction, sizeof(lower));
        std::memcpy(&upper, instruction + 4u, sizeof(upper));
        recent[cycle % recent.size()] = {pc, lower, upper};
        const uint32_t current_pc = pc;
        uint32_t next_pc = current_pc + 1u;
        State before = g_state;
        before.pc = current_pc;
        const uint16_t vi6_before = vi_read(before, 6u);
        const uint16_t vi7_before = vi_read(before, 7u);
        State upper_result = before;
        State lower_result = before;
        if (!translate_upper(&upper_result, upper, unsupported_word)) {
            dispatch_pending_kick(&pending_kick);
            *paired_word = lower;
            *stopped_pc = current_pc;
            *stopped_pipeline = 0;
            return 0;
        }
        if ((upper & (1u << 31u)) != 0) {
            upper_result.i = std::bit_cast<float>(lower);
        } else if (!translate_lower(&lower_result, lower, &next_pc,
                                    unsupported_word, &pending_kick)) {
            dispatch_pending_kick(&pending_kick);
            *paired_word = upper;
            *stopped_pc = current_pc;
            const bool xgkick = (lower >> 25u) == 0x40u &&
                                (lower & 0x3Fu) == 0x3Cu &&
                                ((lower >> 6u) & 31u) == 0x1Bu;
            *stopped_pipeline = xgkick ? 2u : 1u;
            if (!g_vu_context_notice.test_and_set(std::memory_order_relaxed)) {
                std::ostringstream context;
                const uint32_t first_cycle = cycle >= recent.size() - 1u
                    ? cycle - static_cast<uint32_t>(recent.size() - 1u) : 0u;
                for (uint32_t trace_cycle = first_cycle; trace_cycle <= cycle; ++trace_cycle) {
                    const RecentInstruction& recent_instruction =
                        recent[trace_cycle % recent.size()];
                    context << " pc=" << std::hex << recent_instruction.pc
                            << " [" << recent_instruction.lower << '/'
                            << recent_instruction.upper << ']';
                }
                openrac::log::warn("native VU1 executed path to fault:{}", context.str());
            }
            return 0;
        }
        g_state = upper_result;
        for (uint32_t reg = 0; reg < g_state.vf.size(); ++reg) {
            for (uint32_t lane = 0; lane < 4; ++lane) {
                if (lower_result.vf[reg][lane] != before.vf[reg][lane])
                    g_state.vf[reg][lane] = lower_result.vf[reg][lane];
            }
        }
        g_state.vi = lower_result.vi;
        g_state.p = lower_result.p;
        g_state.q = lower_result.q;
        if (lower_result.status_flags != before.status_flags)
            g_state.status_flags = lower_result.status_flags;

        // OpenRAC's VU interpreter sends XGKICK after the next instruction
        // pair. Keep that delay in this port-only VU path so the GIF packet
        // sees the data produced by the intervening guest instructions.
        if (pending_kick.active && pending_kick.delay > 0 &&
            --pending_kick.delay == 0)
            dispatch_pending_kick(&pending_kick);
        if (lower_result.clip_flags != before.clip_flags)
            g_state.clip_flags = lower_result.clip_flags;
        g_state.pc = next_pc;

        static std::atomic<uint32_t> kick_setup_notice_count{0};
        const char* kick_setup_trace = std::getenv("OPENRAC_TRACE_VU1_SETUP");
        if (kick_setup_trace != nullptr && kick_setup_trace[0] != '\0' &&
            kick_setup_trace[0] != '0' && current_pc >= 0x63u && current_pc <= 0x6Du &&
            kick_setup_notice_count.fetch_add(1, std::memory_order_relaxed) < 24u) {
            const uint32_t before_address = vi6_before & 0x3FFu;
            const uint32_t after_address = vi_read(g_state, 6u) & 0x3FFu;
            const uint32_t before_address7 = vi7_before & 0x3FFu;
            const uint32_t after_address7 = vi_read(g_state, 7u) & 0x3FFu;
            uint64_t before_data = 0;
            uint64_t after_data = 0;
            uint64_t before_data7 = 0;
            uint64_t after_data7 = 0;
            std::memcpy(&before_data,
                        guest_address(kVu1Data + before_address * 16u),
                        sizeof(before_data));
            std::memcpy(&after_data,
                        guest_address(kVu1Data + after_address * 16u),
                        sizeof(after_data));
            std::memcpy(&before_data7,
                        guest_address(kVu1Data + before_address7 * 16u),
                        sizeof(before_data7));
            std::memcpy(&after_data7,
                        guest_address(kVu1Data + after_address7 * 16u),
                        sizeof(after_data7));
            openrac::log::info(
                "native VU1 kick setup entry={:#x} pc={:#x} lower={:#010x} VI4={} VI6={}->{} data[VI6]={:#018x}->{:#018x} VI7={}->{} data[VI7]={:#018x}->{:#018x}",
                entry, current_pc, lower, vi_read(before, 4u), vi6_before,
                vi_read(g_state, 6u), before_data, after_data, vi7_before,
                vi_read(g_state, 7u), before_data7, after_data7);
        }

        if (branch_delay) {
            pc = branch_target;
            branch_delay = false;
        } else {
            pc = next_pc;
        }
        if ((upper & (1u << 30u)) != 0) {
            if (end_delay) {
                g_state.pc = pc;
                dispatch_pending_kick(&pending_kick);
                return 1;
            }
            end_delay = true;
        } else if (end_delay) {
            g_state.pc = pc;
            dispatch_pending_kick(&pending_kick);
            return 1;
        }
        if (next_pc != current_pc + 1u) {
            branch_target = next_pc;
            branch_delay = true;
            pc = current_pc + 1u;
        }
        static std::atomic<uint32_t> range_trace_count{0};
        const char* range_trace = std::getenv("OPENRAC_TRACE_VU1_401_RANGE");
        if (entry == 0x401u && range_trace != nullptr &&
            range_trace[0] != '\0' && range_trace[0] != '0' &&
            current_pc >= 0x3D4u && current_pc <= 0x410u &&
            range_trace_count.fetch_add(1, std::memory_order_relaxed) < 8192u) {
            openrac::log::info(
                "native VU1 401 range cycle={} pc={:#x} lower={:#010x} upper={:#010x} next_pc={:#x} branch_target={:#x} VI3={}->{} VI5={}->{} VI6={}->{} VI8={}->{} VI9={}->{} VI10={}->{} branch_delay={} end_delay={}",
                cycle, current_pc, lower, upper, next_pc, branch_target,
                vi_read(before, 3u), vi_read(g_state, 3u),
                vi_read(before, 5u), vi_read(g_state, 5u),
                vi_read(before, 6u), vi_read(g_state, 6u),
                vi_read(before, 8u), vi_read(g_state, 8u),
                vi_read(before, 9u), vi_read(g_state, 9u),
                vi_read(before, 10u), vi_read(g_state, 10u),
                branch_delay ? 1u : 0u, end_delay ? 1u : 0u);
        }
        const bool trace_first_loop = entry == 0x242u &&
            (current_pc == 0x243u || current_pc == 0x253u || current_pc == 0x258u ||
             current_pc == 0x261u || current_pc == 0x263u || current_pc == 0x264u ||
             current_pc == 0x265u || current_pc == 0x26Eu || current_pc == 0x270u ||
             current_pc == 0x272u || current_pc == 0x273u || current_pc == 0x274u);
        const bool trace_later_loop = entry == 0x401u &&
            ((current_pc >= 0x401u && current_pc <= 0x410u) ||
             current_pc == 0x3E5u || current_pc == 0x3E6u || current_pc == 0x3EDu ||
             current_pc == 0x3EEu || current_pc == 0x3FDu || current_pc == 0x3FEu ||
             current_pc == 0x3FFu);
        const bool trace_sample = cycle < 128u ||
            (trace_later_loop && current_pc == 0x3FDu) || cycle % 256u == 0;
        const std::size_t trace_slot = trace_later_loop ? 1u : 0u;
        if (loop_trace_enabled && (trace_first_loop || trace_later_loop) && trace_sample &&
            loop_trace_count[trace_slot].fetch_add(1, std::memory_order_relaxed) < 384u) {
            openrac::log::info(
                "native VU1 loop entry={:#x} cycle={} pc={:#x} lower={:#010x} upper={:#010x} next={:#x} VI4={} VI5={} VI6={} VI8={} VI9={} VI10={} VI14={} VI15={} branch_delay={} end_delay={}",
                entry, cycle, current_pc, lower, upper, pc, vi_read(g_state, 4u),
                vi_read(g_state, 5u), vi_read(g_state, 6u), vi_read(g_state, 8u),
                vi_read(g_state, 9u), vi_read(g_state, 10u), vi_read(g_state, 14u),
                vi_read(g_state, 15u), branch_delay ? 1u : 0u, end_delay ? 1u : 0u);
        }
        if (loop_trace_enabled && entry == 0x26Du &&
            current_pc == 0x2A9u && !trace_world_iteration &&
            world_iteration_notice.fetch_add(1, std::memory_order_relaxed) < 4u)
            trace_world_iteration = true;
        if (trace_world_iteration) {
            openrac::log::info(
                "native VU1 world loop entry={:#x} cycle={} pc={:#x} lower={:#010x} upper={:#010x} next={:#x} VI1={} VI2={} VI5={} VI6={} VI8={} VI9={} VI13={} VI14={} VI15={} branch_delay={}",
                entry, cycle, current_pc, lower, upper, pc, vi_read(g_state, 1u),
                vi_read(g_state, 2u), vi_read(g_state, 5u), vi_read(g_state, 6u),
                vi_read(g_state, 8u), vi_read(g_state, 9u), vi_read(g_state, 13u),
                vi_read(g_state, 14u), vi_read(g_state, 15u), branch_delay ? 1u : 0u);
            if (current_pc == 0x2D7u) trace_world_iteration = false;
        }
    }
    static std::atomic<uint32_t> budget_trace_count{0};
    if (std::getenv("OPENRAC_TRACE_VU1_LIMIT") != nullptr &&
        budget_trace_count.fetch_add(1, std::memory_order_relaxed) < 8u) {
        std::ostringstream context;
        const uint32_t first_cycle = instruction_limit >= recent.size()
            ? instruction_limit - static_cast<uint32_t>(recent.size()) : 0u;
        for (uint32_t trace_cycle = first_cycle; trace_cycle < instruction_limit;
             ++trace_cycle) {
            const RecentInstruction& instruction = recent[trace_cycle % recent.size()];
            context << " pc=" << std::hex << instruction.pc << " ["
                    << instruction.lower << '/' << instruction.upper << ']';
        }
        openrac::log::warn(
            "native VU1 instruction budget exhausted: entry={:#x} cycles={} stopped_pc={:#x} VI5={} VI6={} VI8={} VI9={} VI10={} VI14={} VI15={} recent:{}",
            entry, instruction_limit, pc, vi_read(g_state, 5u), vi_read(g_state, 6u),
            vi_read(g_state, 8u), vi_read(g_state, 9u), vi_read(g_state, 10u),
            vi_read(g_state, 14u), vi_read(g_state, 15u), context.str());
    }
    *unsupported_word = 0xFFFFFFFFu;
    *paired_word = 0xFFFFFFFFu;
    *stopped_pc = pc;
    *stopped_pipeline = 0;
    dispatch_pending_kick(&pending_kick);
    return 0;
}
