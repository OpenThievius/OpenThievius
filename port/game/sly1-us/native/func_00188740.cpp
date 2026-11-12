// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native implementation of the retail 4x4 vector transform.
// Retail accumulates the four row products in VU0; this uses host floats and
// preserves the same output-lane calculation without modeling VU state.
using NativeQuad = unsigned int __attribute__((vector_size(16)));

extern "C" NativeQuad func_00188740(const void* matrix, NativeQuad input)
{
    const auto* bytes = static_cast<const unsigned char*>(matrix);
    float rows[4][4];
    float vector[4];
    float result[4];

    __builtin_memcpy(vector, &input, sizeof(vector));
    for (unsigned row = 0; row < 4; ++row)
        __builtin_memcpy(rows[row], bytes + row * 16, sizeof(rows[row]));

    for (unsigned lane = 0; lane < 4; ++lane) {
        result[lane] = rows[0][lane] * vector[0] +
                       rows[1][lane] * vector[1] +
                       rows[2][lane] * vector[2] +
                       rows[3][lane] * vector[3];
    }

    NativeQuad output;
    __builtin_memcpy(&output, result, sizeof(result));
    return output;
}
