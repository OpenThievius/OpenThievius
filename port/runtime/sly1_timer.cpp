/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Native implementation of Sly's EE Count snapshot routine. */
#include "openrac/guest.h"

#include <cstdint>
#include <cstring>
#include <mutex>

namespace {
constexpr uint32_t kPreviousCount = 0x00261878u;
constexpr uint32_t kExtendedCount = 0x00261870u;
constexpr uint32_t kWrapCount = 0x0027C000u;

std::mutex g_timer_mutex;

bool read_u64(uint32_t address, uint64_t* value) {
    if (openrac_guest_base == nullptr || address > openrac_guest_ram_size ||
        openrac_guest_ram_size - address < sizeof(*value)) return false;
    std::memcpy(value, openrac_guest_base + address, sizeof(*value));
    return true;
}

bool write_u64(uint32_t address, uint64_t value) {
    if (openrac_guest_base == nullptr || address > openrac_guest_ram_size ||
        openrac_guest_ram_size - address < sizeof(value)) return false;
    std::memcpy(openrac_guest_base + address, &value, sizeof(value));
    return true;
}
} // namespace

extern "C" void openrac_lib_sly1_timer_snapshot(void) {
    std::lock_guard lock(g_timer_mutex);
    const uint32_t first_sample = openrac_guest_cp0_count();
    uint64_t wraps = 0;
    if (!read_u64(kWrapCount, &wraps) || !write_u64(kPreviousCount, first_sample)) return;

    const uint32_t second_sample = openrac_guest_cp0_count();
    if (second_sample < first_sample) ++wraps;
    if (!write_u64(kPreviousCount, second_sample) || !write_u64(kWrapCount, wraps)) return;
    const uint64_t extended = (wraps << 32) | second_sample;
    write_u64(kExtendedCount, extended);
}
