/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only resource activation backfill translated from NTSC-U retail C. */
#include "common.h"

extern u8 D_00244950[];

void func_00137290(void* reader, s32 source, s32 size, s32 flags);
void func_00137398(void* reader, s32 mode, void* data);
void func_0011E680(void* object, void* record, void* reader);
void func_001372E0(void* reader, s32 flags);

struct NativeActivationPair {
    u32 record;
    u32 unused;
};

void func_0011EDA8(void* object) {
    u8 reader[0x60];
    u32 descriptor = *(u32*)object;
    s32 descriptor_id;
    s32 count;
    u32 list;

    if (descriptor == 0) return;
    descriptor_id = *(s32*)(descriptor + 4);
    if (descriptor_id < 0 || descriptor_id >= 0x400) return;

    count = *(s32*)(D_00244950 + descriptor_id * 8);
    list = *(u32*)(D_00244950 + descriptor_id * 8 + 4);
    if (count <= 0 || list == 0) return;

    struct NativeActivationPair* pair =
        (struct NativeActivationPair*)(list + 4);
    for (s32 i = 0; i < count; ++i, ++pair) {
        u32 record = pair->record;
        if (record == 0 || (*(u32*)(record + 4) & 0x800u) == 0) continue;

        func_00137290(reader, 0, 0, 0);
        func_00137398(reader, 4, (void*)(record + 8));
        func_0011E680(object, (void*)record, reader);
        func_001372E0(reader, 2);
    }
}
