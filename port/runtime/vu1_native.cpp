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
#include <cstring>
#include <mutex>
#include <sstream>
#include <vector>

#include "common/log.h"

namespace {
constexpr uint32_t kVu1Data = 0x1100C000u;
constexpr uint32_t kVu1Micro = 0x11008000u;
constexpr uint32_t kVif1Tops = 0x10003CC0u;
constexpr size_t kVu1Doublewords = 2048;
constexpr size_t kVu1Quadwords = 1024;
constexpr uint32_t kMaxInstructions = 4096;

using Vector = std::array<float, 4>;

struct State {
    std::array<Vector, 32> vf{};
    std::array<uint16_t, 16> vi{};
    Vector acc{};
    float q = 0.0f;
    float i = 0.0f;
    uint16_t itop = 0;
    uint16_t mac_flags = 0;
    uint16_t status_flags = 0;
    uint32_t clip_flags = 0;
    uint32_t pc = 0;
};

std::mutex g_vu_mutex;
State g_state;
std::atomic_flag g_vu_context_notice = ATOMIC_FLAG_INIT;
std::atomic_flag g_vu_kick_notice = ATOMIC_FLAG_INIT;

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
    std::memcpy(value.data(), openrac_guest_base + kVu1Data + address * 16u, 16u);
    return value;
}

