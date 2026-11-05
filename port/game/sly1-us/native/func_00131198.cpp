// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only setup for the dialogue caption T217D element. The US decompilation
// marks the matching handwritten method as ASM_FUNC; this initializes its
// known native fields so startup can construct the caption widget.
#include "common.h"

void func_001310F0(void* self, s32 index);

extern "C" void func_00131198(void* self) {
    auto* bytes = static_cast<u8*>(self);
    const s32 index = *(s32*)(bytes + 0x214);
    func_001310F0(self, index);

    *(s32*)(bytes + 0x260) = 0;
    *(f32*)(bytes + 0x264) = 0.0f;
    *(f32*)(bytes + 0x268) = 0.0f;
    *(s32*)(bytes + 0x26C) = 0;
    for (s32 i = 0; i < 16; ++i)
        *(s32*)(bytes + 0x270 + i * 4) = 0;

    *(f32*)(bytes + 0x2F0) = 15.0f;
    *(u32*)(bytes + 0x2F4) = 0x80FFFFFFu;
    *(u8*)(bytes + 0x2F8) = 0;
    *(s32*)(bytes + 0x2FC) = 0;
    *(f32*)(bytes + 0x300) = 0.0f;
    *(f32*)(bytes + 0x304) = 0.0f;
    *(f32*)(bytes + 0x308) = 0.0f;
    *(f32*)(bytes + 0x30C) = 0.0f;
    *(f32*)(bytes + 0x314) = 0.4f;
    *(s32*)(bytes + 0x320) = 0;
    *(void**)(bytes + 0x324) = nullptr;
    *(s32*)(bytes + 0x328) = 0;
    *(s32*)(bytes + 0x32C) = 0;
    for (s32 i = 0; i < 8; ++i)
        *(s32*)(bytes + 0x330 + i * 4) = 0;
    *(s32*)(bytes + 0x350) = 0;
    *(s32*)(bytes + 0x354) = 0;
}
