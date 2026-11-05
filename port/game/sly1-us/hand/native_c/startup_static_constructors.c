/* SPDX-License-Identifier: AGPL-3.0-or-later */
/*
 * Port-only backfills for retail static constructors that are not yet
 * available to hostgen. Their source behavior was checked against the latest
 * sly1-decomp; keeping these here lets matching C replace them cleanly later.
 */
#include "common.h"

typedef struct UiConfig {
    u8 pad0[4];
    u8 kind;
    u8 pad5[3];
    f32 scale;
    f32 width;
    f32 height;
    f32 depth;
    s32 color;
} UiConfig;

extern UiConfig D_002758E0;
extern UiConfig D_00276620;
extern f32 D_00276648;
extern f32 D_00276650;
extern u8 D_00275B50[8];
extern u8 D_00262988[];
extern u8 D_002629A8[];
extern u8 D_00275AF0[];
u8* func_001F5C0C(u8* destination, s32 value, s32 byte_count);

void func_001E0848(void* self, u32 mode) {
    if (self == 0 || mode != 0xFFFFu) return;
    (void)func_001F5C0C((u8*)&D_002758E0, 0, 0x1C);
    D_002758E0.kind = 0x2D;
    D_002758E0.scale = 2.0f;
    D_002758E0.width = 1.0f;
    D_002758E0.height = 0.3f;
    D_002758E0.depth = 0.3f;
    D_002758E0.color = (s32)0xFF7D4B00u;
}

void func_001E08D8(void) {
    func_001E0848((void*)1, 0xFFFFu);
}

s32 func_001E8878(s32 enabled, u32 value) {
    if (value == 0xFFFFu && enabled != 0) {
        D_00275B50[0] = 0x80;
        D_00275B50[1] = 0x80;
        D_00275B50[2] = 0x80;
        D_00275B50[3] = 0x80;
    }
    return 0;
}

void func_001E88B0(void) {
    (void)func_001E8878(1, 0xFFFFu);
}

/* Static constructor for D_00275AF0 (Ctor_1E4E38 in sly1-decomp). */
void func_001E5358(void) {
    *(s32*)(D_00275AF0 + 4) = 0;
    *(s32*)(D_00275AF0 + 0x10) = 0;
    *(s32*)(D_00275AF0 + 0x0C) = -1;
    *(s32*)(D_00275AF0 + 8) = -1;
    (void)func_001F5C0C(D_00275AF0 + 0x14, 0, 0x20);
}

void func_001F2688(s32 enabled, s32 value) {
    if (enabled == 0 || value != 0xFFFF) return;
    (void)func_001F5C0C((u8*)&D_00276620, 0, 0x1C);
    D_00276620.color = (s32)0xFF7D4B00u;
    D_00276650 = D_00276648 * 0.75f;
    D_00276620.scale = 2.0f;
    D_00276620.width = 1.0f;
    D_00276620.height = 0.3f;
    D_00276620.depth = 0.3f;
    D_00276620.kind = 0x3D;
}

void func_001F2738(void) {
    func_001F2688(1, 0xFFFF);
}

static void initialize_input_style(u8* config) {
    (void)func_001F5C0C(config, 0, 0x1C);
    config[4] = 0x2D;
    *(f32*)(config + 8) = 2.0f;
    *(f32*)(config + 0x0C) = 1.0f;
    *(f32*)(config + 0x10) = 0.3f;
    *(f32*)(config + 0x14) = 0.3f;
    *(s32*)(config + 0x18) = (s32)0xFF7D4B00u;
}

void func_0016C360(s32 enabled, s32 value) {
    if (enabled == 0 || value != 0xFFFF) return;
    initialize_input_style(D_00262988);
    initialize_input_style(D_002629A8);
}

void func_0016C450(void) {
    func_0016C360(1, 0xFFFF);
}
