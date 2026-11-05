// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only object parent/transform update for the native host build.
#include "common.h"

extern "C" s32 func_001278A8(void* object, void* base, const f32* input,
                              f32* first_axis, f32* translation,
                              f32* second_axis, f32* rotation);
extern "C" s32 func_00128018(void* first, void* second, const void* input, void* output);
extern "C" void func_001280C0(void* first, void* second,
                                const f32* input, f32* output);
extern "C" void func_00128150(const void* first, const void* second,
                                const f32* input, f32* output);
extern "C" void func_00124540(void* object);

extern "C" void func_001245A8(void* object, void* parent) {
    auto* bytes = static_cast<u8*>(object);
    if (*reinterpret_cast<void**>(bytes + 0x18) == parent) return;

    f32 scratch[64] __attribute__((aligned(16))) = {};
    f32* local_position = scratch + 4;
    f32* local_rotation = scratch + 8;
    if (parent != nullptr) {
        func_001278A8(object, nullptr,
            reinterpret_cast<const f32*>(bytes + 0x140),
            local_position, local_rotation, nullptr, nullptr);
    } else {
        for (s32 lane = 0; lane < 4; ++lane) {
            local_position[lane] = *reinterpret_cast<f32*>(bytes + 0x150 + lane * 4);
            local_rotation[lane] = *reinterpret_cast<f32*>(bytes + 0x160 + lane * 4);
        }
    }

    auto** vtable = *reinterpret_cast<void***>(object);
    using Callback0 = void (*)(void*);
    using Callback1 = void (*)(void*, void*);
    reinterpret_cast<Callback0*>(vtable)[7](object);

    func_00128018(nullptr, parent, scratch, bytes + 0x100);
    func_00128150(nullptr, parent, scratch + 12,
        reinterpret_cast<f32*>(bytes + 0xD0));

    if (parent != nullptr) {
        f32 parent_position[4] = {};
        f32 parent_rotation[4] = {};
        func_001278A8(parent, nullptr,
            reinterpret_cast<const f32*>(bytes + 0x140),
            parent_position, parent_rotation, nullptr, nullptr);
        for (s32 lane = 0; lane < 4; ++lane) {
            local_position[lane] -= parent_position[lane];
            local_rotation[lane] -= parent_rotation[lane];
        }
    }

    f32* transformed_position = scratch + 24;
    f32* transformed_rotation = scratch + 28;
    func_001280C0(nullptr, parent, local_position, transformed_position);
    reinterpret_cast<Callback1*>(vtable)[36](object, transformed_position);
    func_001280C0(nullptr, parent, local_rotation, transformed_rotation);
    reinterpret_cast<Callback1*>(vtable)[37](object, transformed_rotation);

    const bool had_parent = *reinterpret_cast<void**>(bytes + 0x18) != nullptr;
    if (had_parent != (parent != nullptr)) {
        u64 flags = *reinterpret_cast<u64*>(bytes + 0x2C8);
        if (parent == nullptr) {
            flags = (flags & 0xFCFFFFFFull) | 0x02000000ull;
            if ((flags & 0x0C000000ull) != 0x04000000ull)
                flags = (flags & 0xF3FFFFFFull) | 0x08000000ull;
        } else {
            flags = (flags & 0xFCFFFFFFull) | 0x01000000ull;
            if ((flags & 0x0C000000ull) != 0x04000000ull)
                flags &= 0xF3FFFFFFull;
        }
        const u64 mode = flags & 0x30000000ull;
        const bool keep_special = mode == 0x20000000ull &&
            *reinterpret_cast<const u32*>(bytes + 0x80) == 0x501502F9u;
        if (mode != 0x10000000ull && !keep_special)
            flags &= 0xCFFFFFFFull;
        *reinterpret_cast<u64*>(bytes + 0x2C8) = flags;
    }

    func_00124540(object);
    *reinterpret_cast<void**>(bytes + 0x18) = parent;
    reinterpret_cast<Callback0*>(vtable)[6](object);
}
