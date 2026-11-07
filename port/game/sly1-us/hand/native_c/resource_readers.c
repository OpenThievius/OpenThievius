/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only resource-field readers, separate from matched decompilation C. */
#include "common.h"

void func_00137A60(void* reader, s32 size, void* destination);
void* func_0018D500(s32 size);
void* func_0018D568(s32 size, void* source);
s32 func_00137BE0(void* reader);
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

void func_00138550(void* reader, void* output) {
    u32 range[8];
    func_00137A60(reader, 0x20, range);
    *(u32*)output = range[1] != range[7]
        ? (u32)func_0018D568(0x20, range)
        : 0;
}

void func_001384A0(void* reader, u8** output) {
    const s32 size = func_00137BE0(reader);
    u8* buffer = (u8*)func_0018D500(size + 1);
    func_00137A60(reader, size, buffer);
    buffer[size] = 0;
    *output = buffer;
}
