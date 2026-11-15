// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only C equivalent of the retail look-direction block setup. The
// matching function remains original assembly in sly1-decomp.
#include "common.h"
#include "globals.h"
#include "placed_object.h"

extern "C" void func_001583E8(void* out, const void* source)
{
    const u8* src = (const u8*)source;
    u8* dst = (u8*)out;

    // This block is also consumed as EffectSpinRecord: scale at 0, rate at
    // 0x20, axis at 0x30, then mode and frame at 0x40/0x44. The caller owns
    // the other bytes (translation and padding).
    __builtin_memcpy(dst + 0x00, src + 0xC0, sizeof(f32));
    __builtin_memcpy(dst + 0x20, src + 0xC4, sizeof(f32));

    Vec4 axis;
    __builtin_memcpy(&axis, src + 0xD0, sizeof(axis));
    const f32 length_squared = axis.x * axis.x + axis.y * axis.y + axis.z * axis.z;
    if (__builtin_sqrtf(length_squared) < 0.0001f) {
        axis = g_unitZVector;
    } else {
        const f32 inverse_length = 1.0f / __builtin_sqrtf(length_squared);
        axis.x *= inverse_length;
        axis.y *= inverse_length;
        axis.z *= inverse_length;
        axis.w *= inverse_length;
    }
    __builtin_memcpy(dst + 0x30, &axis, sizeof(axis));
    __builtin_memcpy(dst + 0x40, src + 0xE0, sizeof(u32));
    __builtin_memcpy(dst + 0x44, src + 0x7C, sizeof(u32));
}
