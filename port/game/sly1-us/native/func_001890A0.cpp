// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only scalar replacement for the retail VU0 matrix orthonormalizer.
#include "sly1/host_abi.h"

struct SlyMatrixRows {
    f32 row[3][4];
};

static void normalize_vec3(f32* v) {
    const f32 inverse = 1.0f / __builtin_sqrtf(
        v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    v[0] *= inverse;
    v[1] *= inverse;
    v[2] *= inverse;
    v[3] *= inverse;
}

static void cross_xyz(const f32* a, const f32* b, f32* out) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
    out[3] = 0.0f;
}

extern "C" void func_001890A0(SlyMatrixRows* matrix) {
    normalize_vec3(matrix->row[0]);
    cross_xyz(matrix->row[0], matrix->row[1], matrix->row[2]);
    normalize_vec3(matrix->row[2]);
    cross_xyz(matrix->row[2], matrix->row[0], matrix->row[1]);
    normalize_vec3(matrix->row[1]);
}
