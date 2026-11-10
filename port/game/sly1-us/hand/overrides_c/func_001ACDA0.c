/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only guard for menu construction before the font registry is ready. */
#include "common.h"

typedef void* (*FontScaleFn)(void* self, f32 sx, f32 sy);
typedef struct FontVTable {
    s32 word0;
    s32 word1;
    s16 delta;
    s16 word2;
    FontScaleFn scale;
} FontVTable;
typedef struct FontObject {
    u8 pad[0x4C];
    FontVTable* vtable;
} FontObject;

void func_001AA268(void* self);
s32 func_0015C188(s32 index);
void* func_0015C1C0(s32 index);
extern u32 D_00274410[];

void func_001ACDA0(void* object) {
    u8* self = (u8*)object;
    FontObject* font;
    FontVTable* vtable;

    func_001AA268(object);
    font = (FontObject*)func_0015C1C0(0);
    if (font == 0 || font->vtable == 0 || font->vtable->scale == 0) {
        *(u32*)(self + 0x04) = 0;
        *(f32*)(self + 0x20C) = 0.8f;
        *(f32*)(self + 0x248) = 0.5f;
        return;
    }

    vtable = font->vtable;
    *(u32*)(self + 0x04) = (u32)vtable->scale(
        (u8*)font + vtable->delta, 1.0f, 1.0f);
    *(f32*)(self + 0x20C) = 0.8f;
    *(f32*)(self + 0x248) = 0.5f;
    if (func_0015C188(2)) {
        *(u32*)(self + 0x210) = (u32)D_00274410;
        *D_00274410 = (u32)func_0015C1C0(2);
    }
}
