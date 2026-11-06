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
