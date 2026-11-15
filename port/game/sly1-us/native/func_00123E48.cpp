// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only scalar equivalent of the assembly acceleration threshold check.
// The retail function remains unchanged in sly1-decomp.
#include "common.h"

extern "C" s32 func_00123E48(const f32* vector)
{
    const f32 length_squared = vector[0] * vector[0]
                             + vector[1] * vector[1]
                             + vector[2] * vector[2];
    return length_squared < 25.0f ? 1 : 0;
}
