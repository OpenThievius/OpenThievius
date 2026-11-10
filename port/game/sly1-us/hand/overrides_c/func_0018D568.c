/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port override for retail resource copying, separate from native backfills. */
#include "common.h"

void* func_0018D500(s32 size);
void func_0018D800(void* destination, void* source, s32 size);

void* func_0018D568(s32 size, void* source) {
    const u32 source_address = (u32)source;
    void* result = func_0018D500(size);
    if (source_address >= 0x02000000u && source_address < 0x70000000u) {
        extern void sly1_native_trace_font(u32, u32, u32, u32, u32, u32);
        sly1_native_trace_font(71u, (u32)size, source_address,
                               (u32)result, 0, 0);
    }
    if (result != 0) func_0018D800(result, source, size);
    return result;
}
