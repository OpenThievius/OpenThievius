/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-side diagnostic translation of the retail animator record loader.
 * Keep this override behavior-equivalent to matched func_001B4A10. */
#include "common.h"

s32 func_00137BE0(void* reader);
void func_00137A60(void* reader, s32 size, void* destination);
s32 func_001B4920(s32 type);
void* func_001B4980(s32 type);
void* func_0018D5C0(s32 size);
s32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);
void sly1_native_loader_stage(u32 stage, u32 object, u32 reader);

void* func_001B4A10(void* reader)
{
    const s32 type = func_00137BE0(reader);
    void* object;
    void* methods;
    u32 params[7];

    if (type == 0) return 0;
    /* Retail's type tables cover 1..8. A malformed or not-yet-understood
     * record must still consume its fixed payload so the next record stays
     * aligned, but it cannot safely construct an object with a null vtable. */
    if (type < 1 || type > 8) {
        func_00137A60(reader, 0x1C, params);
        return 0;
    }
    object = func_0018D5C0(func_001B4920(type));
    methods = func_001B4980(type);
    if (object == 0 || methods == 0) {
        func_00137A60(reader, 0x1C, params);
        return 0;
    }
    sly1_native_loader_stage(171u, (u32)type, (u32)reader);
    sly1_native_loader_stage(172u, (u32)methods, (u32)object);
    *(u32*)((u8*)object + 0x00) = (u32)methods;
    *(s32*)((u8*)object + 0x08) = type;
    func_00137A60(reader, 0x1C, params);
    (void)sly1_native_call_regs4(*(u32*)methods, (u32)object,
                                 (u32)params, 0, 0);
    return object;
}
