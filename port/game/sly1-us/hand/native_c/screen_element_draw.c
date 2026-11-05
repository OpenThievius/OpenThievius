/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only backfill of the retail caption draw callback. */
#include "common.h"

typedef struct SlyScreenTextLayout {
    f32 x;
    f32 y;
    f32 w;
    f32 h;
    u32 color;
    s32 horizontal;
    s32 vertical;
} SlyScreenTextLayout;

typedef struct SlyScreenFont {
    s32 first;
    s32 advance;
    s32 height;
    s32 depth;
    f32 scale_stack[8];
    s32 f30;
    u8 pad34[4];
    u64 f38;
    s32 f40;
    f32 sx;
    f32 sy;
} SlyScreenFont;

extern u8 D_00262280[];
void func_00137228(SlyScreenTextLayout* out, f32 x, f32 y);
void func_00137238(SlyScreenTextLayout* out, f32 w, f32 h);
void func_00137248(SlyScreenTextLayout* out, void* color);
void func_00137260(SlyScreenTextLayout* out, s32 horizontal);
void func_00137268(SlyScreenTextLayout* out, s32 vertical);
void func_0015C8B8(void* font, f32 sx, f32 sy);
void func_0015C900(void* font);
void* func_0015DB28(void* output, s32 text_address, s32 font_address);
void func_0015E318(void* text, SlyScreenTextLayout* layout, s32 flag,
                   void* packet);
void func_0015C638(void* font, char* text, SlyScreenTextLayout* layout,
                   s32 flag, u32 packet);
extern u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2,
                                  u32 a3);

/* Retail's generic ScreenElement draw slot. The game installs this method
 * for captions, counters, prompts, and other text-bearing UI elements. */
void func_001AA790(void* opaque)
{
    u8* self = (u8*)opaque;
    SlyScreenTextLayout layout;
    s32 color;
    u32 frame;
    u32 object;

    if (self == 0 || self[8] == 0) return;
    func_00137228(&layout, *(f32*)(self + 0x21Cu),
                  *(f32*)(self + 0x220u));
    func_00137238(&layout, *(f32*)(self + 0x234u),
                  *(f32*)(self + 0x238u));
    color = *(s32*)(self + 0x208u);
    func_00137248(&layout, &color);
    func_00137260(&layout, 0);
    func_00137268(&layout, 0);

    frame = *(u32*)(self + 0x210u);
    object = frame != 0 ? *(u32*)frame : 0;
    if (object != 0) {
        u32 vtable = *(u32*)(object + 0x4Cu);
        if (vtable != 0) {
            u32 entry = vtable + 9u * 8u;
            s16 adjustment = *(s16*)entry;
            u32 draw = *(u32*)(entry + 4u);
            (void)sly1_native_call_regs4(draw, object + (s32)adjustment,
                                         frame, (u32)&layout,
                                         (u32)D_00262280);
        }
    }

    func_0015C8B8((void*)(u32)*(u32*)(self + 4u),
                  *(f32*)(self + 0x20Cu), *(f32*)(self + 0x20Cu));
    func_0015C638((void*)(u32)*(u32*)(self + 4u), (char*)(self + 8u),
                  &layout, 0, (u32)D_00262280);
    func_0015C900((void*)(u32)*(u32*)(self + 4u));
}

void func_001AB198(void* opaque)
{
    u8* self = (u8*)opaque;
    SlyScreenTextLayout layout;
    SlyScreenFont* font;
    u8 text_buffer[0xB0];
    u32 color;
    u8 alpha;
    f32 scale;
    f32 x;
    f32 y;

    if (self == 0 || self[8] == 0) return;

    scale = 1.0f;
    if (*(s32*)(0x0026FF68u + 0x250u) != 0)
        scale = *(f32*)(0x0026FF68u + 0x284u);
    color = *(u32*)(self + 0x208u);
    alpha = (u8)(s32)(scale * 255.0f);
    color = (color & 0x00FFFFFFu) | ((u32)alpha << 24);

    x = *(f32*)(self + 0x234u);
    y = *(f32*)(self + 0x238u);
    func_00137228(&layout, *(f32*)(self + 0x21Cu),
                  *(f32*)(self + 0x220u));
    func_00137238(&layout, x, y);
    func_00137248(&layout, &color);
    func_00137260(&layout, 0);
    func_00137268(&layout, 0);

    font = (SlyScreenFont*)(u32)*(u32*)(self + 4u);
    if (font == 0) return;
    func_0015C8B8(font, *(f32*)(self + 0x20Cu),
                  *(f32*)(self + 0x20Cu));
    func_0015DB28(text_buffer, (s32)(u32)(self + 8u), (s32)(u32)font);
    func_0015E318(text_buffer, &layout, 0, D_00262280);
    func_0015C900(font);
}
