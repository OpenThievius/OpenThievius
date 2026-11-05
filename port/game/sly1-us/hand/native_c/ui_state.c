/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only startup overlay state callbacks, matching retail state transitions. */
#include "common.h"

extern f32 D_0026185C[];
extern f32 D_00261868[];
extern s32 D_00274738;
extern u8 D_00272930[];
extern u8 D_00272B98[];
extern u8 D_0026C6C8[];
extern u8 D_0026C948[];
extern u8 D_0026CBC8[];
extern u8 D_0026CE48[];
extern u8 D_00264A40[];
extern u8 D_00275F80[];
extern void* D_00275710;

void func_0018C178(void* object, s32 value);
void func_001F0220(void* object, s32 value);
void func_001DDA50(void* object);
void func_001DDA20(void* object);
void func_001A7E70(void);
void func_001C06A0(s32 a, s32 b, f32 value);
void func_00142ED0(f32 value);
extern u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);

static void call_overlay_method(u32 object_address, u32 slot) {
    u32 object = object_address;
    u32 vtable;
    u32 method;
    if (object == 0) return;
    vtable = *(u32*)object;
    if (vtable == 0) return;
    method = *(u32*)(vtable + slot);
    if (method != 0)
        (void)sly1_native_call_regs4(method, object, 0, 0, 0);
}

void func_001E98E8(f32 value) {
    if (D_0026185C[0] > 0.0f) {
        const f32 blend = (1.0f - value) * 0.6f + value;
        func_001C06A0(4, 1, blend);
        func_001C06A0(4, 0, blend);
    }
    func_00142ED0(value);
}

void func_001E96F8(void* opaque, s32 state) {
    u8* self = (u8*)opaque;
    s32 prior;

    if (state == 3) {
        const s32 fallback = *(s32*)(self + 0x30);
        if (fallback != -1) state = fallback;
    }
    prior = *(s32*)(self + 4);
    if (prior == state) return;

    if (prior == 1) {
        call_overlay_method(0x00272930u, 0x3Cu);
        call_overlay_method(0x00272B98u, 0x3Cu);
        func_001DDA50(D_00275710);
    }

    *(s32*)(self + 4) = state;
    *(f32*)(self + 8) = D_00261868[0];
    switch (state) {
    case 0:
        if (*(s32*)(D_00264A40 + 0x18) == 0)
            func_0018C178(D_00264A40, 1);
        func_001F0220(D_00275F80, 0);
        func_001E98E8(1.0f);
        break;
    case 1:
        func_001E98E8(1.0f);
        call_overlay_method(0x00272930u, 0x38u);
        call_overlay_method(0x00272B98u, 0x38u);
        func_001DDA20(D_00275710);
        func_001A7E70();
        break;
    case 2:
        func_001E98E8(1.0f);
        break;
    case 3:
        func_001E98E8(1.0f);
        D_00274738 = -100;
        break;
    case 5:
    case 7:
        func_001E98E8(0.0f);
        break;
    case 8:
        call_overlay_method(0x0026C6C8u, 0x3Cu);
        call_overlay_method(0x0026C948u, 0x3Cu);
        call_overlay_method(0x0026CBC8u, 0x3Cu);
        call_overlay_method(0x0026CE48u, 0x3Cu);
        break;
    default:
        break;
    }
}

void func_001E90E8(void* object) {
    func_001E96F8(object, 3);
}
