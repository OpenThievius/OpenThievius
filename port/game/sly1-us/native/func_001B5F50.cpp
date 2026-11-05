// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only stream backfill for the scene reference table. It preserves the
// stream position and table records, while retail object-link construction is
// deferred until its referenced decompilation is available.
#include "common.h"
#include <stddef.h>

extern "C" s32 func_00137B90(void* reader);
extern "C" s32 func_00137CC0(void* reader);
extern "C" s32 func_00137D10(void* reader);
extern "C" s32 func_00137D78(void* reader);
extern "C" f32 func_00137DF8(void* reader);
extern "C" u8 D_002505D8[];
extern "C" void* func_0018D500(s32 size);
extern "C" void func_00152438(void* list, s32 stride);
extern "C" s32 func_001B63F8(void* object, s32 key);
extern "C" void* func_0011E580(s32 index, void* owner, void* context,
                                s32 key, s32 flags);
extern "C" void func_00182ED8(void* object);
extern "C" void sly1_port_trace_reader(u32 stage, void* reader);
extern "C" void sly1_port_trace_value(u32 tag, u32 value);
extern "C" void __attribute__((noreturn)) sly1_missing_function(
    u32 address, u32 caller, u32 argument0, u32 argument1, u32 argument2);

struct ScenePair_1B5F50 {
    s32 key;
    s32 target;
};

struct SceneEntry_1B5F50 {
    s32 enabled;
    s32 target0;
    s32 value;
    s32 duplicate_value;
    f32 weight;
};

struct PropertyDescriptor_1B5F50 {
    s32 type;
    u32 flags;
    u8 pad8[0x0c];
    s32 count;
    s32 offset;
    void* (*get_object)(void* object, s32 one);
};

static void skip_value_1B5F50(void* reader, s32 type) {
    switch (type) {
    case 0: (void)func_00137B90(reader); break;
    case 5:
    case 13: (void)func_00137D78(reader); break;
    case 1: (void)func_00137DF8(reader); break;
    case 10:
        (void)func_00137DF8(reader);
        (void)func_00137DF8(reader);
        break;
    case 2:
    case 9:
    case 11:
    case 4:
        for (s32 i = 0; i < 3; ++i) (void)func_00137DF8(reader);
        break;
    case 3:
    case 12:
        for (s32 i = 0; i < 4; ++i) (void)func_00137DF8(reader);
        break;
    default: (void)func_00137D10(reader); break;
    }
}

static void skip_property_1B5F50(void* reader,
                                 const PropertyDescriptor_1B5F50& descriptor) {
    if ((descriptor.flags & 0x400u) == 0) {
        skip_value_1B5F50(reader, descriptor.type);
        return;
    }
    for (s32 i = 0; i < descriptor.count; ++i) {
        const s32 type = func_00137D10(reader);
        if (type == 0) (void)func_00137B90(reader);
        else if (type == 5) (void)func_00137D78(reader);
        else if (type == 1 || type == 2 || type == 9 || type == 10) {
            const s32 n = type == 1 ? 1 : (type == 10 ? 2 : 3);
            for (s32 f = 0; f < n; ++f) (void)func_00137DF8(reader);
        } else {
            (void)func_00137D10(reader);
        }
    }
}

static_assert(sizeof(ScenePair_1B5F50) == 8, "retail scene pair size");
static_assert(sizeof(SceneEntry_1B5F50) == 0x14, "retail scene entry size");

