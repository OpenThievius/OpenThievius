// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only guest-memory backfill for Sly's state-transition helper.
#include "common.h"

extern u8 D_00261858[];
extern u8 D_00264A40[];
extern void* D_00272930;
extern void* D_00272B98;
extern void* D_00275710;
extern u8 D_00275F80[];
extern s32 D_00274738;
extern void* D_0026C6C8;
extern void* D_0026C948;
extern void* D_0026CBC8;
extern void* D_0026CE48;

void func_0018C178(void* object, s32 value);
void func_001F0220(void* object, s32 value);
void func_001DDA50(void* object);
void func_001DDA20(void* object);
void func_001A7E70(void);
void func_001E98E8(f32 amount);

static s32 field_s32(void* object, s32 offset) {
    return *(s32*)((u8*)object + offset);
}

static f32 field_f32(void* object, s32 offset) {
    return *(f32*)((u8*)object + offset);
}

static void call_holder_method(void** holder, s32 byte_offset) {
    void* object = *holder;
    if (object == 0) return;
    void (*method)(void*) = *(void (**)(void*))((u8*)object + byte_offset);
    if (method != 0) method(holder);
}

void func_001E96F8(void* frame_state, s32 requested_state) {
    s32 state = requested_state;
    if (state == 3 && field_s32(frame_state, 0x30) != -1) {
        state = field_s32(frame_state, 0x30);
    }

    s32 previous = field_s32(frame_state, 4);
    if (previous == state) return;

    if (previous == 1) {
        call_holder_method(&D_00272930, 0x3C);
        call_holder_method(&D_00272B98, 0x3C);
        func_001DDA50(D_00275710);
    }

    *(s32*)((u8*)frame_state + 4) = state;
    *(f32*)((u8*)frame_state + 8) = field_f32(D_00261858, 0x10);
    if ((u32)state >= 9) return;

    switch (state) {
        case 0:
            if (field_s32(D_00264A40, 0x18) == 0) {
                func_0018C178(D_00264A40, 1);
            }
            func_001F0220(D_00275F80, 0);
            func_001E98E8(1.0f);
            break;
        case 1:
            func_001E98E8(1.0f);
            call_holder_method(&D_00272930, 0x38);
            call_holder_method(&D_00272B98, 0x38);
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
        case 4:
        case 5:
        case 6:
        case 7:
            func_001E98E8(0.0f);
            break;
        case 8:
            call_holder_method(&D_0026C6C8, 0x3C);
            call_holder_method(&D_0026C948, 0x3C);
            call_holder_method(&D_0026CBC8, 0x3C);
            call_holder_method(&D_0026CE48, 0x3C);
            break;
    }
}
