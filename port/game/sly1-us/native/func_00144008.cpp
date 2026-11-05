/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only scalar translation of the retail VU0 camera basis update. */
#include "sly1/host_abi.h"

struct Vec4_00144008 { f32 v[4]; };

extern "C" {
extern const Vec4_00144008 D_002472E0;
extern const Vec4_00144008 D_00248D00;
}

extern "C" void func_00144008(void* self) __asm__("_Z13func_00144008Pv");

extern "C" void func_00144008(void* self) {
    u8* object = static_cast<u8*>(self);
    const f32 x = -(*reinterpret_cast<const f32*>(object + 0x80u));
    const f32 y = -(*reinterpret_cast<const f32*>(object + 0x84u));
    const f32 length = __builtin_sqrtf(x * x + y * y);

    Vec4_00144008 forward;
    if (length < 0.0001f) {
        forward = D_00248D00;
    } else {
        const f32 scale = 1.0f / length;
        forward.v[0] = x * scale;
        forward.v[1] = y * scale;
        forward.v[2] = 0.0f;
        forward.v[3] = 0.0f;
    }

    const Vec4_00144008 up = D_002472E0;
    Vec4_00144008 right = {};
    right.v[0] = up.v[1] * forward.v[2] - up.v[2] * forward.v[1];
    right.v[1] = up.v[2] * forward.v[0] - up.v[0] * forward.v[2];
    right.v[2] = up.v[0] * forward.v[1] - up.v[1] * forward.v[0];

    __builtin_memcpy(object + 0x370u, &forward, sizeof(forward));
    __builtin_memcpy(object + 0x380u, &right, sizeof(right));
    __builtin_memcpy(object + 0x390u, &up, sizeof(up));

    f32* negative = reinterpret_cast<f32*>(object + 0x3A0u);
    const f32* input = reinterpret_cast<const f32*>(object + 0x80u);
    for (u32 i = 0; i < 4; ++i) negative[i] = -input[i];
    negative = reinterpret_cast<f32*>(object + 0x3B0u);
    input = reinterpret_cast<const f32*>(object + 0x90u);
    for (u32 i = 0; i < 4; ++i) negative[i] = -input[i];
    __builtin_memcpy(object + 0x3C0u, object + 0xA0u, sizeof(Vec4_00144008));
}
