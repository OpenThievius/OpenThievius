/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only common resource-object reader. */
#include "common.h"

void func_00137A60(void* reader, s32 size, void* destination);
s32 func_00137B90(void* reader);
s32 func_00137BE0(void* reader);
s32 func_00137C40(void* reader);
s32 func_00137D10(void* reader);
f32 func_00137DF8(void* reader);
void func_00137E70(void* reader, void* destination);
void func_00137E90(void* reader, void* destination);
void func_00137EB0(void* reader, void* destination);
void func_00137F08(void* reader, void* destination);
void* func_0018D500(s32 size);
void* func_0018D5C0(s32 size);
void* func_0018D568(s32 size, void* source);
void func_0011EC40(void* object, void* reader);
void func_0011ECB8(void* world, void* object, void* reader);
void func_00162890(void* block, void* reader, void* object);
s32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);

static void sly1_discard(void* reader, u32 size) {
    /* Retail's read helper takes a signed size and ignores overflowed values. */
    if ((s32)size <= 0) return;
    while (size != 0) {
        const s32 chunk = size > 256u ? 256 : (s32)size;
        func_00137A60(reader, chunk, 0);
        size -= (u32)chunk;
    }
}

void func_00137B60(void* reader, s32 alignment) {
    u8* bytes = (u8*)reader;
    s32 head = *(s32*)(bytes + 0x18);
    s32 aligned = (head + alignment - 1) & -alignment;
    *(s32*)(bytes + 0x18) = aligned;
    *(s32*)(bytes + 0x1C) -= aligned - head;
}

