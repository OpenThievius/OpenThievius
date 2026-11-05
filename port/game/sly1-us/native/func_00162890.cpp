// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only GLOBSET stream backfill. It parses common record data and skips
// variable payloads; VU-specific geometry construction remains incomplete.
#include "common.h"

extern "C" s32 func_00137B90(void* reader);
extern "C" s32 func_00137BE0(void* reader);
extern "C" s32 func_00137D10(void* reader);
extern "C" s32 func_00137C40(void* reader);
extern "C" f32 func_00137DF8(void* reader);
extern "C" void func_00137A60(void* reader, s32 size, void* destination);
extern "C" void func_00137B60(void* reader, s32 alignment);
extern "C" void func_00137E70(void* reader, void* destination);
extern "C" void func_00137E90(void* reader, void* destination);
extern "C" void* func_0018D500(s32 size);
extern "C" void* func_0018D5C0(s32 size);
extern "C" void* func_0018D568(s32 size, void* previous);
extern "C" void func_00165AE8(void* block, void* record, void* companion);
extern "C" void func_00162810(void* block);
extern "C" void sly1_port_trace_reader(u32 stage, void* reader);
extern "C" void sly1_port_trace_value(u32 tag, u32 value);
extern "C" void sly1_port_trace_nested_counts(
    u32 count0, u32 count1, u32 count2, u32 count3, u32 count4);
extern "C" __attribute__((noreturn)) void sly1_missing_function(
    u32 address, u32 caller, u32 argument0, u32 argument1, u32 argument2);

