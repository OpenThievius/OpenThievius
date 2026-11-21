// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

// Read-only view of the retail image records used by the native host renderer.
// The guest owns the pointed-to bytes; callers must copy them before retaining.
struct NativeSlyTextureRecords {
    std::uint32_t image_width = 0;
    std::uint32_t image_height = 0;
    std::uint32_t image_tbw = 0;
    std::uint32_t image_psm = 0;
    std::uint32_t image_bytes = 0;
    const std::uint8_t* image_data = nullptr;
    std::uint32_t palette_entries = 0;
    std::uint32_t palette_bytes = 0;
    const std::uint8_t* palette_data = nullptr;
};

extern "C" int sly1_native_get_texture_records(
    std::uint32_t tbp, std::uint32_t cbp, std::uint32_t psm,
    NativeSlyTextureRecords* records);
