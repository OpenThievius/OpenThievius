/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Sly's CPU-visible DMA helper: copy RAM spans immediately on the host. */
#include "openrac/native_dma.h"

#include "openrac/guest.h"

#include <cstring>

namespace {
bool ram_range(uint32_t address, uint32_t size, uint32_t* offset) {
    const uint32_t normalized = openrac_guest_ram_address(address);
    if (normalized > openrac_guest_ram_size || size > openrac_guest_ram_size - normalized)
        return false;
    *offset = normalized;
    return true;
}
}

extern "C" void openrac_lib_sly1_dma_reset(int unused) {
    (void)unused;
    // Native memory operations have no outstanding EE DMA queue to reset.
}

extern "C" int openrac_lib_sly1_dma_status(uint32_t handle) {
    (void)handle;
    // Submission is synchronous, so every successful handle is complete.
    return -1;
}

extern "C" uint32_t openrac_lib_sly1_dma_submit(uint32_t descriptors, int count) {
    if (count <= 0 || count > 128) return 0;
    uint32_t descriptor_offset = 0;
    const uint32_t descriptor_bytes = static_cast<uint32_t>(count) * 16u;
    if (!ram_range(descriptors, descriptor_bytes, &descriptor_offset)) {
        openrac_guest_missing("Sly DMA descriptor range is outside guest RAM");
        return 0;
    }

    for (int i = 0; i < count; ++i) {
        uint32_t fields[4];
        std::memcpy(fields, openrac_guest_base + descriptor_offset +
                                static_cast<uint32_t>(i) * sizeof(fields), sizeof(fields));
        const uint32_t byte_count = fields[2];
        uint32_t source = 0;
        uint32_t destination = 0;
        if (!ram_range(fields[0], byte_count, &source) ||
            !ram_range(fields[1], byte_count, &destination)) {
            openrac_guest_missing("Sly DMA span targets a non-RAM device");
            return 0;
        }
        std::memmove(openrac_guest_base + destination, openrac_guest_base + source, byte_count);
    }
    return 1;
}
