// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only common object stream reader. The retail body includes VU0 code and
// dispatches a type-specific block to 0x00162890; this version mirrors its
// scalar stream fields while keeping that per-type implementation replaceable.
#include "common.h"
#include "native_function_lookup.hpp"

extern "C" void func_00137EB0(void* reader, void* destination);
extern "C" void func_00137E70(void* reader, void* destination);
extern "C" s32 func_00137B90(void* reader);
extern "C" s32 func_00137C40(void* reader);
extern "C" s32 func_00137D10(void* reader);
extern "C" f32 func_00137DF8(void* reader);
extern "C" void* func_0018D500(s32 size);
extern "C" void func_00109488(void);
extern "C" void func_0011EC40(void* self, void* reader);
extern "C" void func_00162890(void* block, void* reader, void* self);
extern "C" void func_00128C10(void* self, void* reader);
extern "C" void func_0011ECB8(void* world, void* self, void* reader);

struct SlyGroupCurve {
    s32 key;
    f32* values;
};

extern "C" void func_001289C0(void* object, void* reader) {
    auto* self = static_cast<u8*>(object);
    func_00137EB0(reader, self + 0xD0);
    func_00137E70(reader, self + 0x100);

    u64 flags = *reinterpret_cast<u64*>(self + 0x2C8);
    for (u32 field = 0; field < 3; ++field) {
        const u32 shift = 24 + field * 2;
        const u64 mask = ~(u64(3) << shift);
        flags = (flags & mask) | (u64(func_00137B90(reader) & 3) << shift);
    }
    *reinterpret_cast<u64*>(self + 0x2C8) = flags;

    *reinterpret_cast<s32*>(self + 0x88) = func_00137C40(reader);
    *reinterpret_cast<f32*>(self + 0x80) = func_00137DF8(reader);
    *reinterpret_cast<f32*>(self + 0x84) = func_00137DF8(reader);
    *reinterpret_cast<f32*>(self + 0x2A4) = func_00137DF8(reader);
    const f32 f2a8 = func_00137DF8(reader);

    // Retail uses FLT_MAX as an unset sentinel for these two limits.
    const f32 unset = 3.402823466e+38f;
    if (*reinterpret_cast<f32*>(self + 0x80) == unset) {
        *reinterpret_cast<f32*>(self + 0x2A8) = f2a8;
        *reinterpret_cast<f32*>(self + 0x80) = 1.0e10f;
    }
    if (*reinterpret_cast<f32*>(self + 0x84) == unset) {
        *reinterpret_cast<f32*>(self + 0x84) = 2000.0f / *reinterpret_cast<f32*>(self + 0x80);
    }

    // The retail constructor table is filled by this initializer before the
    // first object stream is read. Repeating it is safe and protects native
    // startup paths that enter this loader without running that constructor.
    func_00109488();
    func_0011EC40(object, reader);
    func_00162890(self + 0x22C, reader, object);
    func_00128C10(object, reader);

    using ExtraReader = void (*)(void*, void*);
    auto** descriptor = *reinterpret_cast<void***>(self);
    const u32 retail_extra = reinterpret_cast<u32>(descriptor[0x58 / 4]);
    const u32 native_extra = sly1::native_function_address(retail_extra);
    auto extra = reinterpret_cast<ExtraReader>(native_extra);
    if (extra != nullptr) extra(object, reader);

    const s32 group_count = func_00137B90(reader);
    if (group_count < 0 || group_count > 4096) return;
    *reinterpret_cast<s32*>(self + 0x2B8) = group_count;
    auto* groups = static_cast<SlyGroupCurve*>(func_0018D500(group_count * sizeof(SlyGroupCurve)));
    *reinterpret_cast<SlyGroupCurve**>(self + 0x2BC) = groups;
    for (s32 group = 0; group < group_count; ++group) {
        groups[group].key = func_00137D10(reader);
        s32 value_count = *reinterpret_cast<s32*>(self + 0x26C);
        if (value_count < 0 || value_count > 65536) value_count = 0;
        groups[group].values = static_cast<f32*>(func_0018D500(value_count * sizeof(f32)));
        for (s32 value = 0; value < value_count; ++value)
            groups[group].values[value] = func_00137DF8(reader);
    }

    func_0011ECB8(*reinterpret_cast<void**>(self + 0x14), object, reader);
}
