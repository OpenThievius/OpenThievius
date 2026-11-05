// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only scalar equivalent of Sly's three-vector EE transform.
#include "common.h"

extern "C" void func_00189150(const f32* a, const f32* b, f32* out) {
    // Each input holds three packed four-lane vectors. The EE routine forms
    // one output vector for each component of a, weighted by the vectors in b.
    for (s32 component = 0; component < 3; ++component) {
        f32* dst = out + component * 4;
        const f32* a0 = a;
        const f32* a1 = a + 4;
        const f32* a2 = a + 8;
        const f32* b0 = b;
        const f32* b1 = b + 4;
        const f32* b2 = b + 8;
        const f32 x = a0[component];
        const f32 y = a1[component];
        const f32 z = a2[component];
        for (s32 lane = 0; lane < 4; ++lane) {
            dst[lane] = b0[lane] * x + b1[lane] * y + b2[lane] * z;
        }
    }
}
