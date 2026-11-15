// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only CPU backfill for the retail VU0 inverse-inertia operation. The
// retail assembly remains in sly1-decomp; replace this file if its matching
// source becomes hostable. This does not change decompilation coverage.
#include "collider.h"

namespace {

Vec4 multiply_rows(const Mat3& matrix, const Vec4& value) {
    return {
        matrix.r[0].x * value.x + matrix.r[1].x * value.y +
            matrix.r[2].x * value.z,
        matrix.r[0].y * value.x + matrix.r[1].y * value.y +
            matrix.r[2].y * value.z,
        matrix.r[0].z * value.x + matrix.r[1].z * value.y +
            matrix.r[2].z * value.z,
        value.w,
    };
}

Vec4 multiply_transposed_rows(const Mat3& matrix, const Vec4& value) {
    return {
        matrix.r[0].x * value.x + matrix.r[0].y * value.y +
            matrix.r[0].z * value.z,
        matrix.r[1].x * value.x + matrix.r[1].y * value.y +
            matrix.r[1].z * value.z,
        matrix.r[2].x * value.x + matrix.r[2].y * value.y +
            matrix.r[2].z * value.z,
        value.w,
    };
}

} // namespace

void func_001B8FE8(Collider* self, Vec4* in, Vec4* out) {
    const Vec4 local = multiply_transposed_rows(self->m110, *in);
    const Vec4 local_response = multiply_rows(self->invInertia, local);
    const Vec4 world_response = multiply_rows(self->m110, local_response);
    *out = world_response;
}
