/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only aligned free-list pool setup used by C6C resource objects. */
#include "common.h"

void* func_0018D500(s32 size);

void func_001B5D80(void* pool, s32 stride, s32 count)
{
    u32 current;
    s32 index;
    u8* object = (u8*)pool;

    current = *(u32*)(object + 4u);
    *(s32*)(object + 0u) = stride;
    *(u32*)(object + 8u) = current;
    for (index = 1; index < count; ++index) {
        u32 previous = current;
        current = *(u32*)(object + 4u) + (u32)index * (u32)stride;
        *(u32*)previous = current;
    }
    *(u32*)current = 0;
}

void func_001B5DD0(void* pool, s32 size, s32 count)
{
    s32 stride = (size + 7) & ~7;
    u8* object = (u8*)pool;

    *(u32*)(object + 4u) = (u32)func_0018D500(stride * count);
    func_001B5D80(pool, stride, count);
}

void func_001B5E30(void* pool, s32 size, s32 count)
{
    u32 allocation = (u32)func_0018D500(size * count + 0x30);
    u8* object = (u8*)pool;

    *(u32*)(object + 4u) = (allocation + 0x3Fu) & ~0x3Fu;
    func_001B5D80(pool, size, count);
}
