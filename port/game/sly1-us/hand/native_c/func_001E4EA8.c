/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only Sly deferred-resource coordinator, kept outside matching C. */
#include "common.h"

extern u8 D_002C3B00[];
extern void* D_00275710;

void func_0018D410(void* memory);
void* func_00137290(void* self, s32 source, u32 buffer, s32 buffer_length);
s32 func_001373B0(void* self, const s32* pair);
void func_001373D0(void* self);
void func_00160690(void* self, s32 value);
int sly1_native_read_sectors(u32 sector, u32 count, u32 destination);
void* func_00193210(void* self, const u32* a, const u32* b, const u32* c,
                    const u32* d);
void* func_0011E580(s32 type, void* owner, void* parameter, s32 mode,
                    s32 listener_id);
void func_001DCEA0(void* self, void* reader);

void func_001E4EA8(void* state) {
    u8* request = (u8*)state;
    u32 record[8];
    u32 range[2];
    u32 sector_count;
    u32 callback_data[9] = {0};
    u32 callback_word0 = 0x00003F00u;
    u32 callback_word1 = 0x003F3F3Fu;
    u32 callback_word2 = 0x00003F3Fu;
    u32 callback_word3 = 0x0000003Fu;
    u8 reader[0x60];
    void* object;
    s32 index;

    if (request == 0 || *(u32*)(request + 4) == 0) return;

    func_0018D410(record);
    for (index = 0; index < 8; ++index) {
        record[index] = ((u32*)*(u32*)(request + 4))[index];
    }
    *(u32*)(request + 4) = 0;

    range[0] = record[0] ^ record[5];
    range[1] = record[1] ^ record[7];
    if (range[1] == 0) return;

    /* Retail initializes a 0x24-byte callback record from these four words. */
    func_00193210(callback_data, &callback_word0, &callback_word1,
                  &callback_word2, &callback_word3);

    func_00137290(reader, (s32)(u32)D_002C3B00, 0x00340C00u, 6);
    *(u32*)(reader + 0x30) = (u32)callback_data;
    if (func_001373B0(reader, (const s32*)range) == 0) {
        func_001373D0(reader);
        return;
    }

    if (range[1] > 0x00340C00u) {
        func_001373D0(reader);
        return;
    }
    sector_count = (range[1] + 0x7FFu) >> 11;
    if (sly1_native_read_sectors(range[0], sector_count,
                                 (u32)D_002C3B00) == 0) {
        func_001373D0(reader);
        return;
    }
    *(u32*)(reader + 0x18) = (u32)D_002C3B00;
    *(u32*)(reader + 0x1C) = range[1];

    for (index = 0; index < 8; ++index) {
        *(u32*)(request + 0x14 + index * 4) = record[index];
    }
    func_00160690(request + 0x14, *(s32*)(request + 0x10));

    object = func_0011E580(0x6C, 0, 0, 3, -1);
    D_00275710 = object;
    if (object != 0) {
        func_001DCEA0(object, reader);
    }

    func_001373D0(reader);
    *(u32*)request = 0;
}
