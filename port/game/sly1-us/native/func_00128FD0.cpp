// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only attachment transform callback for the native host build.
#include "common.h"

extern "C" void func_00189150(const f32* a, const f32* b, f32* out);
extern "C" void func_00188520(f32* out, const f32* matrix, const f32* vectors);

extern "C" void func_00128FD0(void* object, f32* vectors) {
    auto* bytes = static_cast<u8*>(object);
    auto* attachment = *reinterpret_cast<u8**>(bytes + 0x224);
    if (attachment == nullptr) return;

    const u32 flags = *reinterpret_cast<const u32*>(attachment + 0xB0);
    if ((flags & 1u) != 0) {
        f32 transformed[12];
        func_00189150(reinterpret_cast<const f32*>(attachment), vectors, transformed);
        for (s32 i = 0; i < 12; ++i) vectors[i] = transformed[i];
    }
    if ((flags & 2u) != 0) {
        f32 transformed[12];
        func_00188520(transformed,
            reinterpret_cast<const f32*>(attachment + 0x30), vectors);
        for (s32 i = 0; i < 12; ++i) vectors[i] = transformed[i];
    }
}