void func_00162890(void* block, void* reader, void* object) {
    u8* bytes = (u8*)block;
    static s32 trace_records;
    extern void sly1_native_loader_stage(u32 stage, u32 object, u32 reader);
    *(s32*)(bytes + 0x50) = 0;
    const s32 first = func_00137B90(reader);
    sly1_native_loader_stage(105, (u32)first, (u32)reader);
    const s32 second = func_00137B90(reader);
    sly1_native_loader_stage(106, (u32)second, (u32)reader);
    const s32 component_count = func_00137B90(reader);
    sly1_native_loader_stage(107, (u32)component_count, (u32)reader);
    (void)second;
    if (component_count < 0 || component_count > 4096) return;
    *(s32*)(bytes + 0x00) = component_count;
    *(u32*)(bytes + 0x04) = (u32)func_0018D5C0(component_count * 0x90);
    *(u32*)(bytes + 0x08) = (u32)func_0018D5C0(component_count * 4);
    for (s32 i = 0; i < component_count; ++i)
        *(s16*)(*(u32*)(bytes + 0x08) + (u32)i * 2u) = (s16)func_00137D10(reader);

    const s32 word_count = func_00137B90(reader);
    sly1_native_loader_stage(108, (u32)word_count, (u32)reader);
    if (word_count < 0 || word_count > 65535) return;
    *(s32*)(bytes + 0x40) = word_count;
    *(u32*)(bytes + 0x44) = (u32)func_0018D5C0(word_count * 4);
    func_00137A60(reader, word_count * 4, (void*)*(u32*)(bytes + 0x44));
    *(u32*)(bytes + 0x48) = (u32)func_0018D568(
        word_count * 4, (void*)*(u32*)(bytes + 0x44));

    const s32 record_count = func_00137BE0(reader);
    sly1_native_loader_stage(109, (u32)record_count, (u32)reader);
    if (record_count < 0 || record_count > 65535) return;
    *(s32*)(bytes + 0x0C) = record_count;
    *(u32*)(bytes + 0x10) = (u32)func_0018D5C0(record_count * 0x70);
    *(u32*)(bytes + 0x14) = (u32)func_0018D5C0(record_count * 0x28);
    u8* records = (u8*)*(u32*)(bytes + 0x10);

    for (s32 i = 0; i < record_count; ++i) {
        u8* record = records + (u32)i * 0x70u;
        const s32 flags = func_00137BE0(reader);
        if (trace_records < 16) {
            sly1_native_loader_stage(110, (u32)flags, (u32)reader);
            ++trace_records;
        }
        if (flags & 1) {
            const s32 source_index = func_00137D10(reader);
            if (source_index >= 0 && source_index < i)
                __builtin_memcpy(record, records + (u32)source_index * 0x70u, 0x70u);
            else
                __builtin_memset(record, 0, 0x70u);
            u32 vectors = (u32)func_0018D500(0x40);
            if (vectors != 0) func_00137F08(reader, (void*)vectors);
            else {
                u8 scratch[64];
                func_00137F08(reader, scratch);
            }
        } else {
            *(u32*)(record + 0x1C) = 0x501502F9u;
            *(u32*)(record + 0x20) = 0x44FA0000u;
            *(u32*)(record + 0x24) = 0x7F7FFFFFu;
            *(u32*)(record + 0x28) = (u32)first;
            *(u32*)(record + 0x30) = 0;
            *(u32*)(record + 0x38) = 0;
            *(u32*)(record + 0x3C) = 0x3F800000u;
            *(u32*)(record + 0x50) = 0x3F800000u;
        }
        *(s32*)(record + 0x68) = flags;
        if (flags & 0x002) *(s32*)(record + 0x00) = func_00137C40(reader);
        if (flags & 0x200) *(f32*)(record + 0x50) = func_00137DF8(reader);
        if (flags & 0x004) {
            f32 value = func_00137DF8(reader);
            if (value != 3.402823466e+38f) value *= value < 0 ? -value : value;
            *(f32*)(record + 0x24) = value;
        }
        if (flags & 0x008) *(f32*)(record + 0x3C) = func_00137DF8(reader);
        if (flags & 0x010) {
            f32 value = func_00137DF8(reader);
            *(f32*)(record + 0x1C) = value == 3.402823466e+38f
                ? 3.402823466e+38f : value;
        }
        if (flags & 0x020) {
            const f32 value = func_00137DF8(reader);
            if (value != 3.402823466e+38f && value < *(f32*)(record + 0x20))
                *(f32*)(record + 0x20) = value;
        }
        if (flags & 0x040) {
            const s32 count = func_00137BE0(reader);
            if (count > 0) sly1_discard(reader, 0x1C);
            ++*(s32*)(bytes + 0x50);
        }
        if (flags & 0x080) {
            const u32 vectors = (u32)func_0018D500(0x20);
            *(u32*)(record + 0x30) = vectors;
            func_00137E70(reader, (void*)vectors);
            func_00137E90(reader, (void*)(vectors + 0x10));
        }

        if (flags & 0x100) {
            u8 scratch[64];
            u32 extra = (u32)func_0018D5C0(0x90);
            (void)func_00137D10(reader);
            s32 mode = func_00137CC0(reader);
            if (i == 4) sly1_native_loader_stage(111, (u32)mode, (u32)reader);
            if (mode != -1) {
                (void)func_00137DF8(reader);
                (void)func_00137DF8(reader);
                (void)func_00137DF8(reader);
                (void)func_00137DF8(reader);
                if (extra != 0) func_00137F08(reader, (void*)(extra + 0x40));
                else func_00137F08(reader, scratch);
            }
            if (i == 4) sly1_native_loader_stage(112, (u32)mode, (u32)reader);
            (void)func_00137B90(reader);
            (void)func_00137B90(reader);
            (void)func_00137B90(reader);
            if (i == 4) sly1_native_loader_stage(113, (u32)mode, (u32)reader);
        } else {
            *(u32*)(record + 0x54) = 0;
        }

        func_00137E70(reader, record);
        *(f32*)(record + 0x10) = func_00137DF8(reader);
        (void)func_00137D10(reader);
        *(s32*)(record + 0x34) = func_00137B90(reader);
        *(s32*)(record + 0x14) = func_00137B90(reader);
        *(s32*)(record + 0x2C) = func_00137B90(reader);
        if (i == 4) sly1_native_loader_stage(114, (u32)flags, (u32)reader);
        const s32 nested_count = flags & 1 ? 0 : func_00137BE0(reader);
        if (flags & 1) {
            /* Retail post-processes the referenced record, then starts the
             * next one without reading the nested-record payload. */
            continue;
        }
        if (nested_count < 0 || nested_count > 8192) return;
        *(s32*)(record + 0x58) = nested_count;
        const u32 nested = (u32)func_0018D5C0(nested_count * 0x50);
        *(u32*)(record + 0x5C) = nested;
        for (s32 n = 0; n < nested_count; ++n) {
            u8* entry = (u8*)(nested + (u32)n * 0x50u);
            func_00137A60(reader, 12, entry);
            *(f32*)(entry + 0x10) = func_00137DF8(reader);
            if (n < 4) sly1_native_loader_stage(115, (u32)nested_count, (u32)reader);
            const u32 c0 = (u32)func_00137B90(reader);
            const u32 c1 = (u32)func_00137B90(reader);
            const u32 c2 = (u32)func_00137B90(reader);
            const u32 c3 = (u32)func_00137B90(reader);
            const u32 c4 = (u32)func_00137B90(reader);
            if (n < 4) sly1_native_loader_stage(116, c0 | (c1 << 8) | (c2 << 16) | (c3 << 24), (u32)reader);
            if (n < 4) sly1_native_loader_stage(117, c4, (u32)reader);
            func_00137B60(reader, 4);
            sly1_discard(reader, c0 * 12u);
            sly1_discard(reader, c1 * 12u);
            sly1_discard(reader, c2 * 4u);
            sly1_discard(reader, c3 * 8u);
            sly1_discard(reader, c4 * 4u);
            if (n < 4) sly1_native_loader_stage(118, (u32)nested_count, (u32)reader);
            *(u32*)(entry + 0x18) = (u32)func_00137BE0(reader);
            *(u32*)(entry + 0x14) = (u32)func_00137B90(reader);
            const u32 refs = (u32)func_00137B90(reader);
            if (n < 4) sly1_native_loader_stage(119, refs, (u32)reader);
            *(u32*)(entry + 0x24) = refs;
            for (u32 r = 0; r < refs; ++r) (void)func_00137B90(reader);
            const u32 base_payload_bytes = refs * c0 * 4u;
            sly1_discard(reader, base_payload_bytes);
            if (n < 4) sly1_native_loader_stage(122, base_payload_bytes, (u32)reader);
            if (word_count) {
                const u32 first_dimension = (u32)func_00137BE0(reader);
                if (n < 4) sly1_native_loader_stage(123, first_dimension, (u32)reader);
                sly1_discard(reader, base_payload_bytes * first_dimension * 12u);

                const u32 second_dimension = (u32)func_00137BE0(reader);
                if (n < 4) sly1_native_loader_stage(124, second_dimension, (u32)reader);
                sly1_discard(reader, base_payload_bytes * second_dimension * 12u);

                const u32 row_bytes = c4 * 2u;
                for (s32 row = 0; row < word_count; ++row) {
                    sly1_discard(reader, row_bytes);
                    sly1_discard(reader, row_bytes);
                }
            }
            if (n < 4) sly1_native_loader_stage(120, refs, (u32)reader);
            if ((n & 0xFF) == 0)
                sly1_native_loader_stage(126, (u32)n, (u32)reader);
        }

        const s32 auxiliary_count = func_00137BE0(reader);
        if (auxiliary_count < 0 || auxiliary_count > 4096) return;
        *(s32*)(record + 0x60) = auxiliary_count;
        *(u32*)(record + 0x64) = (u32)func_0018D5C0(auxiliary_count * 0x20);
        if (auxiliary_count > 64) return;
        for (s32 a = 0; a < auxiliary_count; ++a) {
            (void)func_0018D5C0(0x30);
            const u32 c0 = (u32)func_00137B90(reader);
            sly1_discard(reader, c0 * 12u);
            const u32 c1 = (u32)func_00137B90(reader);
            sly1_discard(reader, c1 * 4u);
            const u32 items = (u32)func_00137B90(reader);
            for (u32 j = 0; j < items; ++j) (void)func_00137B90(reader);
            /* Retail stores item_count * first_count 32-bit values. */
            const u32 base_payload_bytes = items * c0 * 4u;
            sly1_discard(reader, base_payload_bytes);
            if (word_count) {
                const u32 first_dimension = (u32)func_00137BE0(reader);
                sly1_discard(reader, base_payload_bytes * first_dimension * 12u);

                const u32 second_dimension = (u32)func_00137BE0(reader);
                sly1_discard(reader, base_payload_bytes * second_dimension * 12u);

                /* The block word count selects two c0*2-byte arrays per row. */
                const u32 row_bytes = c0 * 2u;
                for (s32 row = 0; row < word_count; ++row) {
                    sly1_discard(reader, row_bytes);
                    sly1_discard(reader, row_bytes);
                }
            }
        }
    }
    (void)object;
}

