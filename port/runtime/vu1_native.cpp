/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Host translation of the VU1 display-list subset submitted by Sly. */
#include "openrac/native_vu1.h"

#include "openrac/guest.h"
#include "openrac/native_gs.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
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
thread_local uint32_t g_vu_trace_submission_id = 0xFFFFFFFFu;
thread_local uint32_t g_vu_trace_vif_command_offset = 0xFFFFFFFFu;

bool trace_vu1_xgkick_context_matches();

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

bool trace_xgkick_data_target(uint32_t address) {
    static const bool enabled = [] {
        const char* setting = std::getenv("OPENRAC_TRACE_VU1_XGKICK_DATA");
        return setting != nullptr && setting[0] != '\0' && setting[0] != '0';
    }();
    static const std::array<uint32_t, 2> selected_range = [] {
        const char* setting = std::getenv("OPENRAC_TRACE_VU1_XGKICK_DATA_QWORD");
        if (setting == nullptr) return std::array<uint32_t, 2>{0u, 0u};
        char* end = nullptr;
        const unsigned long first = std::strtoul(setting, &end, 0);
        if (end == setting || first >= kVu1Quadwords)
            return std::array<uint32_t, 2>{0u, 0u};
        unsigned long last = first;
        if (*end == '-') {
            char* range_end = nullptr;
            last = std::strtoul(end + 1, &range_end, 0);
            if (range_end == end + 1 || *range_end != '\0' || last >= kVu1Quadwords || last < first)
                return std::array<uint32_t, 2>{0u, 0u};
        } else if (*end != '\0') {
            return std::array<uint32_t, 2>{0u, 0u};
        }
        return std::array<uint32_t, 2>{static_cast<uint32_t>(first),
                                       static_cast<uint32_t>(last)};
    }();
    return enabled && trace_vu1_xgkick_context_matches() && (selected_range[0] == 0u
        ? address == 27u || address == 35u
        : address >= selected_range[0] && address <= selected_range[1]);
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
thread_local uint32_t g_vu_trace_entry = 0xFFFFFFFFu;
thread_local uint32_t g_vu_trace_target_submission_id = 0xFFFFFFFFu;
thread_local uint32_t g_vu_trace_target_stream_begin = 0xFFFFFFFFu;
thread_local uint32_t g_vu_trace_target_stream_end = 0xFFFFFFFFu;

bool trace_vu1_xgkick_context_matches() {
    bool has_filter = false;
    bool matches = true;
    if (const char* setting = std::getenv("OPENRAC_TRACE_XGKICK_SUBMISSION")) {
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(setting, &end, 0);
        if (end != setting && *end == '\0' && parsed <= 0xFFFFFFFFul) {
            has_filter = true;
            matches = matches && parsed == g_vu_trace_submission_id;
        }
    }
    if (const char* setting = std::getenv("OPENRAC_TRACE_XGKICK_MSCAL_OFFSETS")) {
        const char* cursor = setting;
        bool valid_value = false;
        bool offset_matches = false;
        while (*cursor != '\0') {
            while (*cursor == ',' || *cursor == ' ' || *cursor == '\t') ++cursor;
            if (*cursor == '\0') break;
            char* end = nullptr;
            const unsigned long parsed = std::strtoul(cursor, &end, 0);
            if (end == cursor) {
                while (*cursor != '\0' && *cursor != ',') ++cursor;
                continue;
            }
            if (parsed <= 0xFFFFFFFFul) {
                valid_value = true;
                if (parsed == g_vu_trace_vif_command_offset)
                    offset_matches = true;
            }
            cursor = end;
            while (*cursor != '\0' && *cursor != ',') ++cursor;
        }
        if (valid_value) {
            has_filter = true;
            matches = matches && offset_matches;
        }
    }
    if (const char* setting = std::getenv("OPENRAC_TRACE_VIF_DMA_ADDRESS")) {
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(setting, &end, 0);
        if (end != setting && *end == '\0' && parsed <= 0xFFFFFFFFul) {
            has_filter = true;
            matches = matches && g_vu_trace_target_submission_id != 0xFFFFFFFFu &&
                g_vu_trace_submission_id == g_vu_trace_target_submission_id &&
                g_vu_trace_target_stream_begin != 0xFFFFFFFFu &&
                g_vu_trace_target_stream_end != 0xFFFFFFFFu &&
                g_vu_trace_vif_command_offset >= g_vu_trace_target_stream_begin &&
                g_vu_trace_vif_command_offset < g_vu_trace_target_stream_end;
        }
    }
    return !has_filter || matches;
}

bool trace_vu1_read_target(uint32_t address) {
    static const std::array<uint32_t, 2> selected_range = [] {
        const char* setting = std::getenv("OPENRAC_TRACE_VU1_READ_QWORD");
        if (setting == nullptr) return std::array<uint32_t, 2>{0xFFFFFFFFu, 0xFFFFFFFFu};
        char* end = nullptr;
        const unsigned long first = std::strtoul(setting, &end, 0);
        if (end == setting || first >= kVu1Quadwords)
            return std::array<uint32_t, 2>{0xFFFFFFFFu, 0xFFFFFFFFu};
        unsigned long last = first;
        if (*end == '-') {
            char* range_end = nullptr;
            last = std::strtoul(end + 1, &range_end, 0);
            if (range_end == end + 1 || *range_end != '\0' ||
                last >= kVu1Quadwords || last < first)
                return std::array<uint32_t, 2>{0xFFFFFFFFu, 0xFFFFFFFFu};
        } else if (*end != '\0') {
            return std::array<uint32_t, 2>{0xFFFFFFFFu, 0xFFFFFFFFu};
        }
        return std::array<uint32_t, 2>{static_cast<uint32_t>(first),
                                       static_cast<uint32_t>(last)};
    }();
    return address >= selected_range[0] && address <= selected_range[1] &&
        trace_vu1_xgkick_context_matches();
}

Vector vf_read(const State& state, uint32_t index) {
    if (index == 0) return {0.0f, 0.0f, 0.0f, 1.0f};
    return state.vf[index & 31u];
}

// VU values are IEEE-shaped, but the VU does not implement IEEE denormals,
// infinities, or NaNs. Keep register loads/stores bit exact; normalize only
// when a value enters arithmetic, then clamp/flush the arithmetic result.
float vu_operand(float value) {
    const uint32_t bits = std::bit_cast<uint32_t>(value);
    const uint32_t exponent = bits & 0x7F800000u;
    if (exponent == 0u) return std::bit_cast<float>(bits & 0x80000000u);
    if (exponent == 0x7F800000u) {
        const uint32_t finite = (bits & 0x80000000u) | 0x7F7FFFFFu;
        return std::bit_cast<float>(finite);
    }
    return value;
}

float vu_result(float value) {
    if (!std::isfinite(value)) {
        const uint32_t sign = std::signbit(value) ? 0x80000000u : 0u;
        return std::bit_cast<float>(sign | 0x7F7FFFFFu);
    }
    const uint32_t bits = std::bit_cast<uint32_t>(value);
    if ((bits & 0x7F800000u) == 0u)
        return std::bit_cast<float>(bits & 0x80000000u);
    return value;
}

float vu_add(float a, float b) {
    return vu_result(vu_operand(a) + vu_operand(b));
}

float vu_sub(float a, float b) {
    return vu_result(vu_operand(a) - vu_operand(b));
}

float vu_mul(float a, float b) {
    return vu_result(vu_operand(a) * vu_operand(b));
}

float vu_madd(float accumulator, float a, float b) {
    return vu_result(std::fma(vu_operand(a), vu_operand(b),
                              vu_operand(accumulator)));
}

float vu_msub(float accumulator, float a, float b) {
    return vu_result(std::fma(-vu_operand(a), vu_operand(b),
                              vu_operand(accumulator)));
}

float vu_div(float numerator, float denominator) {
    numerator = vu_operand(numerator);
    denominator = vu_operand(denominator);
    if (denominator == 0.0f) {
        const uint32_t sign = std::signbit(numerator) != std::signbit(denominator)
            ? 0x80000000u : 0u;
        return std::bit_cast<float>(sign | 0x7F7FFFFFu);
    }
    return vu_result(numerator / denominator);
}

float vu_sqrt(float value) {
    return vu_result(std::sqrt(std::fabs(vu_operand(value))));
}

float vu_reciprocal_sqrt(float value) {
    return vu_div(1.0f, vu_sqrt(value));
}

void vf_write(State* state, uint32_t index, const Vector& value, uint32_t mask) {
    if ((index & 31u) == 0) return;
    Vector& destination = state->vf[index & 31u];
    const Vector previous = destination;
    for (uint32_t lane = 0; lane < 4; ++lane)
        if ((mask & (8u >> lane)) != 0) destination[lane] = value[lane];
    const char* setup_trace = std::getenv("OPENRAC_TRACE_VU1_SETUP");
    static std::atomic<uint32_t> vf16_write_notice{0};
    const bool nan_output = std::any_of(
        destination.begin(), destination.end(), [](float lane) { return std::isnan(lane); });
    const bool nonfinite_output = std::any_of(
        destination.begin(), destination.end(), [](float lane) { return !std::isfinite(lane); });
    const char* xgkick_trace = std::getenv("OPENRAC_TRACE_VU1_XGKICK_DATA");
    static std::array<std::atomic<uint32_t>, 8> xgkick_vf_nan_notice{};
    static std::array<std::atomic<uint32_t>, 8> xgkick_vf_entry_notice{};
    const uint32_t reg = index & 31u;
    const uint32_t trace_slot = reg == 27u ? 0u : reg == 28u ? 1u : reg == 2u ? 2u :
                                reg == 24u ? 3u : reg == 30u ? 4u : reg == 29u ? 5u :
                                reg == 11u ? 6u : reg == 3u ? 7u : 8u;
    const bool previous_nonfinite = std::any_of(
        previous.begin(), previous.end(), [](float lane) { return !std::isfinite(lane); });
    const bool xgkick_trace_enabled = xgkick_trace != nullptr &&
        xgkick_trace[0] != '\0' && xgkick_trace[0] != '0' &&
        trace_vu1_xgkick_context_matches();
    const bool trace_xgkick_vf = xgkick_trace_enabled &&
        trace_slot < xgkick_vf_nan_notice.size() &&
        (g_vu_trace_entry == 0x26Du ||
         (g_vu_trace_entry == 0x411u && reg == 2u && nonfinite_output && !previous_nonfinite) ||
         (g_vu_trace_entry == 0x411u && (reg == 24u || reg == 30u) &&
          nonfinite_output && !previous_nonfinite) ||
         (g_vu_trace_entry == 0x411u && (reg == 29u || reg == 11u) &&
          nonfinite_output && !previous_nonfinite) ||
         (g_vu_trace_entry == 0x411u && reg == 3u) ||
         (nonfinite_output && !previous_nonfinite));
    uint32_t trace_notice = 0;
    if (trace_xgkick_vf) {
        trace_notice = g_vu_trace_entry == 0x26Du ||
                       (g_vu_trace_entry == 0x411u && reg == 3u)
            ? xgkick_vf_entry_notice[trace_slot].fetch_add(1, std::memory_order_relaxed)
            : xgkick_vf_nan_notice[trace_slot].fetch_add(1, std::memory_order_relaxed);
    }
    if (trace_xgkick_vf &&
        trace_notice < (g_vu_trace_entry == 0x26Du ? 256u :
                        g_vu_trace_entry == 0x411u && reg == 3u ? 128u : 64u)) {
        uint32_t instruction[2]{};
        std::memcpy(instruction,
                    guest_address(kVu1Micro + (state->pc & 0x7FFu) * 8u),
                    sizeof(instruction));
        openrac::log::info(
            "native VU1 XGKICK VF write vf={} entry={:#x} pc={:#x} instruction={:#010x}/{:#010x} mask={:#x} old={:#010x},{:#010x},{:#010x},{:#010x} new={:#010x},{:#010x},{:#010x},{:#010x}",
            reg, g_vu_trace_entry, state->pc, instruction[0], instruction[1], mask,
            std::bit_cast<uint32_t>(previous[0]), std::bit_cast<uint32_t>(previous[1]),
            std::bit_cast<uint32_t>(previous[2]), std::bit_cast<uint32_t>(previous[3]),
            std::bit_cast<uint32_t>(destination[0]), std::bit_cast<uint32_t>(destination[1]),
            std::bit_cast<uint32_t>(destination[2]), std::bit_cast<uint32_t>(destination[3]));
    }
    if (((index & 31u) == 16u || nan_output) && setup_trace != nullptr &&
        setup_trace[0] != '\0' && setup_trace[0] != '0' &&
        vf16_write_notice.fetch_add(1, std::memory_order_relaxed) < 128u) {
        uint32_t instruction[2]{};
        std::memcpy(instruction,
                    guest_address(kVu1Micro + (state->pc & 0x7FFu) * 8u),
                    sizeof(instruction));
        openrac::log::info(
            "native VU1 VF write vf={} entry={:#x} pc={:#x} instruction={:#010x}/{:#010x} mask={:#x} old={:#010x},{:#010x},{:#010x},{:#010x} new={:#010x},{:#010x},{:#010x},{:#010x}",
            index & 31u, g_vu_trace_entry, state->pc, instruction[0], instruction[1], mask,
            std::bit_cast<uint32_t>(previous[0]), std::bit_cast<uint32_t>(previous[1]),
            std::bit_cast<uint32_t>(previous[2]), std::bit_cast<uint32_t>(previous[3]),
            std::bit_cast<uint32_t>(destination[0]), std::bit_cast<uint32_t>(destination[1]),
            std::bit_cast<uint32_t>(destination[2]), std::bit_cast<uint32_t>(destination[3]));
    }
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

void trace_vu1_data_load(uint32_t address, uint32_t pc, const Vector& value,
                         const char* operation) {
    if (!trace_vu1_read_target(address)) return;
    static std::atomic<uint32_t> notice_count{0};
    if (notice_count.fetch_add(1, std::memory_order_relaxed) >= 8192u) return;
    openrac::log::info(
        "native VU1 watched load submission={} mscal_offset={:#x} entry={:#x} operation={} pc={:#x} qword={} data={:#010x},{:#010x},{:#010x},{:#010x}",
        g_vu_trace_submission_id, g_vu_trace_vif_command_offset, g_vu_trace_entry,
        operation, pc, address, std::bit_cast<uint32_t>(value[0]),
        std::bit_cast<uint32_t>(value[1]), std::bit_cast<uint32_t>(value[2]),
        std::bit_cast<uint32_t>(value[3]));
}

void data_write(uint32_t address, const Vector& value, uint32_t pc) {
    address &= static_cast<uint32_t>(kVu1Quadwords - 1u);
    uint32_t previous[4]{};
    static const bool setup_trace_enabled = [] {
        const char* setting = std::getenv("OPENRAC_TRACE_VU1_SETUP");
        return setting != nullptr && setting[0] != '\0' && setting[0] != '0';
    }();
    const bool nan_output = std::any_of(
        value.begin(), value.end(), [](float lane) { return std::isnan(lane); });
    const bool trace_target =
        address == 27u || (address >= 32u && address <= 38u) ||
        (address >= 136u && address <= 142u) ||
        (address >= 464u && address <= 476u);
    const bool trace_xgkick_data = trace_xgkick_data_target(address);
    if (trace_target || trace_xgkick_data || nan_output)
        std::memcpy(previous, guest_address(kVu1Data + address * 16u), sizeof(previous));
    std::memcpy(guest_address(kVu1Data + address * 16u), value.data(), 16u);
    static std::atomic<uint32_t> xgkick_data_notice{0};
    uint32_t xgkick_data_limit = 256u;
    if (const char* setting = std::getenv("OPENRAC_TRACE_VU1_XGKICK_DATA_LIMIT")) {
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(setting, &end, 10);
        if (end != setting && *end == '\0' && parsed <= 100000u)
            xgkick_data_limit = static_cast<uint32_t>(parsed);
    }
    if (trace_xgkick_data &&
        xgkick_data_notice.fetch_add(1, std::memory_order_relaxed) < xgkick_data_limit) {
        uint32_t instruction[2]{};
        std::memcpy(instruction, guest_address(kVu1Micro + (pc & 0x7FFu) * 8u),
                    sizeof(instruction));
        uint32_t next[4]{};
        std::memcpy(next, value.data(), sizeof(next));
        openrac::log::info(
            "native VU1 watched store submission={} mscal_offset={:#x} entry={:#x} pc={:#x} instruction={:#010x}/{:#010x} qword={} old={:#010x},{:#010x},{:#010x},{:#010x} new={:#010x},{:#010x},{:#010x},{:#010x}",
            g_vu_trace_submission_id, g_vu_trace_vif_command_offset,
            g_vu_trace_entry, pc, instruction[0], instruction[1], address,
            previous[0], previous[1], previous[2], previous[3],
            next[0], next[1], next[2], next[3]);
    }
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
    static std::atomic<uint32_t> transform_write_notice{0};
    if ((trace_target || nan_output) && setup_trace_enabled &&
        (((address >= 136u && address <= 142u) &&
          transform_write_notice.fetch_add(1, std::memory_order_relaxed) < 64u) ||
         ((address < 136u || address > 142u) &&
          target_write_notice.fetch_add(1, std::memory_order_relaxed) < 32u))) {
        uint32_t next[4]{};
        uint32_t instruction[2]{};
        std::memcpy(instruction,
                    guest_address(kVu1Micro + (pc & 0x7FFu) * 8u),
                    sizeof(instruction));
        std::memcpy(next, value.data(), sizeof(next));
        openrac::log::info(
            "native VU1 store wrote qword={} entry={:#x} pc={:#x} instruction={:#010x}/{:#010x} old={:#010x},{:#010x},{:#010x},{:#010x} new={:#010x},{:#010x},{:#010x},{:#010x}",
            address, g_vu_trace_entry, pc, instruction[0], instruction[1],
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
            const char* dump_setting = std::getenv("OPENRAC_DUMP_VU1_KICKS");
            static std::atomic<uint32_t> dump_count{0};
            if (dump_setting != nullptr && dump_setting[0] != '\0' &&
                dump_setting[0] != '0' && trace_vu1_xgkick_context_matches()) {
                const uint32_t dump_index = dump_count.fetch_add(
                    1, std::memory_order_relaxed);
                if (dump_index < 16u) {
                    char path[96];
                    std::snprintf(path, sizeof(path), "/tmp/sly1-vu1-kick-%02u.bin",
                                  dump_index);
                    if (FILE* file = std::fopen(path, "wb")) {
                        std::fwrite(packet.data(), 1, packet.size(), file);
                        std::fclose(file);
                        openrac::log::info(
                            "native VU1 XGKICK dump submission={} mscal_offset={:#x} entry={:#x} index={} start={} bytes={} path={}",
                            g_vu_trace_submission_id, g_vu_trace_vif_command_offset,
                            g_vu_trace_entry, dump_index, address & 0x3FFu,
                            packet.size(), path);
                    }
                }
            }
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
    static std::atomic<uint32_t> state_trace_count{0};
    uint32_t state_trace_limit = 32u;
    if (const char* setting = std::getenv("OPENRAC_TRACE_XGKICK_STATE_LIMIT")) {
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(setting, &end, 10);
        if (end != setting && *end == '\0' && parsed <= 100000u)
            state_trace_limit = static_cast<uint32_t>(parsed);
    }
    const bool trace_enabled = trace_state != nullptr && trace_state[0] != '\0' &&
        trace_state[0] != '0';
    const bool trace_this_kick = trace_enabled &&
        trace_vu1_xgkick_context_matches() &&
        state_trace_count.fetch_add(1, std::memory_order_relaxed) <
            state_trace_limit;
    if (trace_this_kick) {
        uint32_t tag_loaded = 0;
        uint32_t remaining_qwords = 0;
        uint64_t tag_low = 0;
        openrac_guest_gif_parser_probe(&tag_loaded, &remaining_qwords, &tag_low);
        openrac::log::info(
            "native VU1 XGKICK parser state submission={} mscal_offset={:#x} entry={:#x} PC={:#x} address={} active={} remaining={} tag={:#018x}",
            g_vu_trace_submission_id, g_vu_trace_vif_command_offset,
            g_vu_trace_entry, pending->pc, pending->address, tag_loaded,
            remaining_qwords, tag_low);
    }

    uint32_t failure = 0;
    uint32_t failed_address = 0;
    uint64_t first_tag = 0;
    uint64_t failed_tag = 0;
    uint64_t failed_tag_high = 0;
    const bool accepted = kick_gif(pending->address, &failure, &first_tag,
                                   &failed_address, &failed_tag,
                                   &failed_tag_high);
    if (trace_this_kick) {
        openrac::log::info(
            "native VU1 XGKICK trace submission={} mscal_offset={:#x} entry={:#x} PC={:#x} address={} accepted={} first_tag={:#018x} failed_address={} failed_tag={:#018x}:{:#018x} failure={}",
            g_vu_trace_submission_id, g_vu_trace_vif_command_offset,
            g_vu_trace_entry, pending->pc, pending->address, accepted,
            first_tag, failed_address, failed_tag_high, failed_tag,
            failure == 0 ? "none" : failure == 1 ? "packet exceeds native limit" :
            failure == 2 ? "missing EOP" : "GIFtag has reserved bits set");
    }
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
    const char* xgkick_trace = std::getenv("OPENRAC_TRACE_VU1_XGKICK_DATA");
    if (xgkick_trace != nullptr && xgkick_trace[0] != '\0' && xgkick_trace[0] != '0' &&
        trace_vu1_xgkick_context_matches() &&
        ((g_vu_trace_entry == 0x26Du && state->pc == 0x2CBu) ||
         (g_vu_trace_entry == 0x356u && state->pc == 0x39Cu) ||
         (g_vu_trace_entry == 0x411u &&
          (state->pc == 0x430u || state->pc == 0x43Au || state->pc == 0x43Eu ||
           state->pc == 0x441u || fd == 27u)))) {
        openrac::log::info(
            "native VU1 XGKICK upper input submission={} mscal_offset={:#x} entry={:#x} pc={:#x} instruction={:#010x} opcode={:#x} fd={} fs={} ft={} mask={:#x} ACC={:#010x},{:#010x},{:#010x},{:#010x} VF{}={:#010x},{:#010x},{:#010x},{:#010x} VF{}={:#010x},{:#010x},{:#010x},{:#010x}",
            g_vu_trace_submission_id, g_vu_trace_vif_command_offset,
            g_vu_trace_entry, state->pc, word, opcode, fd, fs, ft, mask,
            std::bit_cast<uint32_t>(state->acc[0]),
            std::bit_cast<uint32_t>(state->acc[1]),
            std::bit_cast<uint32_t>(state->acc[2]),
            std::bit_cast<uint32_t>(state->acc[3]), fs,
            std::bit_cast<uint32_t>(source[0]), std::bit_cast<uint32_t>(source[1]),
            std::bit_cast<uint32_t>(source[2]), std::bit_cast<uint32_t>(source[3]), ft,
            std::bit_cast<uint32_t>(target[0]), std::bit_cast<uint32_t>(target[1]),
            std::bit_cast<uint32_t>(target[2]), std::bit_cast<uint32_t>(target[3]));
    }

    if (opcode < 0x1Cu) {
        const uint32_t group = opcode >> 2u;
        const uint32_t broadcast = opcode & 3u;
        for (uint32_t lane = 0; lane < 4; ++lane) {
            const float a = vu_operand(source[lane]);
            const float b = vu_operand(target[broadcast]);
            switch (group) {
            case 0: result[lane] = vu_add(a, b); break;
            case 1: result[lane] = vu_sub(a, b); break;
            case 2: result[lane] = vu_madd(state->acc[lane], a, b); break;
            case 3: result[lane] = vu_msub(state->acc[lane], a, b); break;
            case 4: result[lane] = std::max(a, b); break;
            case 5: result[lane] = std::min(a, b); break;
            case 6: result[lane] = vu_mul(a, b); break;
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
            const float a = vu_operand(source[lane]);
            const float b = vu_operand(use_scalar ? scalar : target[lane]);
            if (opcode == 0x2B || opcode == 0x1D) result[lane] = std::max(a, b);
            else if (opcode == 0x2F || opcode == 0x1F) result[lane] = std::min(a, b);
            else if (multiply && add) result[lane] = vu_madd(state->acc[lane], a, b);
            else if (multiply) result[lane] = opcode == 0x2D || opcode == 0x25 ||
                                                   opcode == 0x27
                                               ? vu_msub(state->acc[lane], a, b)
                                               : vu_mul(a, b);
            else if (add) result[lane] = vu_add(a, b);
            else result[lane] = vu_sub(a, b);
        }
        if (opcode == 0x21 || opcode == 0x23 || opcode == 0x25 || opcode == 0x27 ||
            opcode == 0x29 || opcode == 0x2D)
            state->acc = result;
        update_float_flags(state, result, mask);
        vf_write(state, fd, result, mask);
        return true;
    }
    case 0x2E: // OPM-SUB
        result = {vu_sub(state->acc[0], vu_sub(vu_mul(source[1], target[2]),
                                               vu_mul(source[2], target[1]))),
                  vu_sub(state->acc[1], vu_sub(vu_mul(source[2], target[0]),
                                               vu_mul(source[0], target[2]))),
                  vu_sub(state->acc[2], vu_sub(vu_mul(source[0], target[1]),
                                               vu_mul(source[1], target[0]))),
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
                    state->acc[lane] = vu_add(source[lane], target[broadcast]);
            return true;
        }
        case 0x04: case 0x05: case 0x06: case 0x07: { // SUBAbc
            const uint32_t broadcast = special & 3u;
            for (uint32_t lane = 0; lane < 4; ++lane)
                if ((mask & (8u >> lane)) != 0)
                    state->acc[lane] = vu_sub(source[lane], target[broadcast]);
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
                state->acc[lane] = vu_mul(source[a], target[b]);
            }
            return true;
        case 0x08: case 0x09: case 0x0A: case 0x0B: { // MADDAbc
            const uint32_t broadcast = special & 3u;
            for (uint32_t lane = 0; lane < 4; ++lane)
                if ((mask & (8u >> lane)) != 0)
                    state->acc[lane] = vu_madd(state->acc[lane], source[lane],
                                               target[broadcast]);
            return true;
        }
        case 0x0C: case 0x0D: case 0x0E: case 0x0F: { // MSUBAbc
            const uint32_t broadcast = special & 3u;
            for (uint32_t lane = 0; lane < 4; ++lane)
                if ((mask & (8u >> lane)) != 0)
                    state->acc[lane] = vu_msub(state->acc[lane], source[lane],
                                               target[broadcast]);
            return true;
        }
        case 0x18: case 0x19: case 0x1A: case 0x1B: { // MULAx/y/z/w
            const uint32_t broadcast = special & 3u;
            for (uint32_t lane = 0; lane < 4; ++lane)
                if ((mask & (8u >> lane)) != 0)
                    state->acc[lane] = vu_mul(source[lane], target[broadcast]);
            return true;
        }
        case 0x1C: // MULAq
        case 0x1E: { // MULAi
            const float scalar = special == 0x1C ? state->q : state->i;
            for (uint32_t lane = 0; lane < 4; ++lane)
                if ((mask & (8u >> lane)) != 0)
                    state->acc[lane] = vu_mul(source[lane], scalar);
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
                    state->acc[lane] = vu_add(source[lane], target[lane]);
            return true;
        }
        case 0x29: { // MADDA
            for (uint32_t lane = 0; lane < 4; ++lane)
                if ((mask & (8u >> lane)) != 0)
                    state->acc[lane] = vu_madd(state->acc[lane], source[lane],
                                               target[lane]);
            return true;
        }
        case 0x2A: { // MULA
            for (uint32_t lane = 0; lane < 4; ++lane)
                if ((mask & (8u >> lane)) != 0)
                    state->acc[lane] = vu_mul(source[lane], target[lane]);
            return true;
        }
        case 0x2C: { // SUBA
            for (uint32_t lane = 0; lane < 4; ++lane)
                if ((mask & (8u >> lane)) != 0)
                    state->acc[lane] = vu_sub(source[lane], target[lane]);
            return true;
        }
        case 0x2D: { // MSUBA
            for (uint32_t lane = 0; lane < 4; ++lane)
                if ((mask & (8u >> lane)) != 0)
                    state->acc[lane] = vu_msub(state->acc[lane], source[lane],
                                               target[lane]);
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
            const Vector loaded = data_read(address);
            trace_vu1_data_load(address, state->pc, loaded, "LQ");
            const char* xgkick_trace = std::getenv("OPENRAC_TRACE_VU1_XGKICK_DATA");
            static std::atomic<uint32_t> xgkick_lq_notice{0};
            const bool trace_lq = xgkick_trace != nullptr && xgkick_trace[0] != '\0' &&
                xgkick_trace[0] != '0' && trace_vu1_xgkick_context_matches() &&
                g_vu_trace_entry == 0x411u &&
                (state->pc == 0x447u || target == 3u || target == 11u);
            const bool nonfinite_input = trace_lq && std::any_of(
                loaded.begin(), loaded.end(), [](float lane) { return !std::isfinite(lane); });
            const uint32_t lq_notice = trace_lq
                ? xgkick_lq_notice.fetch_add(1, std::memory_order_relaxed) : 0u;
            if (trace_lq && (lq_notice < (target == 3u || target == 11u ? 64u : 16u) ||
                             nonfinite_input)) {
                openrac::log::info(
                    "native VU1 XGKICK LQ entry={:#x} pc={:#x} instruction={:#010x} VF{} <- qword={} VI{}={} offset={} mask={:#x} data={:#010x},{:#010x},{:#010x},{:#010x}",
                    g_vu_trace_entry, state->pc, word, target, address, source & 15u,
                    vi_read(*state, source & 15u), offset, (word >> 21u) & 15u,
                    std::bit_cast<uint32_t>(loaded[0]),
                    std::bit_cast<uint32_t>(loaded[1]),
                    std::bit_cast<uint32_t>(loaded[2]),
                    std::bit_cast<uint32_t>(loaded[3]));
            }
            vf_write(state, target, loaded, (word >> 21u) & 15u);
        } else {
            // SQ uses the it field for the VI base and the is field for VF source;
            // LQ uses those fields in the opposite roles.
            const uint32_t address = (vi_read(*state, target & 15u) + offset) & 0x3FFu;
            const Vector value = vf_read(*state, source);
            const uint32_t mask = (word >> 21u) & 15u;
            Vector previous = data_read(address);
            for (uint32_t lane = 0; lane < 4; ++lane)
                if ((mask & (8u >> lane)) != 0) previous[lane] = value[lane];
            data_write(address, previous, state->pc);
        }
        return true;
    }

    if (primary == 0x04u || primary == 0x05u) { // ILW / ISW
        const uint32_t base = source & 15u;
        const int32_t offset = sign_extend(word & 0x7FFu, 11);
        const uint32_t address = (vi_read(*state, base) + offset) & 0x3FFu;
        const uint32_t mask = (word >> 21u) & 15u;
        uint8_t* data = guest_address(kVu1Data + address * 16u);
        if (primary == 0x04u) { // ILW reads only the first selected field.
            for (uint32_t lane = 0; lane < 4; ++lane) {
                if ((mask & (8u >> lane)) == 0) continue;
                uint16_t value = 0;
                std::memcpy(&value, data + lane * 4u, sizeof(value));
                vi_write(state, target, value);
                break;
            }
            return true;
        }
        const bool trace_xgkick_store = trace_xgkick_data_target(address);
        uint32_t previous[4]{};
        if (trace_xgkick_store)
            std::memcpy(previous, data, sizeof(previous));
        const uint16_t stored_value = vi_read(*state, target);
        for (uint32_t lane = 0; lane < 4; ++lane) {
            if ((mask & (8u >> lane)) == 0) continue;
            std::memcpy(data + lane * 4u, &stored_value, sizeof(stored_value));
            const uint16_t zero = 0;
            std::memcpy(data + lane * 4u + 2u, &zero, sizeof(zero));
        }
        static std::atomic<uint32_t> xgkick_store_notice{0};
        if (trace_xgkick_store &&
            xgkick_store_notice.fetch_add(1, std::memory_order_relaxed) < 2048u) {
            uint32_t current[4]{};
            uint32_t instruction[2]{};
            std::memcpy(current, data, sizeof(current));
            std::memcpy(instruction,
                        guest_address(kVu1Micro + (state->pc & 0x7FFu) * 8u),
                        sizeof(instruction));
            openrac::log::info(
                "native VU1 XGKICK ISW entry={:#x} pc={:#x} instruction={:#010x}/{:#010x} qword={} vi{}={:#x} mask={:#x} old={:#010x},{:#010x},{:#010x},{:#010x} new={:#010x},{:#010x},{:#010x},{:#010x}",
                g_vu_trace_entry, state->pc, instruction[0], instruction[1], address,
                target, stored_value, mask, previous[0], previous[1], previous[2],
                previous[3], current[0], current[1], current[2], current[3]);
        }
        return true;
    }

    if ((word >> 25u) == 0x40u) {
        const uint32_t function = word & 0x3Fu;
        const uint32_t field = (word >> 6u) & 31u;
        const uint32_t special = (field << 2u) | (word & 3u);
        if ((word & 0x7FFu) == 0x67Cu) { // MFP.dest VF[ft], P
            const float value = state->p;
            vf_write(state, target, {value, value, value, value},
                     (word >> 21u) & 15u);
            return true;
        }
        if ((word & 0x7FFu) == 0x73Fu) { // ERLENG P, VF[fs]
            const Vector value = vf_read(*state, source);
            const float length_squared = vu_add(
                vu_add(vu_mul(value[0], value[0]), vu_mul(value[1], value[1])),
                vu_mul(value[2], value[2]));
            const float previous_p = state->p;
            state->p = vu_reciprocal_sqrt(length_squared);
            const char* xgkick_trace = std::getenv("OPENRAC_TRACE_VU1_XGKICK_DATA");
            static std::atomic<uint32_t> p_trace_notice{0};
            const uint32_t p_notice = p_trace_notice.fetch_add(1, std::memory_order_relaxed);
            const bool pathological = !std::isfinite(length_squared) || length_squared == 0.0f ||
                !std::isfinite(state->p);
            if (xgkick_trace != nullptr && xgkick_trace[0] != '\0' &&
                xgkick_trace[0] != '0' && trace_vu1_xgkick_context_matches() &&
                g_vu_trace_entry == 0x411u &&
                (p_notice < 64u || pathological)) {
                openrac::log::info(
                    "native VU1 ERLENG entry={:#x} pc={:#x} vf{}={:#010x},{:#010x},{:#010x},{:#010x} length_squared={:#010x} P={:#010x}->{:#010x}",
                    g_vu_trace_entry, state->pc, source,
                    std::bit_cast<uint32_t>(value[0]), std::bit_cast<uint32_t>(value[1]),
                    std::bit_cast<uint32_t>(value[2]), std::bit_cast<uint32_t>(value[3]),
                    std::bit_cast<uint32_t>(length_squared),
                    std::bit_cast<uint32_t>(previous_p), std::bit_cast<uint32_t>(state->p));
            }
            return true;
        }
        if ((word & 0x7FFu) == 0x7BDu) { // ERSQRT P, VF[fs][fsf]
            const uint32_t field = (word >> 21u) & 3u;
            const float value = vf_read(*state, source)[field];
            const float previous_p = state->p;
            state->p = vu_reciprocal_sqrt(value);
            const char* xgkick_trace = std::getenv("OPENRAC_TRACE_VU1_XGKICK_DATA");
            static std::atomic<uint32_t> p_trace_notice{0};
            const uint32_t p_notice = p_trace_notice.fetch_add(1, std::memory_order_relaxed);
            const bool pathological = !std::isfinite(value) || value == 0.0f ||
                !std::isfinite(state->p);
            if (xgkick_trace != nullptr && xgkick_trace[0] != '\0' &&
                xgkick_trace[0] != '0' && trace_vu1_xgkick_context_matches() &&
                g_vu_trace_entry == 0x411u &&
                (p_notice < 64u || pathological)) {
                openrac::log::info(
                    "native VU1 ERSQRT entry={:#x} pc={:#x} vf{}[{}]={:#010x} P={:#010x}->{:#010x}",
                    g_vu_trace_entry, state->pc, source, field,
                    std::bit_cast<uint32_t>(value),
                    std::bit_cast<uint32_t>(previous_p), std::bit_cast<uint32_t>(state->p));
            }
            return true;
        }
        if ((word & 0x7FFu) == 0x3BFu) { // WAITQ; scalar operations complete synchronously here
            return true;
        }
        if ((word & 0x7FFu) == 0x7BFu) { // WAITP; P results are synchronous in this translator
            return true;
        }
        if ((word & 0x7FFu) == 0x3BCu) { // DIV Q, VF[ft]ftf, VF[fs]fsf
            const uint32_t ftf = (word >> 23u) & 3u;
            const uint32_t fsf = (word >> 21u) & 3u;
            const float numerator = vf_read(*state, target)[ftf];
            const float denominator = vf_read(*state, source)[fsf];
            const bool numerator_zero = numerator == 0.0f;
            const bool denominator_zero = denominator == 0.0f;
            uint16_t divide_flags = 0;
            if (denominator_zero)
                divide_flags = numerator_zero ? 0x10u : 0x20u;
            state->status_flags = static_cast<uint16_t>(
                (state->status_flags & ~0x30u) | divide_flags |
                (divide_flags << 6u));
            state->q = vu_div(numerator, denominator);
            const char* setup_trace = std::getenv("OPENRAC_TRACE_VU1_SETUP");
            static std::atomic<uint32_t> divide_notice{0};
            if (setup_trace != nullptr && setup_trace[0] != '\0' &&
                setup_trace[0] != '0' &&
                divide_notice.fetch_add(1, std::memory_order_relaxed) < 128u) {
                openrac::log::info(
                    "native VU1 DIV entry={:#x} pc={:#x} ft=vf{}[{}]={:#010x} fs=vf{}[{}]={:#010x} Q={:#010x}",
                    g_vu_trace_entry, state->pc, target, ftf,
                    std::bit_cast<uint32_t>(numerator), source, fsf,
                    std::bit_cast<uint32_t>(denominator),
                    std::bit_cast<uint32_t>(state->q));
            }
            return true;
        }
        if ((word & 0x7FFu) == 0x3BEu) { // RSQRT Q, VF[ft]ftf, VF[fs]fsf
            const uint32_t ftf = (word >> 23u) & 3u;
            const uint32_t fsf = (word >> 21u) & 3u;
            const float numerator = vf_read(*state, target)[ftf];
            const float denominator = vf_read(*state, source)[fsf];
            state->q = vu_div(numerator, vu_sqrt(denominator));
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
            if (special == 0x30u) { // MOVE.dest ft, fs
                vf_write(state, target, vf_read(*state, source),
                         (word >> 21u) & 15u);
                return true;
            }
            if (special == 0x31u) { // MR32.dest ft, fs
                const Vector input = vf_read(*state, source);
                vf_write(state, target,
                         {input[1], input[2], input[3], input[0]},
                         (word >> 21u) & 15u);
                return true;
            }
            if (special >= 0x34u && special <= 0x37u) {
                // LQI/SQI post-increment; LQD/SQD pre-decrement. The load
                // base is is, while the store base is it.
                const bool load = special == 0x34u || special == 0x36u;
                const bool decrement = special == 0x36u || special == 0x37u;
                const uint32_t base = (load ? source : target) & 15u;
                const uint16_t old_address = vi_read(*state, base);
                const uint16_t address = static_cast<uint16_t>(
                    old_address + (decrement ? -1 : 0));
                if (decrement) vi_write(state, base, address);
                if (load) {
                    const Vector loaded = data_read(address);
                    trace_vu1_data_load(address, state->pc, loaded,
                                        decrement ? "LQD" : "LQI");
                    const char* xgkick_trace = std::getenv("OPENRAC_TRACE_VU1_XGKICK_DATA");
                    static std::atomic<uint32_t> xgkick_lqi_notice{0};
                    if (xgkick_trace != nullptr && xgkick_trace[0] != '\0' &&
                        xgkick_trace[0] != '0' && trace_vu1_xgkick_context_matches() &&
                        g_vu_trace_entry == 0x411u &&
                        target == 3u &&
                        xgkick_lqi_notice.fetch_add(1, std::memory_order_relaxed) < 128u) {
                        openrac::log::info(
                            "native VU1 XGKICK LQI submission={} mscal_offset={:#x} entry={:#x} pc={:#x} VF{} <- qword={} VI{}={} instruction={:#010x} data={:#010x},{:#010x},{:#010x},{:#010x}",
                            g_vu_trace_submission_id, g_vu_trace_vif_command_offset,
                            g_vu_trace_entry, state->pc, target, address, base, old_address,
                            word, std::bit_cast<uint32_t>(loaded[0]),
                            std::bit_cast<uint32_t>(loaded[1]),
                            std::bit_cast<uint32_t>(loaded[2]),
                            std::bit_cast<uint32_t>(loaded[3]));
                    }
                    const char* setup_trace = std::getenv("OPENRAC_TRACE_VU1_SETUP");
                    static std::atomic<uint32_t> lqi_notice{0};
                    const bool has_nan = std::any_of(
                        loaded.begin(), loaded.end(), [](float lane) { return std::isnan(lane); });
                    if (setup_trace != nullptr && setup_trace[0] != '\0' &&
                        setup_trace[0] != '0' && g_vu_trace_entry == 0x401u &&
                        (has_nan || lqi_notice.fetch_add(1, std::memory_order_relaxed) < 24u)) {
                        openrac::log::info(
                            "native VU1 LQI entry={:#x} pc={:#x} vf={} vi{}={} qword={} mask={:#x} words={:#010x},{:#010x},{:#010x},{:#010x}",
                            g_vu_trace_entry, state->pc, target, base, old_address, address,
                            (word >> 21u) & 15u, std::bit_cast<uint32_t>(loaded[0]),
                            std::bit_cast<uint32_t>(loaded[1]), std::bit_cast<uint32_t>(loaded[2]),
                            std::bit_cast<uint32_t>(loaded[3]));
                    }
                    vf_write(state, target, loaded, (word >> 21u) & 15u);
                } else {
                    const Vector value = vf_read(*state, source);
                    Vector previous = data_read(address);
                    const uint32_t mask = (word >> 21u) & 15u;
                    for (uint32_t lane = 0; lane < 4; ++lane)
                        if ((mask & (8u >> lane)) != 0) previous[lane] = value[lane];
                    data_write(address, previous, state->pc);
                }
                if (!decrement)
                    vi_write(state, base, static_cast<uint16_t>(old_address + 1u));
                return true;
            }
            if (special == 0x3Cu) { // MTIR: VF[is][fsf] -> VI[it]
                const uint32_t lane = (word >> 21u) & 3u;
                vi_write(state, target,
                         static_cast<uint16_t>(std::bit_cast<uint32_t>(
                             vf_read(*state, source)[lane])));
                return true;
            }
            if (special == 0x3Du) { // MFIR: sign-extended VI[is] -> VF[it]
                const int32_t value = std::bit_cast<int16_t>(vi_read(*state, source));
                const float bits = std::bit_cast<float>(static_cast<uint32_t>(value));
                vf_write(state, target, {bits, bits, bits, bits},
                         (word >> 21u) & 15u);
                return true;
            }
            if (special == 0x3Eu) { // ILWR reads the first selected field.
                const uint32_t mask = (word >> 21u) & 15u;
                for (uint32_t lane = 0; lane < 4; ++lane) {
                    if ((mask & (8u >> lane)) == 0) continue;
                    uint16_t value = 0;
                    const uint8_t* data = guest_address(
                        kVu1Data + (vi_read(*state, source & 15u) & 0x3FFu) * 16u);
                    std::memcpy(&value, data + lane * 4u, sizeof(value));
                    vi_write(state, target, value);
                    break;
                }
                return true;
            }
            if (special == 0x3Fu) { // ISWR writes VI[it] into all four fields.
                const float bits = std::bit_cast<float>(
                    static_cast<uint32_t>(vi_read(*state, target)));
                data_write(vi_read(*state, source & 15u), {bits, bits, bits, bits},
                           state->pc);
                return true;
            }
            if (special == 0x6Cu) { // XGKICK
                const uint32_t address = vi_read(*state, source);
                const char* kick_address_trace = std::getenv("OPENRAC_TRACE_VU1_KICK_ADDRESS");
                static std::atomic<uint32_t> kick_address_notice{0};
                if (kick_address_trace != nullptr && kick_address_trace[0] != '\0' &&
                    kick_address_trace[0] != '0' &&
                    trace_vu1_xgkick_context_matches() &&
                    kick_address_notice.fetch_add(1, std::memory_order_relaxed) < 64u) {
                    const uint32_t previous_qword = (address + kVu1Quadwords - 1u) & 0x3FFu;
                    const uint8_t* previous_data = guest_address(
                        kVu1Data + previous_qword * 16u);
                    const uint8_t* target_data = guest_address(kVu1Data + address * 16u);
                    uint64_t previous_low = 0, target_low = 0, target_high = 0;
                    std::memcpy(&previous_low, previous_data, sizeof(previous_low));
                    std::memcpy(&target_low, target_data, sizeof(target_low));
                    std::memcpy(&target_high, target_data + sizeof(target_low),
                                sizeof(target_high));
                    openrac::log::info(
                        "native VU1 XGKICK operand submission={} mscal_offset={:#x} entry={:#x} pc={:#x} instruction={:#010x} source=VI{} value={} VI3={} VI4={} VI6={} VI7={} previous_qword={} previous_low={:#018x} target_low={:#018x} target_high={:#018x}",
                        g_vu_trace_submission_id, g_vu_trace_vif_command_offset,
                        g_vu_trace_entry, state->pc, word, source, address,
                        vi_read(*state, 3u), vi_read(*state, 4u),
                        vi_read(*state, 6u), vi_read(*state, 7u), previous_qword,
                        previous_low, target_low, target_high);
                }
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
            if (special == 0x68u || special == 0x69u) { // XTOP / XITOP
                uint32_t value = state->itop;
                if (special == 0x68u) {
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

extern "C" void openrac_guest_vu1_set_trace_context(uint32_t submission_id,
                                                      uint32_t vif_command_offset) {
    g_vu_trace_submission_id = submission_id;
    g_vu_trace_vif_command_offset = vif_command_offset;
}

extern "C" void openrac_guest_vu1_set_trace_target_submission(
    uint32_t submission_id, uint32_t stream_begin, uint32_t stream_end) {
    g_vu_trace_target_submission_id = submission_id;
    g_vu_trace_target_stream_begin = stream_begin;
    g_vu_trace_target_stream_end = stream_end;
}

extern "C" int openrac_guest_vu1_trace_target_context_matches(void) {
    return trace_vu1_xgkick_context_matches() ? 1 : 0;
}

extern "C" int openrac_guest_vu1_execute(uint32_t entry, uint32_t* unsupported_word,
                                           uint32_t* stopped_pc,
                                           uint32_t* stopped_pipeline,
                                           uint32_t* paired_word) {
    if (!guest_memory_available() || unsupported_word == nullptr ||
        stopped_pc == nullptr || stopped_pipeline == nullptr || paired_word == nullptr) return 0;
    std::lock_guard lock(g_vu_mutex);
    g_vu_trace_entry = entry;
    const char* timing_setting = std::getenv("OPENRAC_TRACE_VU1_TIMING");
    const bool timing_enabled = timing_setting != nullptr &&
        timing_setting[0] != '\0' && timing_setting[0] != '0' &&
        trace_vu1_xgkick_context_matches();
    struct VUTimingTrace {
        bool active;
        uint32_t submission;
        uint32_t command_offset;
        uint32_t entry;
        std::chrono::steady_clock::time_point start;
        ~VUTimingTrace() {
            if (!active) return;
            static std::atomic<uint32_t> notices{0};
            if (notices.fetch_add(1, std::memory_order_relaxed) >= 4096u) return;
            const auto elapsed_us = std::chrono::duration_cast<
                std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
            openrac::log::info(
                "native VU1 timing submission={} mscal_offset={:#x} entry={:#x} elapsed_us={}",
                submission, command_offset, entry, elapsed_us);
        }
    } timing_trace{
        timing_enabled,
        g_vu_trace_submission_id, g_vu_trace_vif_command_offset, entry,
        timing_enabled ? std::chrono::steady_clock::now()
                       : std::chrono::steady_clock::time_point{}};
    const char* xgkick_trace = std::getenv("OPENRAC_TRACE_VU1_XGKICK_DATA");
    static std::atomic<uint32_t> xgkick_entry_state_notice{0};
    if (entry == 0x26Du && xgkick_trace != nullptr && xgkick_trace[0] != '\0' &&
        xgkick_trace[0] != '0' && trace_vu1_xgkick_context_matches() &&
        xgkick_entry_state_notice.fetch_add(1, std::memory_order_relaxed) < 16u) {
        openrac::log::info(
            "native VU1 XGKICK entry state entry={:#x} ACC={:#010x},{:#010x},{:#010x},{:#010x} VF27={:#010x},{:#010x},{:#010x},{:#010x} VF28={:#010x},{:#010x},{:#010x},{:#010x}",
            entry, std::bit_cast<uint32_t>(g_state.acc[0]),
            std::bit_cast<uint32_t>(g_state.acc[1]),
            std::bit_cast<uint32_t>(g_state.acc[2]),
            std::bit_cast<uint32_t>(g_state.acc[3]),
            std::bit_cast<uint32_t>(g_state.vf[27][0]),
            std::bit_cast<uint32_t>(g_state.vf[27][1]),
            std::bit_cast<uint32_t>(g_state.vf[27][2]),
            std::bit_cast<uint32_t>(g_state.vf[27][3]),
            std::bit_cast<uint32_t>(g_state.vf[28][0]),
            std::bit_cast<uint32_t>(g_state.vf[28][1]),
            std::bit_cast<uint32_t>(g_state.vf[28][2]),
            std::bit_cast<uint32_t>(g_state.vf[28][3]));
    }
    uint32_t pc = entry == 0xFFFFFFFFu ? g_state.pc : (entry & 0x7FFu);
    bool end_delay = false;
    bool branch_delay = false;
    uint32_t branch_target = 0;
    PendingKick pending_kick{};
    static std::array<std::atomic<uint32_t>, 2> loop_trace_count{};
    const char* loop_trace_setting = std::getenv("OPENRAC_TRACE_VU1_LOOP");
    const bool loop_trace_enabled = loop_trace_setting != nullptr &&
        loop_trace_setting[0] != '\0' && loop_trace_setting[0] != '0';
    const bool stop_trace_enabled = xgkick_trace != nullptr &&
        xgkick_trace[0] != '\0' && xgkick_trace[0] != '0';
    static std::atomic<uint32_t> stop_trace_count{0};
    const auto trace_stop = [&](uint32_t stop_pc, uint32_t cycle, bool completed) {
        if (!stop_trace_enabled || !trace_vu1_xgkick_context_matches() ||
            stop_trace_count.fetch_add(1, std::memory_order_relaxed) >= 128u)
            return;
        openrac::log::info(
            "native VU1 program {} submission={} mscal_offset={:#x} entry={:#x} cycles={} stop_pc={:#x} VI5={} VI6={} VI7={} VI8={} VI9={} VI10={} VI14={} VI15={} pending_kick={}",
            completed ? "completed" : "stopped", g_vu_trace_submission_id,
            g_vu_trace_vif_command_offset, entry, cycle, stop_pc,
            vi_read(g_state, 5u), vi_read(g_state, 6u), vi_read(g_state, 7u),
            vi_read(g_state, 8u), vi_read(g_state, 9u), vi_read(g_state, 10u),
            vi_read(g_state, 14u), vi_read(g_state, 15u), pending_kick.active ? 1u : 0u);
    };
    static std::atomic<uint32_t> world_iteration_notice{0};
    static std::atomic<uint32_t> stream_iteration_notice{0};
    bool trace_world_iteration = false;
    bool trace_stream_iteration = false;
    // The retail world setup routine at 0x26d copies enough geometry to exceed
    // the generic budget. Keep this exception local to that program entry; a
    // larger global limit would let unrelated runaway VU programs monopolize
    // the host. The 16K limit completes the observed copy stream while keeping
    // the existing stop diagnostics for programs that still fail to finish.
    uint32_t instruction_limit = entry == 0x26Du ? 16'384u : kMaxInstructions;
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
            trace_stop(pc, cycle, false);
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
        const bool upper_translated =
            translate_upper(&upper_result, upper, unsupported_word);
        if (!upper_translated) {
            trace_stop(current_pc, cycle, false);
            static std::atomic<uint32_t> upper_fault_context_notice{0};
            const uint32_t notice =
                upper_fault_context_notice.fetch_add(1, std::memory_order_relaxed);
            if (notice < 32u) {
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
                openrac::log::warn(
                    "native VU1 upper path to unsupported instruction entry={:#x}:{}",
                    entry, context.str());
            }
            dispatch_pending_kick(&pending_kick);
            *paired_word = lower;
            *stopped_pc = current_pc;
            *stopped_pipeline = 0;
            return 0;
        }
        const char* xgkick_data_trace = std::getenv("OPENRAC_TRACE_VU1_XGKICK_DATA");
        static std::atomic<uint32_t> acc_nan_transition_notice{0};
        const bool before_acc_nan = std::any_of(
            before.acc.begin(), before.acc.end(), [](float lane) { return std::isnan(lane); });
        const bool after_acc_nan = std::any_of(
            upper_result.acc.begin(), upper_result.acc.end(),
            [](float lane) { return std::isnan(lane); });
        if (xgkick_data_trace != nullptr && xgkick_data_trace[0] != '\0' &&
            xgkick_data_trace[0] != '0' && trace_vu1_xgkick_context_matches() &&
            !before_acc_nan && after_acc_nan &&
            acc_nan_transition_notice.fetch_add(1, std::memory_order_relaxed) < 64u) {
            openrac::log::info(
                "native VU1 XGKICK ACC became NaN entry={:#x} pc={:#x} upper={:#010x} before={:#010x},{:#010x},{:#010x},{:#010x} after={:#010x},{:#010x},{:#010x},{:#010x}",
                entry, current_pc, upper,
                std::bit_cast<uint32_t>(before.acc[0]),
                std::bit_cast<uint32_t>(before.acc[1]),
                std::bit_cast<uint32_t>(before.acc[2]),
                std::bit_cast<uint32_t>(before.acc[3]),
                std::bit_cast<uint32_t>(upper_result.acc[0]),
                std::bit_cast<uint32_t>(upper_result.acc[1]),
                std::bit_cast<uint32_t>(upper_result.acc[2]),
                std::bit_cast<uint32_t>(upper_result.acc[3]));
        }
        const auto acc_has_abnormal = [](const Vector& value) {
            return std::any_of(value.begin(), value.end(), [](float lane) {
                return !std::isfinite(lane) || std::fpclassify(lane) == FP_SUBNORMAL;
            });
        };
        static std::atomic<uint32_t> acc_abnormal_transition_notice{0};
        if (xgkick_data_trace != nullptr && xgkick_data_trace[0] != '\0' &&
            xgkick_data_trace[0] != '0' && trace_vu1_xgkick_context_matches() &&
            entry == 0x411u &&
            !acc_has_abnormal(before.acc) && acc_has_abnormal(upper_result.acc) &&
            acc_abnormal_transition_notice.fetch_add(1, std::memory_order_relaxed) < 32u) {
            const uint32_t fs = (upper >> 11u) & 31u;
            const uint32_t ft = (upper >> 16u) & 31u;
            const Vector source = vf_read(before, fs);
            const Vector target = vf_read(before, ft);
            openrac::log::info(
                "native VU1 ACC first abnormal entry={:#x} pc={:#x} lower={:#010x} upper={:#010x} VF{}={:#010x},{:#010x},{:#010x},{:#010x} VF{}={:#010x},{:#010x},{:#010x},{:#010x} before={:#010x},{:#010x},{:#010x},{:#010x} after={:#010x},{:#010x},{:#010x},{:#010x}",
                entry, current_pc, lower, upper,
                fs, std::bit_cast<uint32_t>(source[0]),
                std::bit_cast<uint32_t>(source[1]), std::bit_cast<uint32_t>(source[2]),
                std::bit_cast<uint32_t>(source[3]), ft,
                std::bit_cast<uint32_t>(target[0]), std::bit_cast<uint32_t>(target[1]),
                std::bit_cast<uint32_t>(target[2]), std::bit_cast<uint32_t>(target[3]),
                std::bit_cast<uint32_t>(before.acc[0]),
                std::bit_cast<uint32_t>(before.acc[1]),
                std::bit_cast<uint32_t>(before.acc[2]),
                std::bit_cast<uint32_t>(before.acc[3]),
                std::bit_cast<uint32_t>(upper_result.acc[0]),
                std::bit_cast<uint32_t>(upper_result.acc[1]),
                std::bit_cast<uint32_t>(upper_result.acc[2]),
                std::bit_cast<uint32_t>(upper_result.acc[3]));
        }
        if ((upper & (1u << 31u)) != 0) {
            upper_result.i = std::bit_cast<float>(lower);
        } else if (!translate_lower(&lower_result, lower, &next_pc,
                                    unsupported_word, &pending_kick)) {
            trace_stop(current_pc, cycle, false);
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
        const char* p_trace = std::getenv("OPENRAC_TRACE_VU1_XGKICK_DATA");
        static std::atomic<uint32_t> p_nan_transition_notice{0};
        if (p_trace != nullptr && p_trace[0] != '\0' && p_trace[0] != '0' &&
            trace_vu1_xgkick_context_matches() &&
            !std::isnan(before.p) && std::isnan(lower_result.p) &&
            p_nan_transition_notice.fetch_add(1, std::memory_order_relaxed) < 64u) {
            openrac::log::info(
                "native VU1 P became NaN entry={:#x} pc={:#x} lower={:#010x} before={:#010x} after={:#010x}",
                entry, current_pc, lower, std::bit_cast<uint32_t>(before.p),
                std::bit_cast<uint32_t>(lower_result.p));
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
                trace_stop(pc, cycle + 1u, true);
                return 1;
            }
            end_delay = true;
        } else if (end_delay) {
            g_state.pc = pc;
            dispatch_pending_kick(&pending_kick);
            trace_stop(pc, cycle + 1u, true);
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
        const uint32_t transform_vi8 = vi_read(before, 8u);
        const bool trace_transform_range = entry == 0x411u &&
            (current_pc >= 0x420u && current_pc <= 0x450u) &&
            (cycle < 128u || (transform_vi8 >= 450u && transform_vi8 <= 470u) ||
             next_pc != current_pc + 1u);
        static std::atomic<uint32_t> transform_trace_count{0};
        if (loop_trace_enabled && trace_transform_range &&
            transform_trace_count.fetch_add(1, std::memory_order_relaxed) < 512u) {
            openrac::log::info(
                "native VU1 transform loop submission={} mscal_offset={:#x} entry={:#x} cycle={} pc={:#x} lower={:#010x} upper={:#010x} next={:#x} VI5={}->{} VI6={}->{} VI8={}->{} VI9={}->{} VI10={}->{} VI14={} VI15={} branch_delay={} end_delay={}",
                g_vu_trace_submission_id, g_vu_trace_vif_command_offset,
                entry, cycle, current_pc, lower, upper, next_pc,
                vi_read(before, 5u), vi_read(g_state, 5u),
                vi_read(before, 6u), vi_read(g_state, 6u),
                transform_vi8, vi_read(g_state, 8u),
                vi_read(before, 9u), vi_read(g_state, 9u),
                vi_read(before, 10u), vi_read(g_state, 10u),
                vi_read(g_state, 14u), vi_read(g_state, 15u),
                branch_delay ? 1u : 0u, end_delay ? 1u : 0u);
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
        if (loop_trace_enabled && entry == 0x318u &&
            current_pc == 0x237u && !trace_stream_iteration &&
            stream_iteration_notice.fetch_add(1, std::memory_order_relaxed) < 4u)
            trace_stream_iteration = true;
        if (trace_stream_iteration) {
            openrac::log::info(
                "native VU1 stream loop entry={:#x} cycle={} pc={:#x} lower={:#010x} upper={:#010x} next={:#x} VI1={} VI2={} VI5={} VI6={} VI8={} VI9={} VI10={} VI13={} VI14={} VI15={} branch_delay={}",
                entry, cycle, current_pc, lower, upper, pc, vi_read(g_state, 1u),
                vi_read(g_state, 2u), vi_read(g_state, 5u), vi_read(g_state, 6u),
                vi_read(g_state, 8u), vi_read(g_state, 9u), vi_read(g_state, 10u),
                vi_read(g_state, 13u), vi_read(g_state, 14u), vi_read(g_state, 15u),
                branch_delay ? 1u : 0u);
            if (current_pc == 0x250u) trace_stream_iteration = false;
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
