/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only scalar versions of retail placed-object transform primitives. */
#include "common.h"

u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);

typedef struct Matrix4 {
    f32 row[4][4];
} Matrix4;

typedef struct TransformWithScale {
    Matrix4 matrix;
    f32 scale_x;
    f32 scale_y;
    f32 scale_z;
    f32 scale_w;
} TransformWithScale;

/* Retail BuildScaledTransformMatrix (func_00188C10). */
void func_00188C10(const f32* position, const f32 rotation[4][4],
                   const f32* scale, f32 output[4][4]) {
    for (u32 row = 0; row < 3; ++row) {
        for (u32 column = 0; column < 3; ++column)
            output[row][column] = rotation[row][column] * scale[row];
        output[row][3] = 0.0f;
    }
    output[3][0] = position[0];
    output[3][1] = position[1];
    output[3][2] = position[2];
    output[3][3] = 1.0f;
}

/* Retail BuildPlacedObjectTransform (func_00125B70). */
void func_00125B70(void* object, const void* source, void* destination) {
    u8* self = (u8*)object;
    TransformWithScale* output = (TransformWithScale*)destination;
    if (source != 0) {
        if (source != destination)
            *(TransformWithScale*)destination = *(const TransformWithScale*)source;
        return;
    }

    func_00188C10((const f32*)(self + 0x140),
                  (const f32 (*)[4])(self + 0x110),
                  (const f32*)0x00248D40u, output->matrix.row);
    output->scale_x = 1.0f;
    output->scale_y = 1.0f;
}

/* Retail BuildMatrixFromRotationAndTranslation (func_00188B08). */
void func_00188B08(const f32* position, const f32 rotation[4][4],
                   f32 output[4][4]) {
    for (u32 row = 0; row < 3; ++row) {
        for (u32 column = 0; column < 4; ++column)
            output[row][column] = rotation[row][column];
        output[row][3] = 0.0f;
    }
    output[3][0] = position[0];
    output[3][1] = position[1];
    output[3][2] = position[2];
    output[3][3] = 1.0f;
}

/* Transpose the rotation basis into VU matrix vectors and transform position. */
void func_00188B48(const f32* position, const f32 rotation[4][4],
                   f32 output[4][4]) {
    for (u32 row = 0; row < 3; ++row) {
        for (u32 column = 0; column < 3; ++column)
            output[row][column] = rotation[column][row];
        output[row][3] = 0.0f;
    }
    for (u32 row = 0; row < 3; ++row) {
        output[3][row] = rotation[row][0] * position[0]
                       + rotation[row][1] * position[1]
                       + rotation[row][2] * position[2];
    }
    output[3][3] = 1.0f;
}

/* Retail four-vector matrix multiply (func_00188668). */
void func_00188668(f32 output[4][4], const f32 matrix[4][4],
                   const f32 input[4][4]) {
    Matrix4 result;
    for (u32 row = 0; row < 4; ++row) {
        for (u32 column = 0; column < 4; ++column) {
            result.row[row][column] = matrix[0][column] * input[row][0]
                                    + matrix[1][column] * input[row][1]
                                    + matrix[2][column] * input[row][2]
                                    + matrix[3][column] * input[row][3];
        }
    }
    *(Matrix4*)output = result;
}

/* Retail sphere/frustum scale calculation (func_001453B0). */
s32 func_001453B0(const u8* camera, const f32 point[4], f32 radius,
                  f32* scale) {
    f32 depth = point[3] * *(const f32*)(camera + 0xB0) + radius;
    f32 distance_squared = depth * depth;
    f32 point_squared = point[0] * point[0]
                      + point[1] * point[1]
                      + point[2] * point[2];
    if (distance_squared < point_squared) return 0;

    f32 far_depth = depth - *(const f32*)(camera + 0xB0) * 400.0f;
    f32 far_squared = far_depth * far_depth;
    if (far_squared < point_squared) {
        *scale = (distance_squared - point_squared)
               / (distance_squared - far_squared);
    } else {
        *scale = 1.0f;
    }
    return 1;
}

/* Retail placed-object submit callback (func_00125BF0). */
void func_00125BF0(void* object, void* camera, void* parent_transform) {
    u8* self = (u8*)object;
    TransformWithScale transform;
    const f32 uniform_scale = *(const f32*)(self + 0x28C);
    const f32 scale_w = *(const f32*)(self + 0x290);
    const f32 scale[4] = { uniform_scale, uniform_scale, scale_w, 0.0f };
    func_00125B70(object, parent_transform, &transform);

    const f32 position[4] = {
        transform.matrix.row[3][0], transform.matrix.row[3][1],
        transform.matrix.row[3][2], transform.matrix.row[3][3]
    };
    func_00188C10(position, (const f32 (*)[4])0x002583D0u, scale,
                  transform.matrix.row);

    u32 context = *(u32*)(self + 0x14);
    u32 dispatcher = context != 0 ? *(u32*)(context + 0x1D2C) : 0;
    u32 vtable = dispatcher != 0 ? *(u32*)dispatcher : 0;
    u32 submit = vtable != 0 ? *(u32*)(vtable + 0xA8) : 0;
    if (submit != 0)
        (void)sly1_native_call_regs4(submit, dispatcher, (u32)camera,
                                     (u32)&transform, 0);
}
