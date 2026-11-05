/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only scalar replacement for the retail VU0 camera-vector update. */
#include "sly1/host_abi.h"

struct Vec4_001848B8 { f32 v[4]; };

extern "C" {
extern const Vec4_001848B8 D_002472E0;
}

void func_001841D0(void* self);
void func_00184B58(void* self);

extern "C" void func_001848B8(void* self, const Vec4_001848B8* source)
    __asm__("_Z13func_001848B8P3C51Pv");

extern "C" void func_001848B8(void* self, const Vec4_001848B8* source) {
    u8* object = static_cast<u8*>(self);
    f32* input = reinterpret_cast<f32*>(object + 0x310u);
    f32* output = reinterpret_cast<f32*>(object + 0x320u);

    input[0] = source->v[0];
    input[1] = source->v[1];
    input[2] = source->v[2];
    input[3] = source->v[3];

    const f32 length_squared = source->v[0] * source->v[0] +
                               source->v[1] * source->v[1] +
                               source->v[2] * source->v[2];
    const f32 length = __builtin_sqrtf(length_squared);
    if (0.0001f < length) {
        const f32 scale = -1.0f / length;
        output[0] = source->v[0] * scale;
        output[1] = source->v[1] * scale;
        output[2] = source->v[2] * scale;
        output[3] = source->v[3] * scale;
    } else {
        output[0] = D_002472E0.v[0];
        output[1] = D_002472E0.v[1];
        output[2] = D_002472E0.v[2];
        output[3] = D_002472E0.v[3];
    }

    func_001841D0(self);
    func_00184B58(self);
}
