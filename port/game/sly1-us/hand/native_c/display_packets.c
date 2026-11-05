// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only C backfills for Sly's GS packet writer, translated from US retail.
#include "common.h"

typedef struct SlyPacketWriter {
    u32 end;
    u32 begin;
    u32 dma_tag;
    u32 cursor;
    s32 pad;
    s32 use_close_callback;
    u32 vtable;
    u32 last_tag;
    u32 registers_per_loop;
    u32 register_count;
} SlyPacketWriter;

extern u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);
void func_00152D70(void* opaque);

static void packet_writer_close(SlyPacketWriter* writer, s32 flag) {
    s16 adjustment = *(s16*)(writer->vtable + 8u);
    u32 callback = *(u32*)(writer->vtable + 12u);
    (void)sly1_native_call_regs4(callback,
                                  (u32)((u8*)writer + adjustment),
                                  (u32)flag, 0, 0);
}

void func_00152888(void* frame) {
    u32* words = (u32*)frame;
    for (s32 i = 0; i < 6; ++i) words[i] = 0;
    words[6] = 0x0024A0E0u;
}

void func_001611B8(void* frame) {
    u32* words = (u32*)frame;
    func_00152888(frame);
    words[5] = 1;
    words[6] = 0x0024A6F8u;
    words[7] = 0;
    words[8] = 0;
    words[9] = 0;
}

void func_001612E8(void* frame) {
    ++((u32*)frame)[9];
}

void func_00152B38(void* opaque) {
    SlyPacketWriter* writer = (SlyPacketWriter*)opaque;
    func_00152D70(opaque);
    u32 cursor = writer->cursor;
    writer->dma_tag = cursor;
    writer->cursor = cursor + 16u;
    *(u64*)cursor = 0x10000000u;
    *(u64*)(cursor + 8u) = 0;
}

void func_00152B80(void* opaque, s32 low, s32 high) {
    SlyPacketWriter* writer = (SlyPacketWriter*)opaque;
    s32 start_new_tag = 0;
    if (writer->dma_tag != 0) {
        func_00152D70(opaque);
        start_new_tag = 1;
    }
    u32 cursor = writer->cursor;
    writer->cursor = cursor + 16u;
    *(u64*)cursor = (u64)(u32)low | 0x40000000u | ((u64)(u32)high << 32);
    *(u64*)(cursor + 8u) = 0;
    if (start_new_tag) func_00152B38(opaque);
}

void func_00161278(void* opaque, s32 end_packet) {
    SlyPacketWriter* writer = (SlyPacketWriter*)opaque;
    if (writer->last_tag == 0) return;
    u32 loops = writer->register_count / writer->registers_per_loop;
    u32 cursor = writer->cursor;
    if ((cursor & 0xFu) != 0) {
        writer->cursor = cursor + 8u;
        *(u64*)cursor = 0;
    }
    u32 tag = *(u32*)writer->last_tag;
    if (end_packet != 0) tag |= 0x8000u;
    *(u32*)writer->last_tag = tag | loops;
    writer->last_tag = 0;
}

void func_001613C0(void* frame, s32 mode, s32 count, s32 format) {
    SlyPacketWriter* writer = (SlyPacketWriter*)frame;
    packet_writer_close(writer, 0);
    u32 start = writer->cursor;
    u64 tag = ((u64)(u32)mode << 47) |
              ((u64)(u32)count << 60) |
              ((u64)1u << 46);
    *(u64*)start = tag;
    *(u64*)(start + 8u) = (u64)(u32)format;
    writer->cursor = start + 16u;
    writer->last_tag = start;
    writer->registers_per_loop = (u32)count;
    writer->register_count = 0;
}

void func_00161708(void* frame, s32 address, u64 value) {
    u32* words = (u32*)frame;
    func_001612E8(frame);
    u32 current = words[3];
    *(u64*)current = value;
    *(u64*)(current + 8) = (u64)(u32)address;
    words[3] = current + 16;
}

void func_00161768(void* opaque, u32 register_count, u64 value) {
    SlyPacketWriter* writer = (SlyPacketWriter*)opaque;
    packet_writer_close(writer, 0);
    u32 cursor = writer->cursor;
    writer->cursor = cursor + 16u;
    *(u64*)(cursor + 8u) = value;
    *(u64*)cursor = ((u64)register_count << 60) | ((u64)0x8000u << 43);
    writer->registers_per_loop = register_count;
    writer->last_tag = cursor;
    writer->register_count = 0;
}

void func_001617E8(void* frame, u32 register_id, f32 value) {
    func_001612E8(frame);
    u32 bits;
    __builtin_memcpy(&bits, &value, sizeof(bits));
    u32 cursor = ((SlyPacketWriter*)frame)->cursor;
    *(u64*)cursor = (u64)register_id | ((u64)bits << 32);
    ((SlyPacketWriter*)frame)->cursor = cursor + 8u;
}

void func_00161860(void* frame, s32 first, s32 second) {
    func_001612E8(frame);
    SlyPacketWriter* writer = (SlyPacketWriter*)frame;
    u32 cursor = writer->cursor;
    *(u64*)cursor = (u64)(u32)first | ((u64)(u32)second << 16);
    writer->cursor = cursor + 8u;
}

void func_001618C0(void* frame, s32 first, s32 second,
                   u32 third, u32 fourth) {
    func_001612E8(frame);
    SlyPacketWriter* writer = (SlyPacketWriter*)frame;
    u32 cursor = writer->cursor;
    *(u64*)cursor = (u64)(u32)first | ((u64)(u32)second << 16) |
                    ((u64)(third & 0xFFFFFFu) << 32) |
                    ((u64)(fourth & 0xFFu) << 56);
    writer->cursor = cursor + 8u;
}

void func_00161200(void* frame) {
    SlyPacketWriter* writer = (SlyPacketWriter*)frame;
    if (writer->last_tag == 0) func_001613C0(frame, 0, 1, 0xE);
    func_00161278(frame, 1);
    packet_writer_close(writer, 1);
}

// Retail packet-list finalizer at 0x00152D70. The host packet submitter
// consumes completed DMA tags synchronously, so console cache work is absent.
void func_00152D70(void* self) {
    SlyPacketWriter* fields = (SlyPacketWriter*)self;
    u32 current = fields->cursor;

    if (fields->pad != 0) {
        if ((current & 0xFu) != 0) {
            do {
                *(u32*)current = 0;
                current += 4u;
            } while ((current & 0xFu) != 0);
            fields->cursor = current;
        }
    } else if (fields->use_close_callback != 0) {
        packet_writer_close(fields, 0);
    }

    if (fields->dma_tag != 0) {
        u32* tag = (u32*)fields->dma_tag;
        const u32 units = ((fields->cursor - fields->dma_tag) >> 4) - 1u;
        tag[0] |= units;
        fields->dma_tag = 0;
    }
}
