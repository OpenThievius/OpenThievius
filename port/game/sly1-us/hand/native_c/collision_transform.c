/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only replacements for collision transform setup and inversion. */
#include "common.h"

typedef struct Matrix3x4 {
    f32 value[3][4];
} Matrix3x4;

static f32 absolute_value(f32 value) {
    return value < 0.0f ? -value : value;
}

s32 func_001897F0(const Matrix3x4* input, Matrix3x4* output) {
    Matrix3x4 working = *input;
    const Matrix3x4* identity = (const Matrix3x4*)0x002483D0u;
    s32 ok = 0;

    *output = *identity;
    for (s32 row = 0; row < 3; ++row) {
        s32 pivot = row;
        f32 best = absolute_value(working.value[row][pivot]);
        for (s32 column = row + 1; column < 3; ++column) {
            f32 candidate = absolute_value(working.value[row][column]);
            if (best < candidate) {
                best = candidate;
                pivot = column;
            }
        }
        if (best == 0.0f) goto done;

        if (pivot != row) {
            for (s32 k = 0; k < 3; ++k) {
                f32 temp = working.value[k][pivot];
                working.value[k][pivot] = working.value[k][row];
                working.value[k][row] = temp;
                temp = output->value[k][pivot];
                output->value[k][pivot] = output->value[k][row];
                output->value[k][row] = temp;
            }
        }
        for (s32 column = row + 1; column < 3; ++column) {
            f32 factor = working.value[row][column] / working.value[row][row];
            for (s32 k = 0; k < 3; ++k) {
                working.value[k][column] -= factor * working.value[k][row];
                output->value[k][column] -= factor * output->value[k][row];
            }
        }
    }

    for (s32 row = 2; row >= 0; --row) {
        for (s32 column = 0; column < row; ++column) {
            f32 factor = working.value[row][column] / working.value[row][row];
            for (s32 k = 0; k < 3; ++k) {
                working.value[k][column] -= factor * working.value[k][row];
                output->value[k][column] -= factor * output->value[k][row];
            }
        }
    }

    for (s32 row = 0; row < 3; ++row) {
        f32 diagonal = working.value[row][row];
        for (s32 column = 0; column < 3; ++column) {
            output->value[column][row] /= diagonal;
        }
    }
    ok = 1;

done:
    return ok;
}

void func_001B9448(void* object, f32 scale) {
    u8* bytes = (u8*)object;
    f32* matrix = (f32*)(bytes + 0x2F0);
    Matrix3x4* inverse = (Matrix3x4*)(bytes + 0x320);

    for (u32 i = 0; i < 12; ++i) matrix[i] *= scale;
    if (!func_001897F0((const Matrix3x4*)matrix, inverse)) {
        for (u32 i = 0; i < 12; ++i) ((f32*)inverse)[i] = 0.0f;
    }
}
