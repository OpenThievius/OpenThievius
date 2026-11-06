/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only screen-fade backfills copied from the current Sly 1 decomp C. */
#include "common.h"

typedef struct ScreenFadeNative {
    s32 unused_00;
    s32 mode;
    f32 mode_start_time;
    f32 opacity;
    u32 active[5];
    s32 style;
    u32 queued[5];
    s32 queued_style;
} ScreenFadeNative;

typedef char check_ScreenFadeNative[
    sizeof(ScreenFadeNative) == 0x40 ? 1 : -1];

extern f32 D_00261858[];
extern void* D_00275710;
extern s32 D_00275AF0;
extern s32 D_00275F84;
extern u32 D_002640A4;
extern u8 D_00275B04[];

extern s32 func_001DDA80(void* world);
extern void func_0016EC20(void* pad, s32 mask);
extern void func_001F0220(void* fade, s32 mode);
extern void func_001C0000(s32 flag, f32 value);
extern void func_0019DDA0(f32 x0, f32 y0, f32 x1, f32 y1,
                          s32 r, s32 g, s32 b, s32 a, void* packet);
extern void func_00180398(u32 triangle_set, f32 opacity);

/* Backfill of retail RestoreFadeRequest (func_001F00F0). */
s32 func_001F00F0(void* opaque, void* pad_opaque, s32 param2) {
    ScreenFadeNative* fade = (ScreenFadeNative*)opaque;
    u8* pad = (u8*)pad_opaque;
    u32* active_key;
    s32 same_key;

    if (fade == 0 || pad == 0 || func_001DDA80(D_00275710) == 0 ||
        fade->queued[0] == 0 || (*(u16*)(pad + 0xAAu) & 0x0800u) == 0)
        return 0;

    func_0016EC20(pad, 0x800);
    for (u32 i = 0; i < 5; ++i) fade->active[i] = fade->queued[i];
    fade->style = fade->queued_style;

    active_key = (u32*)fade->active[1];
    same_key = active_key == 0;
    if (!same_key && D_00275B04 != 0) {
        const u32* default_key = (const u32*)D_00275B04;
        same_key = ((active_key[2] ^ active_key[6]) ==
                    (default_key[2] ^ default_key[6]));
    }
    if (same_key) fade->active[4] |= 8u;
    else fade->active[4] &= ~8u;

    func_001F0220(fade, param2);
    return 1;
}

/* Backfill of retail UpdateScreenFade (func_001EFD10). */
void func_001EFD10(void* opaque, void* pad) {
    ScreenFadeNative* fade = (ScreenFadeNative*)opaque;
    s32 mode;
    f32 elapsed;
    f32 threshold;
    f32 ratio;

    if (fade == 0) return;
    mode = fade->mode;
    elapsed = D_00261858[4] - fade->mode_start_time;

    switch (mode) {
    case 0:
        if (func_001F00F0(fade, pad, 1) != 0) return;
        break;
    case 1:
        if (fade->style != 2) {
            threshold = fade->style == mode ? 1.5f : 0.5f;
            if (threshold < elapsed) {
                mode = 2;
                func_001C0000(((fade->active[4] >> 3) ^ 1u) & 1u, 0.0f);
            } else {
                ratio = elapsed / threshold;
                if (ratio < 0.0f) ratio = 0.0f;
                else if (ratio > 1.0f) ratio = 1.0f;
                fade->opacity = ratio;
                func_001C0000(((fade->active[4] >> 3) ^ 1u) & 1u,
                              1.0f - ratio);
            }
        }
        break;
    case 3: {
        const s32 style = fade->style;
        if (style != 2) {
            threshold = style == 1 ? 1.0f : 0.25f;
            if (threshold < elapsed) {
                if (style == mode) {
                    fade->style = 0;
                    fade->mode_start_time = D_00261858[4];
                    fade->opacity = 1.0f;
                } else {
                    mode = 0;
                }
            } else {
                fade->opacity = 1.0f - elapsed / threshold;
            }
        }
        break;
    }
    case 2:
        if (fade->active[0] != 0 || D_00275AF0 == 0) mode = 3;
        break;
    default:
        break;
    }

    func_001F0220(fade, mode);
}

/* Backfill of retail DrawScreenFade (func_001EFEE0). */
void func_001EFEE0(void* opaque) {
    ScreenFadeNative* fade = (ScreenFadeNative*)opaque;
    if (fade == 0 || D_00275710 == 0 || D_00275F84 == 0) return;

    if (fade->style == 1 && D_002640A4 != 0) {
        func_00180398(D_002640A4, fade->opacity);
        return;
    }
    if (fade->style != 0 && fade->style != 1) return;

    func_0019DDA0(0.0f, 0.0f, 640.0f, 492.80002f,
                  0, 0, 0, (s32)(fade->opacity * 255.0f),
                  (void*)0x00262280u);
}
