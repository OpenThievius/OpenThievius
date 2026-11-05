// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only object transform initialization for the native host build.
#include "common.h"

extern "C" s32 func_00128018(void* first, void* second, const void* input, void* output);
extern "C" void func_00128150(const void* first, const void* second,
                                const f32* input, f32* output);
extern "C" s32 func_001278A8(void* object, void* base, const f32* input,
                              f32* first_axis, f32* translation,
                              f32* second_axis, f32* rotation);
extern "C" void func_001280C0(void* first, void* second,
                                const f32* input, f32* output);

extern "C" void func_00124868(void* object, const void* source) {
    auto* bytes = static_cast<u8*>(object);
    const auto* input = static_cast<const u8*>(source);
    f32 scratch[56] __attribute__((aligned(16))) = {};

    func_00128018(const_cast<void*>(source), nullptr, bytes + 0x100, scratch);
    auto** vtable = *reinterpret_cast<void***>(object);
    using Callback = void (*)(void*, void*);
    reinterpret_cast<Callback*>(vtable)[33](object, scratch);

    func_00128150(source, nullptr,
        reinterpret_cast<const f32*>(bytes + 0xD0), scratch + 4);
    reinterpret_cast<Callback*>(vtable)[34](object, scratch + 4);

    func_001278A8(*reinterpret_cast<void* const*>(input + 0x18), nullptr,
        scratch, scratch + 0x10, scratch + 0x14, nullptr, nullptr);
    func_001280C0(const_cast<void*>(source), nullptr,
        reinterpret_cast<const f32*>(bytes + 0x150), scratch + 0x18);
    func_001280C0(const_cast<void*>(source), nullptr,
        reinterpret_cast<const f32*>(bytes + 0x160), scratch + 0x1C);
    for (s32 lane = 0; lane < 4; ++lane) {
        scratch[0x20 + lane] = scratch[0x10 + lane] + scratch[0x18 + lane];
        scratch[0x24 + lane] = scratch[0x14 + lane] + scratch[0x1C + lane];
    }

    reinterpret_cast<Callback*>(vtable)[36](object, scratch + 0x20);
    reinterpret_cast<Callback*>(vtable)[37](object, scratch + 0x24);

    for (s32 lane = 0; lane < 4; ++lane) {
        *reinterpret_cast<f32*>(bytes + 0x190 + lane * 4) =
            *reinterpret_cast<const f32*>(bytes + 0x100 + lane * 4);
        *reinterpret_cast<f32*>(bytes + 0x1A0 + lane * 4) =
            *reinterpret_cast<const f32*>(bytes + 0xD0 + lane * 4);
        *reinterpret_cast<f32*>(bytes + 0x1B0 + lane * 4) =
            *reinterpret_cast<const f32*>(bytes + 0xE0 + lane * 4);
        *reinterpret_cast<f32*>(bytes + 0x1C0 + lane * 4) =
            *reinterpret_cast<const f32*>(bytes + 0xF0 + lane * 4);
    }
}
