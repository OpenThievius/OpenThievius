// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only transform-chain helper for the native host build.
#include "common.h"

extern "C" void func_00188520(f32* out, const f32* matrix, const f32* vectors);

extern "C" void func_00128150(const void* first, const void* second,
                                const f32* input, f32* output) {
    if (first == second) {
        for (s32 i = 0; i < 12; ++i) output[i] = input[i];
        return;
    }

    f32 intermediate[12];
    const f32* current = input;
    if (first != nullptr) {
        const auto* matrix = reinterpret_cast<const f32*>(
            static_cast<const u8*>(first) + 0x110);
        for (s32 vector = 0; vector < 3; ++vector) {
            const f32* v = input + vector * 4;
            f32* dst = intermediate + vector * 4;
            for (s32 row = 0; row < 3; ++row) {
                const f32* basis = matrix + row * 4;
                dst[row] = basis[0] * v[0] + basis[1] * v[1] + basis[2] * v[2];
            }
            dst[3] = v[3];
        }
        current = intermediate;
    }

    if (second != nullptr) {
        func_00188520(output,
            reinterpret_cast<const f32*>(static_cast<const u8*>(second) + 0x110), current);
    } else if (current != input) {
        for (s32 i = 0; i < 12; ++i) output[i] = current[i];
    } else {
        for (s32 i = 0; i < 12; ++i) output[i] = input[i];
    }
}
