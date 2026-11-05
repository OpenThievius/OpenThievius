/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only menu open/close state transition from the retail UI path. */
#include "common.h"

extern u8 D_00262C70[];
extern u8 D_00275BC0[];
extern void* D_002623C0;
extern s32 D_0027051C;
extern u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);

void func_001BED48(void);
void func_001BED70(void);
void func_001E96F8(void* object, s32 state);
void func_0016E0F0(s32 bits);
void func_0016E118(s32 bits);
void func_0016EC88(void* object);

void func_001940E8(void* object, s32 mode) {
    u8* self = (u8*)object;
    u8* input = (u8*)D_002623C0;
    s32 key;
    u32 vtable;
    u32 method;

    if (*(s32*)(self + 0x25C) == mode) return;
    key = (*(s32*)(input + 0x19D8) << 8) | *(s32*)(input + 0x19DC);

    vtable = *(u32*)object;
    if (mode != 0) {
        if (key == 0x308) {
            func_001BED48();
            func_001E96F8(D_00275BC0, 5);
        } else if (D_0027051C != 0) {
            func_001E96F8(D_00275BC0, 5);
        } else if (*(s32*)(D_00275BC0 + 4) == 3) {
            func_001E96F8(D_00275BC0, 4);
        } else {
            func_001E96F8(D_00275BC0, 5);
        }
        if (*(s32*)(self + 0x288) != 0) {
            func_001BE998();
            *(s32*)(self + 0x288) = 0;
        }
        method = vtable != 0 ? *(u32*)(vtable + 0x38) : 0;
        if (method != 0)
            (void)sly1_native_call_regs4(method, (u32)object, 0, 0, 0);
        func_0016E0F0(1);
        func_0016EC88(D_00262C70);
    } else {
        if (key == 0x308) {
            func_001BED70();
            func_001E96F8(D_00275BC0, 3);
        } else if (D_0027051C != 0 || *(s32*)(self + 0x260) != 0) {
            func_001E96F8(D_00275BC0, 3);
        } else {
            func_001E96F8(D_00275BC0, 6);
        }
        if (*(s32*)(self + 0x288) != 0) {
            func_001BE998();
            *(s32*)(self + 0x288) = 0;
        }
        method = vtable != 0 ? *(u32*)(vtable + 0x3C) : 0;
        if (method != 0)
            (void)sly1_native_call_regs4(method, (u32)object, 0, 0, 0);
        func_0016E118(1);
    }
    *(s32*)(self + 0x25C) = mode;
}
