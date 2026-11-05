/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only scalar translation of the retail tagged direction-list query. */
#include "common.h"

void func_001EEBB0(void* state, const f32* direction, u32* result) {
    void* current = state != 0 ? *(void**)((u8*)state + 0x38) : 0;
    u32 visited;
    if (current == 0) {
        *result = 0xFFFFFFFFu;
        return;
    }
    if (((u64)current & 0x80000000u) != 0) {
        *result = (u32)((u64)current & 0x7FFFFFFFu);
        return;
    }

    for (visited = 0; visited < 0x10000u; ++visited) {
        const u8* record = (const u8*)current;
        f32 dot = direction[0] * *(const f32*)(record + 0x00) +
                  direction[1] * *(const f32*)(record + 0x04) +
                  direction[2] * *(const f32*)(record + 0x08);
        f32 threshold = *(const f32*)(record + 0x10);
        u32 value = *(const u32*)(record + 0x14);
        if (dot < threshold && (value & 0x80000000u) != 0) {
            *result = value & 0x7FFFFFFFu;
            return;
        }
        current = *(void* const*)(record + 0x18);
        if (current == 0 || ((u64)current & 0x80000000u) != 0) {
            *result = (u32)((u64)current & 0x7FFFFFFFu);
            return;
        }
    }
    *result = 0xFFFFFFFFu;
}
