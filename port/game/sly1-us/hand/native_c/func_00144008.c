/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only scalar translation of the retail VU0 camera basis update. */
#include "common.h"

extern f32 D_002472E0[4];
extern f32 D_00248D00[4];

void func_00144008(void* self) {
    u8* object = (u8*)self;
    f32 x = -*(f32*)(object + 0x80);
    f32 y = -*(f32*)(object + 0x84);
    f32 length = __builtin_sqrtf(x * x + y * y);
    f32 forward[4];
    f32 right[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    s32 i;

    if (length < 0.0001f) {
        __builtin_memcpy(forward, D_00248D00, sizeof(forward));
    } else {
        f32 scale = 1.0f / length;
        forward[0] = x * scale;
        forward[1] = y * scale;
        forward[2] = 0.0f;
        forward[3] = 0.0f;
    }

    right[0] = D_002472E0[1] * forward[2] - D_002472E0[2] * forward[1];
    right[1] = D_002472E0[2] * forward[0] - D_002472E0[0] * forward[2];
    right[2] = D_002472E0[0] * forward[1] - D_002472E0[1] * forward[0];
    __builtin_memcpy(object + 0x370, forward, sizeof(forward));
    __builtin_memcpy(object + 0x380, right, sizeof(right));
    __builtin_memcpy(object + 0x390, D_002472E0, sizeof(D_002472E0));

    for (i = 0; i < 4; ++i) {
        *(f32*)(object + 0x3A0 + i * 4) = -*(f32*)(object + 0x80 + i * 4);
        *(f32*)(object + 0x3B0 + i * 4) = -*(f32*)(object + 0x90 + i * 4);
    }
    __builtin_memcpy(object + 0x3C0, object + 0xA0, sizeof(forward));
}
