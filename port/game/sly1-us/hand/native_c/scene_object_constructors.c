/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only scene-object constructor chain translated from US retail. */
#include "common.h"

void func_00152438(void* list, s32 link_offset);
void func_00152470(void* list, void* object);
void func_00182C18(void* object);

s32 func_00140FF8(s32* counter)
{
    return ++*counter;
}

void func_00123ED0(void* object)
{
    u8* self = (u8*)object;
    const u8* defaults = (const u8*)0x002583D0u;
    u32 flags = *(u32*)(self + 0x2C8);

    func_00152438(self + 0x34, 0x1C);
    func_00152438(self + 0x60, 0x58);
    if (*(u32*)(self + 0x18) != 0) {
        flags = (flags & 0xFCFFFFFFu) | 0x01000000u;
        flags &= 0xF3FFFFFFu;
    } else {
        flags = (flags & 0xFCFFFFFFu) | 0x02000000u;
        flags = (flags & 0xF3FFFFFFu) | 0x08000000u;
    }
    *(u32*)(self + 0x2C8) = flags;
    func_00182C18(object);

    *(f32*)(self + 0x84) = *(const f32*)0x002599F0u;
    *(f32*)(self + 0x80) = *(const f32*)0x002599F0u;
    *(u32*)(self + 0x20C) = 0x00260E70u;
    *(u32*)(self + 0x210) = 0x00260E80u;
    *(u32*)(self + 0x214) = 0x00260E90u;
    *(u32*)(self + 0x218) = 0x00260EA0u;
    *(u32*)(self + 0x21C) = 0x00260EB0u;
    *(u32*)(self + 0x220) = 0x00260EC0u;
    *(u32*)(self + 0x88) = 0x0FFFFFFFu;
    self[0x2C8] = 0;
    self[0x2C9] = 0xFF;
    self[0x2CA] = 0xFF;
    __builtin_memcpy(self + 0xD0, defaults, 48);
    __builtin_memcpy(self + 0x110, defaults, 48);
    __builtin_memcpy(self + 0x1A0, defaults, 48);
    func_00152438(self + 0x1E0, 8);
}

void func_00146340(void* object)
{
    u8* self = (u8*)object;
    u8* owner = (u8*)*(u32*)(self + 0x14);
    *(s32*)(self + 0x2D8) = 0;
    *(f32*)(self + 0x388) = 30.0f;
    *(s32*)(self + 0x2D0) = -1;
    *(s32*)(self + 0x2DC) = -1;
    func_00123ED0(object);
    func_00152470(owner + 0x1C54, object);
    *(s32*)(self + 0x340) = 1;
    *(s32*)(self + 0x37C) = func_00140FF8((s32*)0x00261420u);
    self[0x2B0] = 16;
    self[0x2B1] = 16;
    self[0x2B2] = 16;
    self[0x2B3] = 16;
}

void func_00148D90(void* object)
{
    u8* self = (u8*)object;
    func_00146340(object);
    *(f32*)(self + 0x34C) = 45.0f;
    *(f32*)(self + 0x394) = 6.0f;
    *(f32*)(self + 0x344) = 250.0f;
    *(f32*)(self + 0x348) = 50.0f;
    *(f32*)(self + 0x358) = 0.05f;
    *(f32*)(self + 0x354) = 0.5f;
    *(f32*)(self + 0x390) = 4.0f;
    *(f32*)(self + 0x350) = 50.0f;
    *(s32*)(self + 0x320) = 0;
}
