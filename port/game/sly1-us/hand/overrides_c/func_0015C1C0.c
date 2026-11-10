/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only diagnostic backfill; replace when this function is decompiled. */
#include "common.h"

extern u32 D_00262268[];
extern s32 D_0026227C;
extern void sly1_native_trace_font(u32 stage, u32 a, u32 b, u32 c, u32 d, u32 e);

s32 func_0015C1C0(s32 index) {
    static u32 trace_count;
    u32 entry = 0;
    s32 result;

    if (index >= 0 && index < 256) entry = D_00262268[index];
    result = index == -1 ? 0 : (entry != 0 ? (s32)entry : D_0026227C);
    if (trace_count < 24u) {
        sly1_native_trace_font(70u, (u32)index, entry,
                               (u32)D_0026227C, (u32)result,
                               (u32)D_00262268);
        ++trace_count;
    }
    return result;
}
