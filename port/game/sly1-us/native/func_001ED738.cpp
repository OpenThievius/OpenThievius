// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only scalar equivalent of the matched VU0 vector projection.
#include "common.h"
#include "placed_object.h"

f32 func_001ED738(const Vec02* vector, const Vec02* axis,
                  Vec02* along, Vec02* rest)
{
    const f32 x = vector->x;
    const f32 y = vector->y;
    const f32 z = vector->z;
    const f32 w = vector->w;
    const f32 ax = axis->x;
    const f32 ay = axis->y;
    const f32 az = axis->z;
    const f32 dot = x * ax + y * ay + z * az;
    const f32 px = ax * dot;
    const f32 py = ay * dot;
    const f32 pz = az * dot;

    if (along != 0) {
        along->x = px;
        along->y = py;
        along->z = pz;
        along->w = 0.0f;
    }
    if (rest != 0) {
        rest->x = x - px;
        rest->y = y - py;
        rest->z = z - pz;
        rest->w = w;
    }
    return dot;
}
