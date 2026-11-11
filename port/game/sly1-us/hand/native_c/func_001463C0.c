/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only level-stream callback for state notifier objects. */
#include "common.h"

void func_0012AA28(void* object, s32 value);
void func_001289C0(void* object, void* reader);
void func_00124DF8(void* object, s32 count, void* links);
extern u8 D_002619A0[];
void sly1_native_trace_font(u32 stage, u32 a0, u32 a1, u32 a2, u32 a3,
                            u32 a4);

void func_001463C0(void* object, void* reader)
{
    static s32 trace_count;
    u32 object_id = *(u32*)((u8*)object + 0x18);
    u32 descriptor = *(u32*)object;
    s32 trace = trace_count < 4 && descriptor != 0 &&
        *(u32*)(descriptor + 4) == 0x57;
    if (trace) {
        ++trace_count;
        sly1_native_trace_font(86u, (u32)object, object_id,
            *(u32*)((u8*)reader + 0x18),
            (u32)*(s32*)((u8*)reader + 0x1C), descriptor);
    }
    func_0012AA28(object, 1);
    if (trace) {
        sly1_native_trace_font(87u, (u32)object,
            *(u32*)((u8*)reader + 0x18),
            (u32)*(s32*)((u8*)reader + 0x1C), object_id, 0x12AA28u);
    }
    func_001289C0(object, reader);
    if (trace) {
        sly1_native_trace_font(88u, (u32)object,
            *(u32*)((u8*)reader + 0x18),
            (u32)*(s32*)((u8*)reader + 0x1C), object_id, 0x1289C0u);
    }
    func_00124DF8(object, 5, D_002619A0);
    if (trace) {
        sly1_native_trace_font(89u, (u32)object,
            *(u32*)((u8*)reader + 0x18),
            (u32)*(s32*)((u8*)reader + 0x1C), object_id, 0x124DF8u);
    }
}
