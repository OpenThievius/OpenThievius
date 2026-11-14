// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only replacement for the VU0 particle-filter microprogram call site.
// Keep this separate from the matched decomp definition. The decomp documents
// the inputs, lifetime test, outputs and angle update, but the VU microprogram's
// exact arithmetic is still unknown; this uses the sibling effect motion path
// as the closest CPU-side equivalent until that arithmetic is recovered.
#include "common.h"
#include "placed_object.h"
#include "vu0.h"

extern f32 g_gameTime;

namespace {

f32 wrap_angle_port(f32 angle) {
    constexpr f32 pi = 3.1415927f;
    constexpr f32 tau = 6.2831855f;
    while (angle > pi) angle -= tau;
    while (angle < -pi) angle += tau;
    return angle;
}

Vec02 transform_velocity(const f32* matrix, const Vec02& velocity) {
    // TurnMatrixBySpin produces four row vectors. Like MoveEffect, apply its
    // translation term with w=1, then integrate the resulting velocity by dt.
    const f32 x = velocity.x;
    const f32 y = velocity.y;
    const f32 z = velocity.z;
    return {
        matrix[0] * x + matrix[4] * y + matrix[8] * z + matrix[12],
        matrix[1] * x + matrix[5] * y + matrix[9] * z + matrix[13],
        matrix[2] * x + matrix[6] * y + matrix[10] * z + matrix[14],
        velocity.w,
    };
}

} // namespace

s32 func_0013A498(const f32* matrix, u128 scale_bits,
                  Vec02* positions, Vec02* velocities,
                  Vec02* records, s32 count, s32 mode,
                  void* bound_minimum, void* bound_maximum) {
    union ScaleBits {
        u128 bits;
        f32 lanes[4];
    } scale{scale_bits};
    const f32 dt = scale.lanes[0];
    auto* lo = static_cast<Vec02*>(bound_minimum);
    auto* hi = static_cast<Vec02*>(bound_maximum);
    s32 dropped = 0;
    s32 output = 0;

    for (s32 i = 0; i < count; ++i) {
        if (velocities[i].w <= g_gameTime) {
            ++dropped;
            continue;
        }

        const Vec02 velocity = transform_velocity(matrix, velocities[i]);
        const Vec02 old_position = positions[i];
        const Vec02 old_record = records[i];
        velocities[output] = velocity;
        positions[output] = {
            old_position.x + velocity.x * dt,
            old_position.y + velocity.y * dt,
            old_position.z + velocity.z * dt,
            old_position.w,
        };
        records[output] = old_record;
        if (mode == 2) {
            records[output].w = wrap_angle_port(old_record.w + old_record.z * dt);
        }

        if (positions[output].x < lo->x) lo->x = positions[output].x;
        if (positions[output].y < lo->y) lo->y = positions[output].y;
        if (positions[output].z < lo->z) lo->z = positions[output].z;
        if (positions[output].x > hi->x) hi->x = positions[output].x;
        if (positions[output].y > hi->y) hi->y = positions[output].y;
        if (positions[output].z > hi->z) hi->z = positions[output].z;
        ++output;
    }

    return dropped;
}
