/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only Sly deferred-resource coordinator, kept outside matching C. */
#include "common.h"

extern u8 D_002C3B00[];
extern u8 D_00247AF0[];
extern void* D_00275710;
extern void* D_00261990;
extern u8 D_00261EF0[];
extern u8 D_002623D8[];
extern u8 D_0026FA90[];
extern void* D_0026C6C8;
extern f32 D_0026186C;

void func_0018D410(void* memory);
void* func_00137290(void* self, s32 buffer_length, u32 buffer, s32 flags);
s32 func_001373B0(void* self, const s32* pair);
void func_001373D0(void* self);
void func_00160690(void* self, s32 value);
void func_001DB928(s32 changed, void* reader);
void* func_00193210(void* self, const u32* a, const u32* b, const u32* c,
                    const u32* d);
void* func_0011E580(s32 type, void* owner, void* parameter, s32 mode,
                    s32 listener_id);
void func_001DCEA0(void* self, void* reader);
void sly1_native_loader_stage(u32 stage, u32 object, u32 reader);
void sly1_native_trace_resource_range(u32 sector, u32 byte_count);
void func_001E4EA8(void* state);
void func_001DB838(s32 has_world);
void func_001E4E80(void* block, s32 a, s32 b, s32 c, s32 d);
u32 sly1_native_boot_level_key(void);
void func_00142ED0(f32 scale);
void func_00191E70(s32 mode);
void func_00143A60(void* self, s32 initialize);
void func_001C0070(void);
void func_001C06A0(s32 row, s32 column, f32 value);
s32 func_001F4318(void* world, void* request_a, void* request_b);
void func_00151E18(void* self);
void func_00160CE8(void* self);
void func_0016B520(void* self);
void func_00138C00(void* world, s32 mode);
void func_00143E40(void* self, s32 reset, f32 delta);
void func_001C06D8(void);
void func_00191EB0(void);
u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);

static void finish_resource_service(u8* request) {
    void* world = D_00275710;
    func_00143A60(D_00261990, 1);
    func_001C0070();
    func_001C06A0(10, 0, 1.0f);
    (void)func_001F4318(world, request + 8, request + 0x0C);
    func_00151E18(D_00261EF0);
    func_00160CE8(D_002623D8);
    func_0016B520(D_0026FA90);
    func_00138C00(world, 0);
    func_00143E40(D_00261990, 0, D_0026186C);

    if ((*(u32*)(request + 0x10) & 0x10u) != 0) {
        const u32 holder_address = (u32)&D_0026C6C8;
        const u32 holder = *(u32*)holder_address;
        if (holder != 0) {
            const u32 vtable = *(u32*)holder;
            const u32 method = vtable != 0 ? *(u32*)(vtable + 0x38u) : 0;
            if (method != 0)
                (void)sly1_native_call_regs4(method, holder_address, 0, 0, 0);
        }
    }

    func_00142ED0(1.0f);
    func_001C06D8();
    if (world != 0)
        func_001C06A0(1, 1, *(f32*)((u8*)world + 0x1D7Cu));
    func_00191EB0();
    *(u32*)request = 0;
}

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
    static s32 direct_level_attempted = 0;
    u8* request = (u8*)state;
    u32 record[8];
    u32 range[2];
    u32 sector_count;
    u32 ring_buffer;
    u32 buffer_capacity;
    u32 callback_data[9] = {0};
    u32 callback_word0 = 0x00003F00u;
    u32 callback_word1 = 0x003F3F3Fu;
    u32 callback_word2 = 0x00003F3Fu;
    u32 callback_word3 = 0x0000003Fu;
    u8 reader[0x60];
    void* object;
    s32 index;

    /* Trace point 9 is the request boundary, before validation or I/O. */
    func_00191E70(2);
    sly1_native_loader_stage(9, 0, (u32)state);
    if (request == 0) return;
    if (*(u32*)(request + 4) == 0) {
        /* Retail still runs its per-frame resource/camera service tail when
         * there is no pending request. */
        finish_resource_service(request);
        return;
    }

    func_0018D410(record);
    for (index = 0; index < 8; ++index) {
        record[index] = ((u32*)*(u32*)(request + 4))[index];
    }
    *(u32*)(request + 4) = 0;

    range[0] = record[0] ^ record[5];
    range[1] = record[1] ^ record[7];
    if (range[1] == 0) {
        finish_resource_service(request);
        return;
    }
    sly1_native_trace_resource_range(range[0], range[1]);

    /* Retail initializes a 0x24-byte callback record from these four words. */
    func_00193210(callback_data, &callback_word0, &callback_word1,
                  &callback_word2, &callback_word3);

    sector_count = (range[1] + 0x7FFu) >> 11;
    buffer_capacity = sector_count << 11;
    if (buffer_capacity > 0x00C00000u) {
        finish_resource_service(request);
        return;
    }
    /* D_002C3B00 also backs renderer frame buffers. Large reader rings use
     * reserved upper EE RAM instead of overwriting those buffers. */
    ring_buffer = buffer_capacity > 0x00340C00u ? 0x01000000u :
                                                    (u32)D_002C3B00;
    func_00137290(reader, (s32)buffer_capacity, ring_buffer, 6);
    *(u32*)(reader + 0x30) = (u32)callback_data;
    if (func_001373B0(reader, (const s32*)range) == 0) {
        func_001373D0(reader);
        finish_resource_service(request);
        return;
    }
    /* Match retail transition ordering: retire the previous world before
     * decoding and publishing its replacement. */
    func_001DB838(D_00275710 != 0);
    D_00275710 = 0;

    /* Preserve the retail reader state from func_00137328: +0x20 is the
     * ring-buffer write cursor and +0x24 is the previous refill length.
     * func_00137490 performs bounded sector reads into that ring when the
     * byte reader needs data. Pre-reading the entire range and setting +0x24
     * to its total length makes the refill path advance past the buffer. */

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
        func_001C06A0(10, 0, 0.0f);
        func_001DCEA0(object, reader);
        sly1_native_loader_stage(13, (u32)object, (u32)reader);
        /* Retail resumes the game clock after its resource setup completes. */
        func_00142ED0(1.0f);
    }

    func_001373D0(reader);
    sly1_native_loader_stage(14, (u32)object, (u32)reader);

    /* Development-only shortcut from the first completed startup resource to
     * a retail catalog level. This avoids waiting for movie/title systems that
     * the native port has not implemented yet. */
    if (!direct_level_attempted && object != 0) {
        const u32 wanted = sly1_native_boot_level_key();
        if (wanted != 0) {
            direct_level_attempted = 1;
            for (index = 0; index < 46; ++index) {
                u8* entry = D_00247AF0 + (u32)index * 0x2Cu;
                if (*(u32*)(entry + 0x20) == wanted) {
                    sly1_native_loader_stage(150, wanted, (u32)entry);
                    func_001E4E80((void*)0x00275AF0u, (s32)entry, 0, 0, 0);
                    break;
                }
            }
        }
    }
    finish_resource_service(request);
}
