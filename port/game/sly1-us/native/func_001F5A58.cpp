// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only byte-range copy used by the retail stream refill routines.
#include "common.h"

extern "C" void func_001F5A58(void* destination, const void* source, s32 size) {
    auto* dst = static_cast<u8*>(destination);
    const auto* src = static_cast<const u8*>(source);
    if (dst == src || size <= 0) return;

    const u32 dst_address = reinterpret_cast<u32>(dst);
    const u32 src_address = reinterpret_cast<u32>(src);
    if (dst_address < src_address || dst_address - src_address >= static_cast<u32>(size)) {
        for (s32 i = 0; i < size; ++i) dst[i] = src[i];
    } else {
        for (s32 i = size; i-- > 0;) dst[i] = src[i];
    }
}
