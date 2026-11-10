// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only matrix-to-axis-angle conversion for the native host build.
#include "common.h"

extern "C" f32 func_00205578(f32 value);

extern "C" void func_00188D28(const f32* matrix, f32* angle, f32* axis) {
    const f32 m00 = matrix[0];
    const f32 m11 = matrix[5];
    const f32 m22 = matrix[10];
    f32 cosine = (m00 + m11 + m22 - 1.0f) * 0.5f;
    if (cosine > 1.0f) cosine = 1.0f;
    if (cosine < -1.0f) cosine = -1.0f;

    *angle = func_00205578(cosine);
    if (*angle < 0.0001f) {
        axis[0] = 0.0f;
        axis[1] = 0.0f;
        axis[2] = 0.0f;
        return;
    }

    const f32 x = matrix[9] - matrix[6];
    const f32 y = matrix[2] - matrix[8];
    const f32 z = matrix[4] - matrix[1];
    const f32 length = __builtin_sqrtf(x * x + y * y + z * z);
    if (length > 0.0001f) {
        const f32 scale = 1.0f / length;
        axis[0] = x * scale;
        axis[1] = y * scale;
        axis[2] = z * scale;
        return;
    }

    // At a half turn the skew terms vanish. Recover a stable axis from the
    // diagonal and choose signs from the symmetric off-diagonal terms.
    axis[0] = __builtin_sqrtf((m00 + 1.0f) * 0.5f);
    axis[1] = __builtin_sqrtf((m11 + 1.0f) * 0.5f);
    axis[2] = __builtin_sqrtf((m22 + 1.0f) * 0.5f);
    if (matrix[1] < 0.0f) axis[1] = -axis[1];
    if (matrix[2] < 0.0f) axis[2] = -axis[2];
}
