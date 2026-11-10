/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only mesh table reader for the still-unmatched retail entry. */
#include "sly1/host_abi.h"
#include "native_function_lookup.hpp"

namespace {

template <typename Function>
Function retail_function(u32 address) {
    return reinterpret_cast<Function>(sly1::native_function_address(address));
}

s32 read_u16(void* stream) {
    auto read = retail_function<s32 (*)(void*)>(0x00137BE0u);
    return read != nullptr ? read(stream) : 0;
}

s32 read_s16(void* stream) {
    auto read = retail_function<s32 (*)(void*)>(0x00137D10u);
    return read != nullptr ? read(stream) : -1;
}

f32 read_float(void* stream) {
    auto read = retail_function<f32 (*)(void*)>(0x00137DF8u);
    return read != nullptr ? read(stream) : 0.0f;
}

void read_vector3(void* stream, void* out) {
    auto read = retail_function<void (*)(void*, void*)>(0x00137E70u);
    if (read != nullptr) read(stream, out);
}

void* allocate_front(s32 bytes) {
    auto allocate = retail_function<void* (*)(s32)>(0x0018D500u);
    return allocate != nullptr ? allocate(bytes) : nullptr;
}

} // namespace

extern "C" s32 func_001E4C48(void* stream, void* point_count_out,
                                void* points_out, void* link_count_out,
                                void* links_out) {
    const s32 point_count = read_u16(stream);
    auto* points = static_cast<u8*>(allocate_front(point_count * 0x20));
    if (point_count > 0 && points == nullptr) return -1;

    for (s32 i = 0; i < point_count; ++i) {
        u8* point = points + static_cast<u32>(i) * 0x20u;
        *reinterpret_cast<f32*>(point) = read_float(stream);
        read_vector3(stream, point + 0x10u);
    }

    const s32 link_count = read_u16(stream);
    auto* links = static_cast<u8*>(allocate_front(link_count * 0x0Cu));
    if (link_count > 0 && links == nullptr) return -1;

    for (s32 i = 0; i < link_count; ++i) {
        u8* link = links + static_cast<u32>(i) * 0x0Cu;
        const s32 point_index = read_u16(stream);
        *reinterpret_cast<u32*>(link) =
            reinterpret_cast<u32>(points + static_cast<u32>(point_index) * 0x20u);

        const s32 a_index = read_s16(stream);
        *reinterpret_cast<u32*>(link + 4u) = a_index == -1 ? 0u :
            reinterpret_cast<u32>(links + static_cast<u32>(a_index) * 0x0Cu);

        const s32 b_index = read_s16(stream);
        *reinterpret_cast<u32*>(link + 8u) = b_index == -1 ? 0u :
            reinterpret_cast<u32>(links + static_cast<u32>(b_index) * 0x0Cu);
    }

    *static_cast<s32*>(point_count_out) = point_count;
    *static_cast<void**>(points_out) = points;
    *static_cast<s32*>(link_count_out) = link_count;
    *static_cast<void**>(links_out) = links;
    return 0;
}
