/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only collider initialization from the US retail class-0x03 constructor. */
#include "common.h"

void func_0011B008(void* value);
void func_0011B060(void* value, s32 release_mode);
void func_00123ED0(void* object);
void func_00152438(void* list, s32 link_offset);
void func_00160E80(void* mesh);
s32 func_0011A498(void* object, s32 key, s32 mode, void* result);
void func_001B9AE0(void* object, s32 a, s32 b, s32 c, s32 d);
void func_001BB4A0(void* object);

static void copy_words(u8* destination, const u8* source, u32 count)
{
    u32 i;
    for (i = 0; i < count; ++i)
        ((u32*)destination)[i] = ((const u32*)source)[i];
}

void func_001B7968(void* object)
{
    u8* self = (u8*)object;
    u32 script_value[2];
    s32 flags;

    func_0011B008(script_value);
    func_00152438(self + 0x2D8, 0x2E4);
    func_00123ED0(self);
    func_00160E80(self + 0x380);
    copy_words(self + 0x3A4, self + 0x380, 9);

    if (*(u32*)(self + 0x18) != 0)
        func_001B9AE0(self, 3, 0, 3, 0);

    *(s32*)(self + 0x4BC) = -1;
    *(s32*)(self + 0x4C0) = -1;
    if (*(u32*)(self + 0x2C) != 0) {
        flags = func_0011A498(*(void**)(self + 0x2C), 6, 1, script_value);
        flags |= func_0011A498(*(void**)(self + 0x2C), 7, 1, script_value);
        if (flags != 0)
            *(u32*)(self + 0x53C) |= 0x80000000u;
    }

    copy_words(self + 0x350,
               (u8*)*(u32*)(self + 0x14) + 0x1EE0, 4);
    func_001BB4A0(self);
    func_0011B060(script_value, 2);
}
