// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only setup for the T217D health gauge. The retail routine's common
// initialization and initial bar bounds are reproduced here; its per-level
// animation tuning remains a decompilation target.
#include "common.h"

extern void func_001AA268(void* self);
void* func_0015C1C0(s32 index);
void func_0015C8B8(void* font, f32 x, f32 y);
extern "C" u8* D_002623C0;

extern "C" void func_001ABE70(void* self) {
    auto* bytes = static_cast<u8*>(self);
    func_001AA268(self);
    *(void**)(bytes + 0x04) = func_0015C1C0(4);

    // The retail default is a full gauge at unit scale.
    *(f32*)(bytes + 0x20C) = 1.0f;
    s32 progress_kind = 4;
    s32 progress_limit = 1;
    const s32 level = *(s32*)(D_002623C0 + 0x19D8);
    if (level == 4) {
        progress_kind = 0x0024CD70;
        progress_limit = 10;
    } else if (level == 5) {
        progress_limit = 5;
    }
    *(s32*)(bytes + 0x260) = progress_kind;
    *(s32*)(bytes + 0x264) = progress_limit;
    *(f32*)(bytes + 0x274) = 1.0f;
    *(f32*)(bytes + 0x278) = 1.0f;

    // Retail dimensions are stored as 18, 8, 26 and 340 and scaled by the
    // element's text scale when its gauge layout is initialized.
    const f32 scale = *(f32*)(bytes + 0x20C);
    void* font = *(void**)(bytes + 0x04);
    func_0015C8B8(font, scale, scale);
    *(f32*)(bytes + 0x280) = 18.0f * scale;
    *(f32*)(bytes + 0x284) = 8.0f * scale;
    *(f32*)(bytes + 0x288) = 26.0f * scale;
    *(f32*)(bytes + 0x28C) = 340.0f * scale;

    // Reset the cached progress value so the next update can animate to the
    // current level's value.
    *(f32*)(bytes + 0x27C) = 0.0f;
}
