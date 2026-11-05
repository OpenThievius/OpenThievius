// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only conservative body-bounds update. The retail method uses VU0 macro
// instructions to combine every part's transformed mesh bounds; this native
// version uses each part's cached centre and collision radius until that VU0
// routine is decompiled. Keep the approximation isolated from matched code.
#include "common.h"

extern "C" void func_001B8540(void* object) {
    auto* self = static_cast<u8*>(object);
    auto* part = *reinterpret_cast<u8**>(self + 0x2D8);
    f32 minimum[3] = {3.402823466e+38f, 3.402823466e+38f, 3.402823466e+38f};
    f32 maximum[3] = {-3.402823466e+38f, -3.402823466e+38f, -3.402823466e+38f};
    f32 enclosing_radius = 0.0f;
    s32 part_count = 0;

    while (part != nullptr && part_count++ < 256) {
        const f32 radius = *(f32*)(part + 0x3CC);
        const f32* centre = reinterpret_cast<const f32*>(part + 0x140);
        f32 distance_squared = 0.0f;
        for (s32 axis = 0; axis < 3; ++axis) {
            const f32 position = centre[axis];
            const f32 extent = radius < 0.0f ? 0.0f : radius;
            const f32 lo = position - extent;
            const f32 hi = position + extent;
            if (lo < minimum[axis]) minimum[axis] = lo;
            if (hi > maximum[axis]) maximum[axis] = hi;
            distance_squared += position * position;
        }
        const f32 distance = __builtin_sqrtf(distance_squared);
        const f32 candidate = distance + (radius < 0.0f ? 0.0f : radius);
        if (candidate > enclosing_radius) enclosing_radius = candidate;
        part = *reinterpret_cast<u8**>(part + 0x2E4);
    }

    if (part_count == 0) {
        minimum[0] = minimum[1] = minimum[2] = 0.0f;
        maximum[0] = maximum[1] = maximum[2] = 0.0f;
    }
    for (s32 axis = 0; axis < 3; ++axis) {
        *(f32*)(self + 0x420 + axis * 4) = minimum[axis];
        *(f32*)(self + 0x430 + axis * 4) = maximum[axis];
    }
    *(f32*)(self + 0x3D0) = enclosing_radius;
    *(f32*)(self + 0x3D4) = enclosing_radius;
}
