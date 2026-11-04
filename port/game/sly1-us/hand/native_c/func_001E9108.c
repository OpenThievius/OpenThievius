// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only guest-memory backfill for Sly's frame-state dispatcher.
#include "common.h"

extern u32 D_00262C68;
extern u32 D_00262C6C;
extern u8 D_00262C70[];
extern u8 D_00262D38[];
extern u8 D_00261858[];
extern u8 D_00264A40[];
extern void* D_002701B8;
extern u8 D_00275F80[];
extern void* D_002623C0;
extern u8 D_0026FF68[];
extern u8 D_0026F808[];

void func_0016E118(s32 mask);
void* func_001F4378(void* object);
void func_001F4808(void* resource);
void func_0016EC20(void* state, s32 mask);
void func_0018C568(void);
void func_001605E8(void* object);
void func_00160948(void);
void func_00194D30(void* object, s32 first, s32 count);
void func_001E9498(void* object);
void func_001E96F8(void* object, s32 state);
void func_001E98E8(f32 amount);
void func_001409E0(void);
void func_001A9FD0(void);

static s32 field_s32(void* object, s32 offset) {
    return *(s32*)((u8*)object + offset);
}

static u16 field_u16(void* object, s32 offset) {
    return *(u16*)((u8*)object + offset);
}

static f32 field_f32(void* object, s32 offset) {
    return *(f32*)((u8*)object + offset);
}

static void call_item_state(void (*callback)(void*, void*), void* item, void* state_data) {
    if (callback != 0) callback(item, state_data);
}

void func_001E9108(void* frame_state) {
    void* state_data = (D_00262C68 & 4u) != 0 ? D_00262C70 : D_00262D38;
    s32 state = field_s32(frame_state, 4);
    f32 elapsed = field_f32(D_00261858, 0x10) - field_f32(frame_state, 8);
    int repeat_transition = 0;

    if ((D_00262C6C & 0x80u) != 0 && field_f32(D_00261858, 4) > 0.1f) {
        func_0016E118(0x80);
    }

    switch (state) {
        case 0: {
            int should_present = 0;
            if (D_002701B8 == 0) {
                s32 mode = field_s32(D_00264A40, 0x18);
                should_present = mode == 2 ||
                    (mode == 1 && field_s32(D_00264A40, 0x0C) == 5);
            }
            if (should_present) {
                func_001F4808(func_001F4378(frame_state));
            }
            break;
        }
        case 1: {
            u16 flags = field_u16(state_data, 0xAA);
            if ((flags & 0x800u) != 0) {
                func_0016EC20(state_data, 0x800);
                s32 pending = field_s32(D_00264A40, 0x4F48);
                if (pending != 0) {
                    *(s32*)(D_00264A40 + 0x4F44) = pending;
                    func_0018C568();
                } else {
                    func_001605E8(D_002623C0);
                }
                func_00160948();
                state = field_s32(frame_state, 4);
            } else if ((flags & 0x100u) != 0) {
                func_0016EC20(state_data, 0x100);
                func_00194D30(D_0026FF68, 0, 0x17);
                state = field_s32(frame_state, 4);
            }
            break;
        }
        case 2:
        case 5:
        case 7:
            repeat_transition = 1;
            break;
        case 3: {
            if ((D_00262C6C & 2u) != 0 || field_s32(D_00275F80, 4) != 0) {
                break;
            }
            u16 flags = field_u16(state_data, 0xAA);
            int reset_scene = (flags & 0x800u) != 0;
            if (!reset_scene && field_s32(state_data, 0x14) == 1) {
                reset_scene = field_f32(D_00261858, 4) >= 5.0f;
            }
            if (reset_scene) {
                func_00194D30(D_0026FF68, 0, 0);
                state = field_s32(frame_state, 4);
                func_0016EC20(state_data, 0x800);
            } else if ((flags & 0x100u) != 0) {
                void* pending = *(void**)(D_0026F808 + 0x260);
                if (pending != 0 && field_s32(pending, 0x2D0) == 0) {
                    func_001E9498(frame_state);
                    state = field_s32(frame_state, 4);
                    func_0016EC20(state_data, 0x100);
                }
            }
            break;
        }
        case 4:
            if (elapsed <= 0.2f) {
                func_001E98E8(5.0f * (0.2f - elapsed));
            } else {
                state = 5;
            }
            break;
        case 6:
            if (elapsed <= 0.5f) {
                func_001E98E8(elapsed + elapsed);
            } else {
                state = 3;
            }
            break;
    }

    func_001E96F8(frame_state, state);

    s32 count = field_s32(frame_state, 0x2C);
    void* item = *(void**)((u8*)frame_state + 8 + count * 4);
    if (item != 0) {
        void** vtable = *(void***)item;
        void (*callback)(void*, void*) = (void (*)(void*, void*))vtable[4];
        call_item_state(callback, item, state_data);
    } else if (repeat_transition) {
        func_001E96F8(frame_state, 3);
    }

    func_001409E0();
    func_001A9FD0();
}
