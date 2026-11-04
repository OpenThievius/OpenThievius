// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only guest-memory backfill for Sly's persistent frame loop.
#include "common.h"

extern void* D_00264834;
extern void* D_00264838;
extern u32 D_002622CC;
extern void* D_00275AF0;
extern void* D_00275710;
extern void* D_00261990;
extern u8 D_00275BC0[];
extern u8 D_00269A00[];
extern u8 D_00275F80[];
extern u8 D_00262C70[];
extern u8 D_00261858[];
extern u8 D_00264A40[];

void func_001FAE18(void);
void func_00185B38(void);
void func_0015F210(s32 mode);
void func_0018F148(void* object);
void func_001E4EA8(void* resource_state);
void func_0016E238(void* object);
void func_0016F2E0(void);
void func_0018BDC8(void* object);
void func_001E9108(void* frame_state);
void func_001607D0(f32 delta);
void func_00144270(void* object);
void func_0015E9F0(void);
void func_00142F10(void* timer);
void func_0019D6C0(void* object, void* player);
void func_001E9668(void* state_data);
void func_0019DC98(void* object, void* player);
void func_001E9648(void* state_data);
void func_0015EC68(void);

static s32 field_s32(void* object, s32 offset) {
    return *(s32*)((u8*)object + offset);
}

static f32 field_f32(void* object, s32 offset) {
    return *(f32*)((u8*)object + offset);
}

void func_00185758(void* game_state, void* secondary_state) {
    func_001FAE18();
    D_00264834 = game_state;
    D_00264838 = secondary_state;
    func_00185B38();

    for (;;) {
        if (field_s32(D_00269A00, 4) != 0 && field_s32(D_00275F80, 4) != 1) {
            func_0015F210(1);
            func_0018F148(D_00269A00);
        }

        if (D_00275AF0 != 0) {
            func_0015F210(1);
            func_001E4EA8(&D_00275AF0);
        }

        if (field_s32(D_00269A00, 4) != 0 && field_s32(D_00275F80, 4) != 1) {
            func_0015F210(1);
            func_0018F148(D_00269A00);
        }

        func_0016E238(D_00262C70);
        func_0016F2E0();
        func_0018BDC8(D_00264A40);
        func_001E9108(D_00275BC0);

        f32 delta = field_f32(D_00261858, 8);
        func_001607D0(delta);

        if (D_00275710 != 0) {
            func_00144270(D_00261990);
            func_0015E9F0();
            func_00142F10(D_00261858);

            void** vtable = *(void***)D_00275710;
            void (*update)(void*, void*, f32) = (void (*)(void*, void*, f32))vtable[0x54 / 4];
            f32 callback_delta = field_f32(D_00261858, 8);
            if (update != 0) update(D_00275710, D_00261990, callback_delta);

            func_0019D6C0(D_00275710, D_00261990);
            func_001E9668(D_00275BC0);
            func_0019DC98(D_00275710, D_00261990);
            func_001E9648(D_00275BC0);
            func_0015EC68();
        }

        ++D_002622CC;
    }
}