extern "C" void func_00162890(void* block, void* reader, void* object) {
    auto* bytes = static_cast<u8*>(block);
    *reinterpret_cast<s32*>(bytes + 0x50) = 0;

    // The first two bytes are retained only in the retail stack frame; the
    // third is a count of 0x90-byte component records.
    const s32 first = func_00137B90(reader);
    const s32 second = func_00137B90(reader);
    const s32 component_count = func_00137B90(reader);
    sly1_port_trace_reader(1, reader);
    (void)first;
    (void)second;
    if (component_count < 0 || component_count > 4096)
        sly1_missing_function(0x00162890u,
            reinterpret_cast<u32>(__builtin_return_address(0)),
            reinterpret_cast<u32>(block), reinterpret_cast<u32>(reader),
            reinterpret_cast<u32>(object));

    *reinterpret_cast<s32*>(bytes + 0x00) = component_count;
    *reinterpret_cast<void**>(bytes + 0x04) = func_0018D5C0(component_count * 0x90);
    auto* component_ids = static_cast<s16*>(func_0018D5C0(component_count * 4));
    *reinterpret_cast<void**>(bytes + 0x08) = component_ids;
    for (s32 i = 0; i < component_count; ++i)
        component_ids[i] = static_cast<s16>(func_00137D10(reader));
    sly1_port_trace_reader(2, reader);

    const s32 word_count = func_00137B90(reader);
    sly1_port_trace_reader(3, reader);
    if (word_count < 0 || word_count > 65535)
        sly1_missing_function(0x00162890u,
            reinterpret_cast<u32>(__builtin_return_address(0)),
            reinterpret_cast<u32>(block), reinterpret_cast<u32>(reader),
            reinterpret_cast<u32>(object));
    *reinterpret_cast<s32*>(bytes + 0x40) = word_count;
    void* words = func_0018D5C0(word_count * 4);
    *reinterpret_cast<void**>(bytes + 0x44) = words;
    func_00137A60(reader, word_count * 4, words);
    sly1_port_trace_reader(4, reader);
    *reinterpret_cast<void**>(bytes + 0x48) = func_0018D568(word_count * 4, words);

    const s32 record_count = func_00137BE0(reader);
    sly1_port_trace_reader(5, reader);
    if (record_count < 0 || record_count > 65535)
        sly1_missing_function(0x00162890u,
            reinterpret_cast<u32>(__builtin_return_address(0)),
            reinterpret_cast<u32>(block), reinterpret_cast<u32>(reader),
            reinterpret_cast<u32>(object));
    *reinterpret_cast<s32*>(bytes + 0x0C) = record_count;
    *reinterpret_cast<void**>(bytes + 0x10) = func_0018D5C0(record_count * 0x70);
    *reinterpret_cast<void**>(bytes + 0x14) = func_0018D5C0(record_count * 0x28);
    auto unsupported = [&]() __attribute__((noreturn)) {
        sly1_missing_function(0x00162890u,
            reinterpret_cast<u32>(__builtin_return_address(0)),
            reinterpret_cast<u32>(block), reinterpret_cast<u32>(reader),
            reinterpret_cast<u32>(object));
    };

    auto* records = static_cast<u8*>(*reinterpret_cast<void**>(bytes + 0x10));
    for (s32 i = 0; i < record_count; ++i) {
        auto* record = records + i * 0x70;
        const s32 flags = func_00137BE0(reader);
        sly1_port_trace_reader(6, reader);
        *reinterpret_cast<s32*>(record + 0x68) = flags;
        if ((flags & 0x101) != 0)
            unsupported();

        // Initialize the common defaults before applying the optional fields.
        *reinterpret_cast<u32*>(record + 0x1c) = 0x501502f9u;
        *reinterpret_cast<u32*>(record + 0x20) = 0x44fa0000u;
        *reinterpret_cast<u32*>(record + 0x24) = 0x7f7fffffu;
        *reinterpret_cast<u32*>(record + 0x28) = static_cast<u32>(first);
        *reinterpret_cast<u32*>(record + 0x30) = 0;
        *reinterpret_cast<u32*>(record + 0x38) = 0;
        *reinterpret_cast<u32*>(record + 0x3c) = 0x3f800000u;
        *reinterpret_cast<u32*>(record + 0x50) = 0x3f800000u;

        if (flags & 0x2)
            *reinterpret_cast<s32*>(record + 0x00) = func_00137C40(reader);
        if (flags & 0x200)
            *reinterpret_cast<f32*>(record + 0x50) = func_00137DF8(reader);
        if (flags & 0x4) {
            f32 value = func_00137DF8(reader);
            if (value == 3.402823466e+38f)
                value = 3.402823466e+38f;
            else
                value *= value < 0 ? -value : value;
            *reinterpret_cast<f32*>(record + 0x24) = value;
        }
        if (flags & 0x8)
            *reinterpret_cast<f32*>(record + 0x3c) = func_00137DF8(reader);
        if (flags & 0x10) {
            f32 value = func_00137DF8(reader);
            const u32 bits = value == 3.402823466e+38f
                ? 0x501502f9u : *reinterpret_cast<u32*>(&value);
            *reinterpret_cast<u32*>(record + 0x1c) = bits;
        }
        if (flags & 0x20) {
            const f32 value = func_00137DF8(reader);
            f32& limit = *reinterpret_cast<f32*>(record + 0x20);
            if (value != 3.402823466e+38f && value < limit) limit = value;
        }
        if (flags & 0x40) {
            const s32 extension_type = func_00137BE0(reader);
            sly1_port_trace_value(0x40, static_cast<u32>(extension_type));
            u8 extension_data[0x1c];
            func_00137A60(reader, sizeof(extension_data), extension_data);
            *reinterpret_cast<void**>(record + 0x38) = nullptr;
            ++*reinterpret_cast<s32*>(bytes + 0x50);
        }

        if (flags & 0x80) {
            auto* vectors = static_cast<u8*>(func_0018D500(0x20));
            *reinterpret_cast<void**>(record + 0x30) = vectors;
            func_00137E70(reader, vectors);
            func_00137E90(reader, vectors + 0x10);
        } else {
            *reinterpret_cast<void**>(record + 0x30) = nullptr;
        }

        if (flags & 0x100)
            unsupported();

        func_00137A60(reader, 12, record);
        *reinterpret_cast<f32*>(record + 0x10) = func_00137DF8(reader);
        (void)func_00137D10(reader);
        *reinterpret_cast<s32*>(record + 0x34) = func_00137B90(reader);
        *reinterpret_cast<s32*>(record + 0x14) = func_00137B90(reader);
        *reinterpret_cast<s32*>(record + 0x2c) = func_00137B90(reader);

        if (flags & 0x2) {
            auto* companions = static_cast<u8*>(*reinterpret_cast<void**>(bytes + 0x14));
            func_00165AE8(block, record, companions + i * 0x28);
            break;
        }

        const s32 nested_count = func_00137BE0(reader);
        sly1_port_trace_value(0x58, static_cast<u32>(nested_count));
        if (nested_count < 0 || nested_count > 16)
            unsupported();
        *reinterpret_cast<s32*>(record + 0x58) = nested_count;
        auto* nested = static_cast<u8*>(func_0018D5C0(nested_count * 0x50));
        *reinterpret_cast<void**>(record + 0x5c) = nested;
        auto discard = [&](u32 size) {
            u8 scratch[256];
            while (size != 0) {
                const s32 chunk = size > sizeof(scratch)
                    ? static_cast<s32>(sizeof(scratch))
                    : static_cast<s32>(size);
                func_00137A60(reader, chunk, scratch);
                size -= static_cast<u32>(chunk);
            }
        };
        const s32 word_count_value = *reinterpret_cast<s32*>(bytes + 0x40);
        if (word_count_value != 0 && nested_count != 0)
            unsupported();
        for (s32 nested_index = 0; nested_index < nested_count; ++nested_index) {
            auto* entry = nested + nested_index * 0x50;
            func_00137A60(reader, 12, entry);
            *reinterpret_cast<f32*>(entry + 0x10) = func_00137DF8(reader);
            const u32 count0 = static_cast<u32>(func_00137B90(reader));
            const u32 count1 = static_cast<u32>(func_00137B90(reader));
            const u32 count2 = static_cast<u32>(func_00137B90(reader));
            const u32 count3 = static_cast<u32>(func_00137B90(reader));
            const u32 count4 = static_cast<u32>(func_00137B90(reader));
            sly1_port_trace_nested_counts(count0, count1, count2, count3, count4);
            func_00137B60(reader, 4);
            discard(count0 * 12);
            discard(count1 * 12);
            discard(count2 * 4);
            discard(count3 * 8);
            discard(count4 * 4);
            *reinterpret_cast<u32*>(entry + 0x18) =
                static_cast<u32>(func_00137BE0(reader));
            *reinterpret_cast<u32*>(entry + 0x14) =
                static_cast<u32>(func_00137B90(reader));
            const u32 reference_count = static_cast<u32>(func_00137B90(reader));
            *reinterpret_cast<u32*>(entry + 0x24) = reference_count;
            for (u32 reference_index = 0; reference_index < reference_count;
                 ++reference_index)
                (void)func_00137B90(reader);
            discard(reference_count * count0 * 4);
            sly1_port_trace_reader(0x80u + static_cast<u32>(nested_index), reader);
        }

        const s32 auxiliary_count = func_00137BE0(reader);
        sly1_port_trace_value(0x60, static_cast<u32>(auxiliary_count));
        if (auxiliary_count < 0)
            unsupported();
        *reinterpret_cast<s32*>(record + 0x60) = auxiliary_count;
        *reinterpret_cast<void**>(record + 0x64) =
            func_0018D5C0(auxiliary_count * 0x20);
        if (auxiliary_count > 64) {
            u8 vector[12];
            func_00137A60(reader, sizeof(vector), vector);
            (void)func_00137DF8(reader);
            const u32 count0 = static_cast<u32>(func_00137B90(reader));
            const u32 count1 = static_cast<u32>(func_00137B90(reader));
            const u32 count2 = static_cast<u32>(func_00137B90(reader));
            sly1_port_trace_nested_counts(count0, count1, count2, 0, 0);
            unsupported();
        }
        for (s32 auxiliary_index = 0; auxiliary_index < auxiliary_count;
             ++auxiliary_index) {
            (void)func_0018D5C0(0x30);
            const u32 count0 = static_cast<u32>(func_00137B90(reader));
            if (count0 > 255)
                unsupported();
            discard(count0 * 12);
            const u32 count1 = static_cast<u32>(func_00137B90(reader));
            discard(count1 * 4);
            const u32 item_count = static_cast<u32>(func_00137B90(reader));
            for (u32 item = 0; item < item_count; ++item)
                (void)func_00137B90(reader);
            discard(item_count * 4);
            if (word_count_value != 0)
                unsupported();
            sly1_port_trace_reader(0xa0u + static_cast<u32>(auxiliary_index), reader);
        }
        sly1_port_trace_reader(0x90u + static_cast<u32>(i), reader);
    }
    func_00162810(block);
}
