// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only scalar equivalent of the EE matrix-by-three-vector helper.
#include "common.h"

extern "C" void func_00188520(f32* out, const f32* matrix, const f32* vectors) {
    for (s32 vector = 0; vector < 3; ++vector) {
        const f32* input = vectors + vector * 4;
        f32* output = out + vector * 4;
        for (s32 row = 0; row < 3; ++row) {
            const f32* basis = matrix + row * 4;
            output[row] = basis[0] * input[0] + basis[1] * input[1] + basis[2] * input[2];
        }
        output[3] = input[3];
    }
}
