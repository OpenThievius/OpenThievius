/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only scalar translation of the retail value-tree point lookup. */
#include "common.h"

static u32 ExpandValueLeaf(void* state, u32 bits) {
    s32 count = *(s32*)((u8*)state + 0x3C);
    u32* values = *(u32**)((u8*)state + 0x40);
    u32 expanded = 0xFFFFFFFFu;
    u32 i;
    bits &= 0x7FFFFFFFu;
    if (bits == 0) return 0;
    if (count < 0 || count > 32 || (count != 0 && values == 0))
        return 0xFFFFFFFFu;
    for (i = 0; i < (u32)count; ++i) {
        if ((bits & (1u << i)) != 0)
            expanded &= values[i];
    }
    return expanded | bits;
}

void func_001EEBB0(void* state, const f32* direction, u32* result) {
    u32 current = state != 0 ? *(u32*)((u8*)state + 0x38) : 0;
    u32 visited;
    u32 node_limit;
    if (current == 0) {
        *result = 0xFFFFFFFFu;
        return;
    }
    if ((current & 0x80000000u) != 0) {
        *result = ExpandValueLeaf(state, current);
        return;
    }

    node_limit = *(u32*)((u8*)state + 0x34);
    if (node_limit == 0 || node_limit > 4096u) node_limit = 4096u;
    for (visited = 0; visited <= node_limit; ++visited) {
        const u8* record = (const u8*)current;
        f32 dot = direction[0] * *(const f32*)(record + 0x00) +
                  direction[1] * *(const f32*)(record + 0x04) +
                  direction[2] * *(const f32*)(record + 0x08);
        f32 threshold = *(const f32*)(record + 0x10);
        u32 below = *(const u32*)(record + 0x14);
        u32 above = *(const u32*)(record + 0x18);
        current = dot < threshold ? below : above;
        if (current == 0 || (current & 0x80000000u) != 0) {
            *result = current == 0 ? 0xFFFFFFFFu : ExpandValueLeaf(state, current);
            return;
        }
    }
    *result = 0xFFFFFFFFu;
}
