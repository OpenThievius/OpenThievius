/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only resource record readers. Replace with decompiled C when available. */
#include "common.h"

typedef struct SlyPaletteRecord {
    u32 flags;
    u16 width;
    u16 format;
    s32 source;
    s32 qwords;
    u64 gs_bits;
} SlyPaletteRecord;

typedef struct SlyImageRecord {
    u16 type;
    u16 format;
    u32 flags;
    u8 psm;
    u8 dbw;
    u16 f0A;
    u32 bytes;
    s32 source;
    s32 qwords;
    u64 gs_bits;
} SlyImageRecord;

extern s32 D_00275718;
extern s32 D_00274500;
extern u32 D_00274504;
extern s32 D_00274508;
extern u32 D_0027450C;
extern s32 D_00274510;
extern s32 D_00274514;

u16 func_00137BE0(u32 reader);
void func_00137A60(u32 reader, s32 size, u32 destination);
u32 func_0018D5C0(u32 size);
void sly1_native_trace_font(u32 stage, u32 a0, u32 a1, u32 a2,
                            u32 a3, u32 a4);

static void read_image_records_b(u32 reader) {
    s32 count = func_00137BE0(reader);
    s32 flagged = 0;
    s32 i;
    u32 records = func_0018D5C0((u32)(count * 0x18));

    D_00274508 = count;
    D_0027450C = records;
    for (i = 0; i < count; ++i) {
        u32 record = records + (u32)(i * 0x18);
        func_00137A60(reader, 12, record);
        SlyPaletteRecord* entry = (SlyPaletteRecord*)(u32)record;
        entry->source += D_00275718;
        entry->qwords = (s32)(((entry->width << 2) + 15) >> 4);
        if ((entry->flags & 0x10000000u) != 0) ++flagged;
    }
    D_00274514 = flagged > count - flagged ? flagged : count - flagged;
    sly1_native_trace_font(71u, (u32)reader, (u32)count, (u32)records,
                           (u32)D_00274514, (u32)D_00275718);
}

static void read_image_records_a(u32 reader) {
    s32 count = func_00137BE0(reader);
    s32 flagged = 0;
    s32 i;
    u32 records = func_0018D5C0((u32)(count * 0x20));

    D_00274500 = count;
    D_00274504 = records;
    for (i = 0; i < count; ++i) {
        u32 record = records + (u32)(i * 0x20);
        func_00137A60(reader, 0x14, record);
        SlyImageRecord* entry = (SlyImageRecord*)(u32)record;
        entry->qwords = (s32)((entry->bytes + 15) >> 4);
        entry->source += D_00275718;
        if ((entry->flags & 0x10000000u) != 0) ++flagged;
    }
    D_00274510 = flagged > count - flagged ? flagged : count - flagged;
    sly1_native_trace_font(72u, (u32)reader, (u32)count, (u32)records,
                           (u32)D_00274510, (u32)D_00275718);
}

void func_001B2BA8(u32 reader) {
    read_image_records_b(reader);
}

void func_001B2CF8(u32 reader) {
    read_image_records_a(reader);
}