void func_00128C10(void* object, void* reader) {
    u8* self = (u8*)object;
    const s32 flags = func_00137C40(reader);
    if (flags == 0) return;
    const u32 extra = (u32)func_0018D5C0(0xC0);
    *(u32*)(self + 0x224) = extra;
    *(s32*)(extra + 0xB0) = flags;
    if (flags & 0x01) func_00137EB0(reader, (void*)extra);
    if (flags & 0x02) func_00137EB0(reader, (void*)(extra + 0x30));
    if (flags & 0x0C) {
        const s32 value = func_00137D10(reader);
        if (value != -1) *(s32*)(extra + 0x60) = value;
    }
    if (flags & 0x10) *(s32*)(extra + 0x60) = func_00137D10(reader);
    if (flags & 0x20) {
        *(s32*)(extra + 0x84) = func_00137D10(reader);
        func_00137E70(reader, (void*)(extra + 0x60));
        func_00137E70(reader, (void*)(extra + 0x70));
        (void)func_00137DF8(reader);
        *(s32*)(extra + 0x98) = 0;
    }
    if (flags & 0x40) {
        *(s32*)(extra + 0x60) = func_00137D10(reader);
        *(s32*)(extra + 0x64) = func_00137D10(reader);
    }
    if (flags & 0x80) *(s32*)(extra + 0xA0) = func_00137B90(reader);
}

