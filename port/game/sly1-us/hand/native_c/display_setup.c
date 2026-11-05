/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only display setup backfills translated from the NTSC-U ELF. */
#include "common.h"

int func_001AEA70(s32 initialize, s32 mode)
{
    static const u32 objects[5] = {
        0x002743D0u, 0x002743F0u, 0x00274410u, 0x00274450u, 0x00274488u
    };
    u32 i;

    if (initialize == 0 || mode != -1) return 0;
    for (i = 0; i < 5; ++i) {
        u8* object = (u8*)objects[i];
        *(u32*)(object + 0x00) = 0;
        *(u8*)(object + 0x04) = i == 2 || i == 3 ? 61 : 45;
        *(f32*)(object + 0x08) = 2.0f;
        *(f32*)(object + 0x0C) = i == 0 ? 1.0f : -4.0f;
        *(f32*)(object + 0x10) = 0.3f;
        *(f32*)(object + 0x14) = 0.3f;
        *(u32*)(object + 0x18) = 0xFF7D4B00u;
    }
    return 0;
}

int func_001AEC90(void)
{
    return func_001AEA70(1, -1);
}

int func_00196330(s32 initialize, s32 mode)
{
    u8* object = (u8*)0x0027A210u;
    u32 i;

    if (initialize == 0 || mode != -1) return 0;
    for (i = 0; i < 7; ++i) *(u32*)(object + i * 4u) = 0;
    *(u8*)(object + 0x04) = 45;
    *(f32*)(object + 0x08) = 15.0f;
    *(f32*)(object + 0x10) = 0.6f;
    *(f32*)(object + 0x14) = 0.6f;
    *(u32*)(object + 0x18) = 0xFF7D4B00u;
    return 0;
}

int func_001963B8(void)
{
    return func_00196330(1, -1);
}

int func_001930B8(s32 initialize, s32 mode)
{
    u8* first = (u8*)0x00604700u;
    u8* second = (u8*)0x00279C10u;
    u8* template_a = (u8*)0x00258D30u;
    u8* template_b = (u8*)0x00258D20u;
    u32 i;

    if (initialize == 0 || mode != -1) return 0;
    for (i = 0; i < 0x80; ++i) {
        first[i] = 0;
        second[i] = 0;
    }
    for (i = 0; i < 0x10; ++i) {
        first[0x60 + i] = template_a[i];
        second[0x30 + i] = template_b[i];
    }
    *(f32*)(second + 0x00) = 1000.0f;
    *(f32*)(second + 0x04) = 1200.0f;
    *(f32*)(second + 0x08) = 1.0f;
    *(f32*)(second + 0x18) = -1750.0f;
    *(f32*)(second + 0x20) = -0.5f;
    *(u32*)(second + 0x4C) = 1;
    *(f32*)(second + 0x50) = 0.5f;
    *(f32*)(second + 0x54) = 2.0f;
    *(f32*)(second + 0x70) = 1.0f;
    *(f32*)(second + 0x74) = -1.0f;
    return 0;
}

int func_001931F0(void)
{
    return func_001930B8(1, -1);
}

int func_00180E38(s32 initialize, s32 mode)
{
    u8* gray = (u8*)0x00264098u;
    u8* blue = (u8*)0x002640A0u;

    if (initialize == 0 || mode != -1) return 0;
    gray[0] = 0xBF;
    gray[1] = 0xBF;
    gray[2] = 0xBF;
    gray[3] = 0x7F;
    blue[0] = 0x00;
    blue[1] = 0x87;
    blue[2] = 0xFF;
    blue[3] = 0x7F;
    return 0;
}

int func_00180E90(void)
{
    return func_00180E38(1, -1);
}

int func_0018F940(s32 initialize, s32 mode)
{
    if (initialize != 0 && mode == -1)
        *(u32*)0x00279AECu = 0x0025B5B8u;
    return 0;
}

int func_0018F968(void)
{
    return func_0018F940(1, -1);
}

void func_001FFBE0(void* first, void* entry)
{
    u8* node = (u8*)entry;
    u32* head = (u32*)0x0062A7B8u;
    u32 prior = *head;

    *(u32*)(node + 0x0C) = (u32)first;
    *(u32*)(node + 0x14) = prior;
    *head = (u32)entry;
    *(u32*)(node + 0x08) = (u32)first;
    *(u32*)(node + 0x04) = 0;
    *(u32*)(node + 0x00) = 0;
    *(u32*)(node + 0x10) = 0;
}

int func_001C29C8(void)
{
    u32* first = (u32*)0x002748F0u;
    u32* second = (u32*)0x00274940u;
    u32 i;

    for (i = 0; i < 20; ++i) {
        first[i] = 0;
        second[i] = 0;
    }
    first[0] = 0x02605440u;
    first[1] = 0x70000000u;
    second[0] = 0x026054C0u;
    second[1] = 0x70002000u;
    return 0;
}
