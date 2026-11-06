/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only font atlas setup and GS transfer packets. */
#include "common.h"

typedef struct SlyBumpAllocator {
    s32 next;
    s32 start;
    s32 limit;
} SlyBumpAllocator;

typedef struct SlyFontImageA {
    u8 source_header[0x14];
    u32 qwords;
    u64 gs_bits;
} SlyFontImageA;

typedef struct SlyFontImageB {
    u32 flags;
    u16 width;
    u16 qwords;
    u32 source;
    u32 destination;
    u64 gs_bits;
} SlyFontImageB;

typedef struct SlyV24A0 {
    u8 pad[0x30];
} SlyV24A0;

typedef struct SlyFontFrame {
    SlyV24A0 writer;
    u8 packet[0x100];
} SlyFontFrame;

extern s32 D_00262260;
extern u8* D_00262264;
extern u8* D_00274504;
extern u8* D_0027450C;

void func_001611B8(void* writer);
void func_001529C8(void* writer, s32 qwords, void* buffer);
void func_00152A50(void* writer, u32* count, u32* begin, u32* out);
void func_001613C0(void* writer, s32 mode, s32 count, s32 format);
void func_00161708(void* writer, s32 address, u64 value);
void* func_0018D5C0(s32 size);

u32 func_00168D30(u32 value) {
    u32 bits = 0;
    u32 power = 1;
    while (power < value && bits < 31) {
        power <<= 1;
        ++bits;
    }
    return bits;
}

void func_00168D68(void* opaque, s32 start, s32 limit) {
    SlyBumpAllocator* allocator = (SlyBumpAllocator*)opaque;
    if (allocator == 0) return;
    allocator->next = start;
    allocator->start = start;
    allocator->limit = limit;
}

s32 func_00168D88(void* opaque, s32 size) {
    SlyBumpAllocator* allocator = (SlyBumpAllocator*)opaque;
    s32 result = allocator->start;
    allocator->start += size;
    return result;
}

void func_00168D98(u32 dbp, u32 dbw, u32 dpsm, u64 width,
                   u32 height, s32 qwords, void* writer) {
    func_001613C0(writer, 0, 1, 0xE);
    func_00161708(writer, 0x50,
                  ((u64)dbp << 32) | ((u64)dbw << 48) | ((u64)dpsm << 56));
    func_00161708(writer, 0x51, 0);
    func_00161708(writer, 0x52, width | ((u64)height << 32));
    func_00161708(writer, 0x53, 0);
    func_00161958(writer, qwords);
}

void func_00168FB0(void* opaque, u32 dbp) {
    SlyFontImageA* image = (SlyFontImageA*)opaque;
    u64 bits = image->gs_bits;
    const u16* dimensions = (const u16*)image->source_header;
    const u8 dbw = image->source_header[9];
    const u8 psm = image->source_header[8];
    const u16 width = dimensions[0];
    const u16 height = dimensions[1];
    bits &= ~((u64)0x3FFFu | ((u64)0x3Fu << 14) | ((u64)0x3Fu << 20) |
              ((u64)0xFu << 26) | ((u64)0xFu << 30) | ((u64)1u << 34));
    bits |= (u64)(dbp & 0x3FFFu) | ((u64)(dbw & 0x3Fu) << 14) |
            ((u64)(psm & 0x3Fu) << 20) |
            ((u64)(func_00168D30(width) & 0xFu) << 26) |
            ((u64)(func_00168D30(height) & 0xFu) << 30) |
            ((u64)1u << 34);
    image->gs_bits = bits;
}

void func_00169088(void* opaque, u32 dbp, void* writer) {
    SlyFontImageA* image = (SlyFontImageA*)opaque;
    const u16* dimensions = (const u16*)image->source_header;
    func_00168D98(dbp, image->source_header[9], image->source_header[8],
                  dimensions[0], dimensions[1], (s32)image->qwords, writer);
}

void func_00168E78(void* opaque, u32 dbp) {
    SlyFontImageB* image = (SlyFontImageB*)opaque;
    u64 bits = image->gs_bits;
    const u32 width_mode = image->width <= 0x10u ? 0x14u : 0x13u;
    bits &= ~(((u64)0x3FFFu << 37) | ((u64)0x3Fu << 20) |
              ((u64)0xFu << 51) | ((u64)1u << 55) |
              ((u64)0x1Fu << 56) | ((u64)7u << 61));
    bits |= ((u64)(dbp & 0x3FFFu) << 37) | ((u64)width_mode << 20) |
            ((u64)1u << 61);
    image->gs_bits = bits;
}

void func_00168F48(void* opaque, s32 dbp, void* writer) {
    SlyFontImageB* image = (SlyFontImageB*)opaque;
    const s32 step = image->width > 16 ? 16 : 8;
    const s32 last = (s32)image->width - 1;
    const s32 height = ((last + step) & -step) / step;
    func_00168D98((u32)dbp, 1, 0, (u64)step, (u32)height,
                  (s32)image->destination, writer);
}

void func_0015D998(void* font_opaque, void* allocator) {
    u8* font = (u8*)font_opaque;
    SlyFontImageA* image_a;
    SlyFontFrame* frame;
    s32 address;
    if (font == 0 || allocator == 0) return;
    image_a = (SlyFontImageA*)(u32)*(u32*)(font + 0x54u);
    if (image_a == 0) return;
    frame = (SlyFontFrame*)func_0018D5C0((s32)sizeof(SlyFontFrame));
    if (frame == 0) return;

    func_001611B8(&frame->writer);
    address = func_00168D88(allocator, *(u16*)(image_a->source_header + 0x0Au));
    func_00168FB0(image_a, (u32)address);
    *(u64*)(font + 0x60u) = image_a->gs_bits;
    func_001529C8(&frame->writer, 16, frame->packet);
    func_00169088(image_a, (u32)address, &frame->writer);
    func_00152A50(&frame->writer, (u32*)(font + 0x68u), 0,
                  (u32*)(font + 0x6Cu));

    if (*(u32*)(font + 0x58u) != 0) {
        SlyFontImageB* image_b = (SlyFontImageB*)(u32)*(u32*)(font + 0x58u);
        func_001611B8(&frame->writer);
        address = func_00168D88(allocator, image_b->qwords);
        func_00168E78(image_b, (u32)address);
        *(u64*)(font + 0x60u) |= image_b->gs_bits;
        func_001529C8(&frame->writer, 16, frame->packet);
        func_00168F48(image_b, address, &frame->writer);
        func_00152A50(&frame->writer, (u32*)(font + 0x70u), 0,
                      (u32*)(font + 0x74u));
    }
}

void func_0015E5F0(void) {
    SlyBumpAllocator allocator;
    func_00168D68(&allocator, 0x1400, 0x1E00);
    for (s32 i = 0; i < D_00262260; ++i)
        func_0015D998(D_00262264 + (u32)i * 0x88u, &allocator);
}