void func_001289C0(void* object, void* reader) {
    u8* self = (u8*)object;
    extern void sly1_native_loader_stage(u32 stage, u32 object, u32 reader);
    func_00137EB0(reader, self + 0xD0);
    func_00137E70(reader, self + 0x100);
    u64 flags = *(u64*)(self + 0x2C8);
    for (u32 i = 0; i < 3; ++i) {
        const u32 shift = 24 + i * 2;
        flags = (flags & ~(3ULL << shift)) |
            ((u64)(func_00137B90(reader) & 3) << shift);
    }
    *(u64*)(self + 0x2C8) = flags;
    *(s32*)(self + 0x88) = func_00137C40(reader);
    *(f32*)(self + 0x80) = func_00137DF8(reader);
    *(f32*)(self + 0x84) = func_00137DF8(reader);
    *(f32*)(self + 0x2A4) = func_00137DF8(reader);
    const f32 f2a8 = func_00137DF8(reader);
    if (*(f32*)(self + 0x80) == 3.402823466e+38f) {
        *(f32*)(self + 0x2A8) = f2a8;
        *(f32*)(self + 0x80) = 1.0e10f;
    }
    if (*(f32*)(self + 0x84) == 3.402823466e+38f)
        *(f32*)(self + 0x84) = 2000.0f / *(f32*)(self + 0x80);
    func_0011EC40(object, reader);
    sly1_native_loader_stage(130, (u32)object, (u32)reader);
    func_00162890(self + 0x22C, reader, object);
    sly1_native_loader_stage(131, (u32)object, (u32)reader);
    func_00128C10(object, reader);

    const u32 descriptor = *(u32*)self;
    const u32 extra = *(u32*)(descriptor + 0x58);
    if (extra != 0)
        (void)sly1_native_call_regs4(extra, (u32)object, (u32)reader, 0, 0);
    sly1_native_loader_stage(132, (u32)object, (u32)reader);

    const s32 group_count = func_00137B90(reader);
    sly1_native_loader_stage(133, (u32)group_count, (u32)reader);
    if (group_count < 0 || group_count > 4096) return;
    *(s32*)(self + 0x2B8) = group_count;
    const u32 groups = (u32)func_0018D500(group_count * 8);
    *(u32*)(self + 0x2BC) = groups;
    for (s32 i = 0; i < group_count; ++i) {
        *(s32*)(groups + (u32)i * 8u) = func_00137D10(reader);
        const s32 value_count = *(s32*)(self + 0x26C);
        if (value_count < 0 || value_count > 65536) return;
        const u32 values = (u32)func_0018D500(value_count * 4);
        *(u32*)(groups + (u32)i * 8u + 4) = values;
        for (s32 n = 0; n < value_count; ++n)
            *(f32*)(values + (u32)n * 4u) = func_00137DF8(reader);
    }
    sly1_native_loader_stage(134, (u32)group_count, (u32)reader);
    /* Retail has one byte after the group curves before the child list. */
    (void)func_00137B90(reader);
    func_0011ECB8((void*)*(u32*)(self + 0x14), object, reader);
}
