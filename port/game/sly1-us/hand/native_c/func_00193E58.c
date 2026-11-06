/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only initialization for the pause/title menu object. */
#include "common.h"

extern u8 D_0026A210[];
extern u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);
void sly1_native_trace_font(u32 stage, u32 a0, u32 a1, u32 a2, u32 a3,
                            u32 a4);

void func_00193E58(void* object) {
    u8* bytes = (u8*)object;
    (void)sly1_native_call_regs4(0x001AA268u, (u32)object, 0, 0, 0);
    if (sly1_native_call_regs4(0x0015C188u, 2, 0, 0, 0)) {
        *(u32*)(bytes + 0x210) = (u32)D_0026A210;
        *(u32*)D_0026A210 = sly1_native_call_regs4(0x0015C1C0u, 2, 0, 0, 0);
    }
    *(s32*)(bytes + 0x260) = -1;
    *(s32*)(bytes + 0x27C) = -1;
    *(s32*)(bytes + 0x268) = -1;
    *(s32*)(bytes + 0x26C) = -1;
    *(s32*)(bytes + 0x270) = -1;
    *(f32*)(bytes + 0x284) = 1.0f;
    sly1_native_trace_font(33, (u32)object, *(u32*)bytes,
                           (u32)*(s32*)(bytes + 0x260),
                           (u32)*(s32*)(bytes + 0x27C),
                           (u32)*(s32*)(bytes + 0x250));
}
