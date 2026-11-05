/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only Sly resource state selection and request queue setup. */
#include "common.h"

extern u8* D_002623C0;
extern u8* D_002623C4;
extern u8* D_002623C8;
extern s32 D_002623CC;
extern u8 D_00247AF0[];

void func_001608E0(void);
void func_00160880(void);
void func_00140D90(u8* queue);

static s32 resource_key_id(const u8* key)
{
    u32 key_value;
    s32 index;

    if (key == 0) return -1;
    key_value = *(const u32*)(key + 8) ^ *(const u32*)(key + 24);
    for (index = 0; index < 46; ++index) {
        const u8* entry = D_00247AF0 + (u32)index * 0x2Cu;
        if (key_value == (*(const u32*)(entry + 8) ^
                          *(const u32*)(entry + 24))) {
            return *(const s32*)(entry + 0x20);
        }
    }
    return -1;
}

void func_00140F48(u8* queue)
{
    u32 index;
    *(s32*)(queue + 4) = 0;
    for (index = 0; index < 0x81u; ++index) {
        *(u32*)(queue + 8 + index * 4u) =
            *(u32*)(queue + 0x220 + index * 4u);
    }
    *(s32*)(queue + 0x20C) = 0;
}

void func_00160690(const u8* key, s32 flags)
{
    s32 resource_id = resource_key_id(key);
    s32 hi = resource_id != -1 ? resource_id >> 8 : 0;
    s32 lo = resource_id != -1 ? resource_id & 0xFF : 0;
    u8* base = D_002623C0 + 0x10u + (u32)hi * 0x44Cu;
    s32 gate = *(s32*)(D_002623C0 + 0x19E0u);

    D_002623C4 = base;
    D_002623C8 = base + (u32)lo * 0x78u;
    if (gate < 0 || (flags & 2) != 0) {
        func_001608E0();
    } else if ((flags & 1) != 0) {
        func_00140F48((u8*)0x00261420u);
    } else if ((flags & 4) != 0) {
        func_00160880();
    } else {
        func_00140D90((u8*)0x00261420u);
    }

    D_002623CC = *(s32*)(D_002623C0 + 0x19D8u);
    *(s32*)(D_002623C0 + 0x19D8u) = hi;
    *(s32*)(D_002623C0 + 0x19DCu) = lo;
    *(u32*)(D_002623C4 + 0x448u) |= 1u;
    *(u32*)D_002623C8 |= 1u;
}
