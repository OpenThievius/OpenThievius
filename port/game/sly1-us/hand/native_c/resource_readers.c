/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only resource-field readers, separate from matched decompilation C. */
#include "common.h"

void func_00137A60(void* reader, s32 size, void* destination);
void* func_0018D500(s32 size);
void* func_0018D568(s32 size, void* source);
s32 func_00137BE0(void* reader);
void func_0018D800(void* destination, void* source, s32 size);

void* func_0018D568(s32 size, void* source) {
    void* result = func_0018D500(size);
    if (result != 0) func_0018D800(result, source, size);
    return result;
}

/* Port-only safety backfill for corrupt stream counts. Retail returns the
 * raw word; native startup must not let an invalid asset count drive an
 * unbounded translated loop or wrap an allocation size. Keep the threshold
 * diagnostic and isolated here so matching decomp C can replace this later. */
u32 func_00137C40(void* reader) {
    u8* bytes = (u8*)reader;
    s32 remaining = *(s32*)(bytes + 0x1C);
    u32 value = 0;
    if (remaining >= 4) {
        const u8* cursor = (const u8*)*(u32*)(bytes + 0x18);
        value = (u32)cursor[0] | ((u32)cursor[1] << 8) |
                ((u32)cursor[2] << 16) | ((u32)cursor[3] << 24);
        *(u32*)(bytes + 0x18) += 4u;
        *(s32*)(bytes + 0x1C) = remaining - 4;
    } else {
        extern void func_00137A60(void*, s32, void*);
        func_00137A60(reader, 4, &value);
    }
    if (value > 0x100000u) {
        extern void sly1_native_loader_stage(u32, u32, u32);
        sly1_native_loader_stage(161u, value, (u32)reader);
        return 0;
    }
    return value;
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