extern "C" void func_001B5F50(void* object, void* reader) {
    auto unsupported = [&]() __attribute__((noreturn)) {
        sly1_missing_function(0x001B5F50u,
            reinterpret_cast<u32>(__builtin_return_address(0)),
            reinterpret_cast<u32>(object), reinterpret_cast<u32>(reader), 0);
    };

    auto* bytes = static_cast<u8*>(object);
    func_00152438(bytes + 0x48, 8);

    const s32 pair_count = func_00137B90(reader);
    sly1_port_trace_value(0xb0, static_cast<u32>(pair_count));
    if (pair_count < 0 || pair_count > 255)
        unsupported();
    *reinterpret_cast<s32*>(bytes + 0x34) = pair_count;
    auto* pairs = static_cast<ScenePair_1B5F50*>(func_0018D500(pair_count * 8));
    *reinterpret_cast<ScenePair_1B5F50**>(bytes + 0x38) = pairs;
    for (s32 i = 0; i < pair_count; ++i) {
        pairs[i].key = func_00137D10(reader);
        pairs[i].target = func_00137D10(reader);
        if (pairs[i].target != -1)
            pairs[i].target = func_001B63F8(object, pairs[i].target);
    }
    sly1_port_trace_reader(0xb1, reader);

    const s32 entry_count = func_00137B90(reader);
    sly1_port_trace_value(0xb1, static_cast<u32>(entry_count));
    if (entry_count < 0 || entry_count > 255)
        unsupported();
    *reinterpret_cast<s32*>(bytes + 0x3c) = entry_count;
    auto* entries = static_cast<SceneEntry_1B5F50*>(func_0018D500(entry_count * 0x14));
    *reinterpret_cast<SceneEntry_1B5F50**>(bytes + 0x40) = entries;
    bool has_link[255] = {};
    for (s32 i = 0; i < entry_count; ++i) {
        auto& entry = entries[i];
        entry.enabled = func_00137CC0(reader);
        has_link[i] = entry.enabled != 0;
        sly1_port_trace_value(0xb2, static_cast<u32>(entry.enabled));
        const s32 target0 = func_00137D10(reader);
        entry.target0 = target0 == -1 ? -1 : func_001B63F8(object, target0);
        const s32 target1 = func_00137D10(reader);
        entry.target0 = target1 == -1 ? -1 : func_001B63F8(object, target1);
        entry.value = func_00137D78(reader);
        entry.duplicate_value = entry.value;
        entry.weight = func_00137DF8(reader);
    }
    sly1_port_trace_reader(0xb2, reader);

    // Preserve stream alignment for property values while their runtime
    // callbacks remain unimplemented in this port-only backfill.
    for (s32 i = 0; i < 4096; ++i) {
        const s32 key = func_00137D10(reader);
        if (key < 0) break;
        if (key >= 1179) unsupported();
        const auto* descriptors =
            reinterpret_cast<const PropertyDescriptor_1B5F50*>(D_002505D8);
        skip_property_1B5F50(reader, descriptors[key]);
        if (i == 4095) unsupported();
    }
    sly1_port_trace_reader(0xb3, reader);

    const s32 root_index = func_00137D10(reader);
    sly1_port_trace_value(0xb3, static_cast<u32>(root_index));
    for (s32 i = 0; i < entry_count; ++i) {
        if (!has_link[i])
            continue;
        const s32 first = func_00137D10(reader);
        const s32 second = func_00137D10(reader);
        const s32 index = func_00137D10(reader);
        sly1_port_trace_value(0xb5, static_cast<u32>(first));
        sly1_port_trace_value(0xb6, static_cast<u32>(second));
        sly1_port_trace_value(0xb7, static_cast<u32>(index));
        if (first < 0 || first >= 149)
            unsupported();
        // Retail passes the serialized values as slot/key/flags and inherits
        // the owner and context from this scene object's constructor fields.
        void* owner = *reinterpret_cast<void**>(bytes + 0x14);
        void* context = *reinterpret_cast<void**>(bytes + 0x18);
        void* child = func_0011E580(first, owner, context, second, index);
        using SceneMethod = void (*)(void*, void*);
        auto* vtable = *reinterpret_cast<SceneMethod**>(child);
        vtable[14](child, reader);
        func_00182ED8(child);
        entries[i].enabled = reinterpret_cast<s32>(child);
    }
    sly1_port_trace_reader(0xb4, reader);
}
