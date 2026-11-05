/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only scalar translation of the retail VU0 direction-list query. */
#include "sly1/host_abi.h"

struct Vec4_001EEBB0 { f32 v[4]; };

extern "C" void func_001EEBB0(void* state, const Vec4_001EEBB0* direction,
                                u32* result)
    __asm__("_Z13func_001EEBB0PvS_S_");

extern "C" void func_001EEBB0(void* state, const Vec4_001EEBB0* direction,
                                u32* result) {
    u32 current = state != nullptr
        ? *reinterpret_cast<const u32*>(static_cast<const u8*>(state) + 0x38u)
        : 0;
    if (current == 0) {
        *result = 0xFFFFFFFFu;
        return;
    }
    if ((current & 0x80000000u) != 0) {
        *result = current & 0x7FFFFFFFu;
        return;
    }

    for (u32 visited = 0; visited < 0x10000u; ++visited) {
        const u8* record = reinterpret_cast<const u8*>(current);
        const f32 dot = direction->v[0] * *reinterpret_cast<const f32*>(record + 0x00u) +
                        direction->v[1] * *reinterpret_cast<const f32*>(record + 0x04u) +
                        direction->v[2] * *reinterpret_cast<const f32*>(record + 0x08u);
        const f32 threshold = *reinterpret_cast<const f32*>(record + 0x10u);
        const u32 value = *reinterpret_cast<const u32*>(record + 0x14u);
        if (dot < threshold && (value & 0x80000000u) != 0) {
            *result = value & 0x7FFFFFFFu;
            return;
        }

        current = *reinterpret_cast<const u32*>(record + 0x18u);
        if (current == 0 || (current & 0x80000000u) != 0) {
            *result = current & 0x7FFFFFFFu;
            return;
        }
    }

    /* Retail list links terminate with a tagged word. Avoid an infinite host
     * loop if a malformed resource supplies a cyclic chain. */
    *result = 0xFFFFFFFFu;
}
