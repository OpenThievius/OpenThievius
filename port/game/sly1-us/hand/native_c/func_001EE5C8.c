/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only repair for the unmatched retail packet cursor helper.
 * Kept separate so matching decomp C can replace it when available. */
#include "common.h"

s32 func_001EE560(u32 writer, s32 type, s32 count, s32 flags);
void sly1_native_trace_font(u32 stage, u32 a0, u32 a1, u32 a2, u32 a3,
                            u32 a4);

void func_001EE5C8(u32 writer, u32 type, s32 count, u32 flags,
                   u32 out_size, u32 out_cursor) {
    s32 size;
    u32 cursor;

    sly1_native_trace_font(65u, writer, type, (u32)count,
                           out_size, out_cursor);

    /* A negative count means the upstream strip range is malformed. Retail
     * arithmetic would wrap the writer cursor; leave this packet empty while
     * the port-side loader/relocation path is being corrected. */
    if (count <= 0) {
        if (out_cursor != 0) *(u32*)out_cursor = 0;
        if (out_size != 0) *(s32*)out_size = 0;
        return;
    }

    size = func_001EE560(writer, (s32)type, count, (s32)flags);
    cursor = *(u32*)(writer + 0x0Cu);
    if (out_cursor != 0) *(u32*)out_cursor = cursor;
    cursor += ((u32)size + 3u) & ~3u;
    *(u32*)(writer + 0x0Cu) = cursor;
    if (out_size != 0) *(s32*)out_size = size;
}
