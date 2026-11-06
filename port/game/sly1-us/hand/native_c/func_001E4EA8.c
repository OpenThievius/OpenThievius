/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only Sly deferred-resource coordinator, kept outside matching C. */
#include "common.h"

extern u8 D_002C3B00[];
extern void* D_00275710;
extern u8 D_00275F80[];

void func_0018D410(void* memory);
void* func_00137290(void* self, s32 buffer_length, u32 buffer, s32 flags);
s32 func_001373B0(void* self, const s32* pair);
void func_001373D0(void* self);
void func_00160690(void* self, s32 value);
void func_001DB928(s32 changed, void* reader);
int sly1_native_read_sectors(u32 sector, u32 count, u32 destination);
void* func_00193210(void* self, const u32* a, const u32* b, const u32* c,
                    const u32* d);
void* func_0011E580(s32 type, void* owner, void* parameter, s32 mode,
                    s32 listener_id);
void func_001DCEA0(void* self, void* reader);
void sly1_native_loader_stage(u32 stage, u32 object, u32 reader);
void sly1_native_trace_resource_range(u32 sector, u32 byte_count);
void func_001E4EA8(void* state);
void func_00142ED0(f32 scale);
void func_001F0220(void* state, s32 mode);

void func_001E4E80(void* block, s32 a, s32 b, s32 c, s32 d) {
    u8* bytes = (u8*)block;
    if (*(s32*)bytes != 0) return;
    *(s32*)(bytes + 0x00) = 1;
    *(s32*)(bytes + 0x04) = a;
    *(s32*)(bytes + 0x08) = b;
    *(s32*)(bytes + 0x0C) = c;
    *(s32*)(bytes + 0x10) = d;
    sly1_native_loader_stage(7, 0, (u32)block);
    /* The native loader performs synchronous sector reads, so dispatch the
     * queued job here instead of waiting for Sly's absent worker scheduler. */
    func_001E4EA8(block);
}

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

    /* Trace point 9 is the request boundary, before validation or I/O. */
    sly1_native_loader_stage(9, 0, (u32)state);
    if (request == 0) return;
    if (*(u32*)(request + 4) == 0) {
        /* The native reader completed synchronously; retire the retail
         * worker's busy state on the next frame-loop service pass. */
        if (D_00275710 != 0) func_001F0220(D_00275F80, 0);
        return;
    }

    func_0018D410(record);
    for (index = 0; index < 8; ++index) {
        record[index] = ((u32*)*(u32*)(request + 4))[index];
    }
    *(u32*)(request + 4) = 0;

    range[0] = record[0] ^ record[5];
    range[1] = record[1] ^ record[7];
    if (range[1] == 0) return;
    sly1_native_trace_resource_range(range[0], range[1]);

    /* Retail initializes a 0x24-byte callback record from these four words. */
    func_00193210(callback_data, &callback_word0, &callback_word1,
                  &callback_word2, &callback_word3);

    func_00137290(reader, 0x00340C00, (u32)D_002C3B00, 6);
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

    /* The native synchronous read replaces retail's async refill worker.
     * +0x20/+0x24 are the compressed source cursor and length; +0x18/+0x1C
     * stay empty so the first byte request runs the LZ window refill. */
    *(u32*)(reader + 0x20) = (u32)D_002C3B00;
    *(s32*)(reader + 0x24) = (s32)range[1];

    sly1_native_loader_stage(10, 0, (u32)reader);

    for (index = 0; index < 8; ++index) {
        *(u32*)(request + 0x14 + index * 4) = record[index];
    }
    func_00160690(request + 0x14, *(s32*)(request + 0x10));
    /* Retail reads the resource's 8-byte header and prepares its decoded
     * output buffer before the type 0x6C parser sees the stream. */
    func_001DB928(1, reader);

    object = func_0011E580(0x6C, 0, 0, 3, -1);
    D_00275710 = object;
    sly1_native_loader_stage(11, (u32)object, (u32)reader);
    if (object != 0) {
        sly1_native_loader_stage(12, (u32)object, (u32)reader);
        func_001DCEA0(object, reader);
        sly1_native_loader_stage(13, (u32)object, (u32)reader);
        /* Retail resumes the game clock after its resource setup completes. */
        func_00142ED0(1.0f);
    }

    func_001373D0(reader);
    *(u32*)request = 0;
    sly1_native_loader_stage(14, (u32)object, (u32)reader);
}
