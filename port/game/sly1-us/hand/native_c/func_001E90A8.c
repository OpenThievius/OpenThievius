/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only backfill for Sly's frame-state constructor callback. */
#include "common.h"

extern u8 D_00275BC0[];
void func_001E96F8(void* object, s32 state);

void func_001E90A8(void* object) {
    u8* state = (u8*)object;
    *(s32*)(state + 0x30) = -1;
    *(s32*)(state + 0x04) = -1;
    func_001E96F8(object, 3);
    *(s32*)(state + 0x0C) = 0;
    *(s32*)(state + 0x2C) = 1;
}

void func_001E9088(void) {
    func_001E90A8(D_00275BC0);
}
