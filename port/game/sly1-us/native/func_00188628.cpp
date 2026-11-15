// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only scalar replacement for the retail VU0 matrix scale helper.
#include "common.h"
#include "vu0.h"

extern "C" s32 func_00188628(void* out, const void* matrix, u128 scale)
{
    f32 factor;
    __builtin_memcpy(&factor, &scale, sizeof(factor));

    const f32* src = (const f32*)matrix;
    f32* dst = (f32*)out;
    for (s32 i = 0; i < 16; ++i) {
        dst[i] = src[i] * factor;
    }
    return 0;
}