void data_write(uint32_t address, const Vector& value) {
    address &= static_cast<uint32_t>(kVu1Quadwords - 1u);
    std::memcpy(openrac_guest_base + kVu1Data + address * 16u, value.data(), 16u);
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

bool kick_gif(uint32_t address, uint32_t* failure, uint64_t* first_tag) {
    std::vector<uint8_t> packet;
    size_t cursor = static_cast<size_t>(address & 0x3FFu) * 16u;
    const size_t memory_end = kVu1Quadwords * 16u;
    if (failure != nullptr) *failure = 0;
    if (first_tag != nullptr) *first_tag = 0;
    while (cursor + 16u <= memory_end) {
        const uint8_t* tag = openrac_guest_base + kVu1Data + cursor;
        uint64_t low = 0;
        std::memcpy(&low, tag, sizeof(low));
        if (first_tag != nullptr && cursor == static_cast<size_t>(address & 0x3FFu) * 16u)
            *first_tag = low;
        const uint32_t loops = static_cast<uint32_t>(low & 0x7FFFu);
        const uint32_t format = static_cast<uint32_t>((low >> 58u) & 3u);
        uint32_t registers = static_cast<uint32_t>((low >> 60u) & 0xFu);
        if (registers == 0) registers = 16;
        size_t payload_qwords = 0;
        if (format == 0) payload_qwords = static_cast<size_t>(loops) * registers;
        else if (format == 1)
            payload_qwords = (static_cast<size_t>(loops) * registers + 1u) / 2u;
        else payload_qwords = loops;
        if (payload_qwords > kVu1Quadwords ||
            cursor + (payload_qwords + 1u) * 16u > memory_end) {
            if (failure != nullptr) *failure = 1;
            return false;
        }
        const size_t bytes = (payload_qwords + 1u) * 16u;
        packet.insert(packet.end(), tag, tag + bytes);
        cursor += bytes;
        if (((low >> 15u) & 1u) != 0) {
            openrac_guest_gif_stream_write(packet.data(), packet.size());
            return true;
        }
    }
    if (failure != nullptr) *failure = 2;
    return false;
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
        case 0x08: case 0x09: case 0x0A: case 0x0B: { // MADDAx/y/z/w
            const uint32_t lane = special - 0x08u;
            state->acc[lane] += source[lane] * target[lane];
            return true;
        }
        case 0x0C: case 0x0D: case 0x0E: case 0x0F: { // MSUBAx/y/z/w
            const uint32_t lane = special - 0x0Cu;
            state->acc[lane] -= source[lane] * target[lane];
            return true;
        }
        case 0x18: case 0x19: case 0x1A: case 0x1B: { // MULAx/y/z/w
            const uint32_t lane = special - 0x18u;
            state->acc[lane] = source[lane] * target[lane];
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
                     uint32_t* unsupported) {
    if (word == 0 || word == 0x8000033Cu) return true;
    const uint32_t primary = word >> 25u;
    const uint32_t target = (word >> 16u) & 31u;
    const uint32_t source = (word >> 11u) & 31u;
    const uint32_t destination = (word >> 6u) & 31u;
    if (primary == 0x00u || primary == 0x01u) { // LQ / SQ
        const uint32_t base = source & 15u;
        const int32_t offset = sign_extend(word & 0x7FFu, 11);
        const uint32_t address = (vi_read(*state, base) + offset) & 0x3FFu;
        if (primary == 0x00u)
            vf_write(state, target, data_read(address), (word >> 21u) & 15u);
        else data_write(address, vf_read(*state, target));
        return true;
    }

    if (primary == 0x04u || primary == 0x05u) { // ILW / ISW
        const uint32_t base = source & 15u;
        const int32_t offset = sign_extend(word & 0x7FFu, 11);
        const uint32_t address = (vi_read(*state, base) + offset) & 0x3FFu;
        Vector value = data_read(address);
        const uint32_t mask = (word >> 21u) & 15u;
        if (primary == 0x04u) {
            vf_write(state, target, value, mask);
        } else {
            value = vf_read(*state, target);
            for (uint32_t lane = 0; lane < 4; ++lane) {
                if ((mask & (8u >> lane)) == 0) continue;
                std::memcpy(openrac_guest_base + kVu1Data + address * 16u + lane * 4u,
                            &value[lane], sizeof(float));
            }
        }
        return true;
    }

    if ((word >> 25u) == 0x40u) {
        const uint32_t function = word & 0x3Fu;
        const uint32_t field = (word >> 6u) & 31u;
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
            if (field == 0x0Fu || field == 0x0Du) {
                const uint32_t base = source & 15u;
                const int32_t step = field == 0x0Fu ? 1 : -1;
                const uint32_t address = (vi_read(*state, base) + (step < 0 ? -1 : 0)) & 0x3FFu;
                if (function == 0x3Cu || function == 0x3Eu)
                    vf_write(state, target, data_read(address), (word >> 21u) & 15u);
                else data_write(address, vf_read(*state, target));
                vi_write(state, base, static_cast<uint16_t>(vi_read(*state, base) + step));
                return true;
            }
            if (function == 0x3Cu && field == 0x1Bu) { // XGKICK
                uint32_t failure = 0;
                uint64_t first_tag = 0;
                const uint32_t address = vi_read(*state, source);
                if (!kick_gif(address, &failure, &first_tag)) {
                    if (!g_vu_kick_notice.test_and_set(std::memory_order_relaxed))
                        openrac::log::warn(
                            "native VU1 XGKICK failed: VI{}={} first_tag={:#018x} reason={}",
                            source & 15u, address, first_tag,
                            failure == 1 ? "tag payload exceeds VU1 data memory" : "missing EOP");
                    *unsupported = word;
                    return false;
                }
                return true;
            }
            if ((function == 0x3Cu || function == 0x3Du) && field == 0x1Au) {
                uint32_t value = state->itop;
                if (function == 0x3Cu) {
                    std::memcpy(&value, openrac_guest_base + kVif1Tops, sizeof(value));
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
        if (primary == 0x25u)
            vi_write(state, target, static_cast<uint16_t>(state->pc + 2u));
        *next_pc = vi_read(*state, source) & 0x7FFu;
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
    if (openrac_guest_base == nullptr || unsupported_word == nullptr ||
        stopped_pc == nullptr || stopped_pipeline == nullptr || paired_word == nullptr) return 0;
    std::lock_guard lock(g_vu_mutex);
    uint32_t pc = entry == 0xFFFFFFFFu ? g_state.pc : (entry & 0x7FFu);
    bool end_delay = false;
    bool branch_delay = false;
    uint32_t branch_target = 0;
    struct RecentInstruction { uint32_t pc; uint32_t lower; uint32_t upper; };
    std::array<RecentInstruction, 16> recent{};
    for (uint32_t cycle = 0; cycle < kMaxInstructions; ++cycle) {
        if (pc >= kVu1Doublewords) {
            *unsupported_word = 0xFFFFFFFFu;
            *paired_word = 0xFFFFFFFFu;
            *stopped_pc = pc;
            *stopped_pipeline = 0;
            return 0;
        }
        uint32_t lower = 0, upper = 0;
        const uint8_t* instruction = openrac_guest_base + kVu1Micro + pc * 8u;
        std::memcpy(&lower, instruction, sizeof(lower));
        std::memcpy(&upper, instruction + 4u, sizeof(upper));
        recent[cycle % recent.size()] = {pc, lower, upper};
        const uint32_t current_pc = pc;
        uint32_t next_pc = current_pc + 1u;
        State before = g_state;
        before.pc = current_pc;
        State upper_result = before;
        State lower_result = before;
        if (!translate_upper(&upper_result, upper, unsupported_word)) {
            *paired_word = lower;
            *stopped_pc = current_pc;
            *stopped_pipeline = 0;
            return 0;
        }
        if ((upper & (1u << 31u)) != 0) {
            upper_result.i = std::bit_cast<float>(lower);
        } else if (!translate_lower(&lower_result, lower, &next_pc, unsupported_word)) {
            *paired_word = upper;
            *stopped_pc = current_pc;
            *stopped_pipeline = 1;
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
        if (lower_result.status_flags != before.status_flags)
            g_state.status_flags = lower_result.status_flags;
        if (lower_result.clip_flags != before.clip_flags)
            g_state.clip_flags = lower_result.clip_flags;
        g_state.pc = next_pc;

        if (branch_delay) {
            pc = branch_target;
            branch_delay = false;
        } else {
            pc = next_pc;
        }
        if ((upper & (1u << 30u)) != 0) {
            if (end_delay) {
                g_state.pc = pc;
                return 1;
            }
            end_delay = true;
        } else if (end_delay) {
            g_state.pc = pc;
            return 1;
        }
        if (next_pc != current_pc + 1u) {
            branch_target = next_pc;
            branch_delay = true;
            pc = current_pc + 1u;
        }
    }
    *unsupported_word = 0xFFFFFFFFu;
    *paired_word = 0xFFFFFFFFu;
    *stopped_pc = pc;
    *stopped_pipeline = 0;
    return 0;
}
