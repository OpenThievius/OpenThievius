// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only scalar equivalent of the spin-acceleration magnitude test.
#include "common.h"
#include "placed_object.h"

s32 func_00123E88(Vec02* acceleration)
{
    const f32 squared_length = acceleration->x * acceleration->x +
                               acceleration->y * acceleration->y +
                               acceleration->z * acceleration->z;
    return squared_length < 0.0025f;
}
