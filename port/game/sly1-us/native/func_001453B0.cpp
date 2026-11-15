// SPDX-License-Identifier: AGPL-3.0-or-later
// Direct-host port backfill for the handwritten VU0 camera fade function.
#include "common.h"
#include "vu0.h"

struct Camera;

s32 func_001453B0(Camera* camera, u128 point, f32* fade,
                  f32 radius, f32 unused_distance);

s32 func_001453B0(Camera* camera, u128 point, f32* fade,
                  f32 radius, f32 unused_distance) {
    const f32 range_scale = *(const f32*)((const u8*)camera + 0xB0);
    const f32 depth = Vu0::HostLane(point, 3) * range_scale + radius;
    const f32 depth_squared = depth * depth;
    const f32 x = Vu0::HostLane(point, 0);
    const f32 y = Vu0::HostLane(point, 1);
    const f32 z = Vu0::HostLane(point, 2);
    const f32 distance_squared = x * x + y * y + z * z;
    (void)unused_distance;

    if (depth_squared < distance_squared) {
        return 0;
    }

    const f32 fade_depth = depth - range_scale * 400.0f;
    const f32 fade_depth_squared = fade_depth * fade_depth;
    if (fade_depth_squared < distance_squared) {
        *fade = (depth_squared - distance_squared) /
                (depth_squared - fade_depth_squared);
    } else {
        *fade = 1.0f;
    }
    return 1;
}
