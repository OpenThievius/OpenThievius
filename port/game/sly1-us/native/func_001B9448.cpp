// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only scalar replacement for the retail VU0 inertia inverse. It keeps
// the game's padded 3x4 row layout and clears the inverse when singular.
#include "common.h"

extern "C" void func_001B9448(void* object, f32 scale) {
    auto* bytes = static_cast<u8*>(object);
    const f32* input = reinterpret_cast<const f32*>(bytes + 0x2F0);
    f32* output = reinterpret_cast<f32*>(bytes + 0x320);

    const f32 a = input[0], b = input[1], c = input[2];
    const f32 d = input[4], e = input[5], f = input[6];
    const f32 g = input[8], h = input[9], i = input[10];
    const f32 A = e * i - f * h;
    const f32 B = c * h - b * i;
    const f32 C = b * f - c * e;
    const f32 determinant = a * A + d * B + g * C;

    for (s32 element = 0; element < 12; ++element) output[element] = 0.0f;
    if (!(determinant > 1.0e-20f || determinant < -1.0e-20f)) return;

    const f32 factor = scale / determinant;
    output[0] = A * factor;
    output[1] = (c * h - b * i) * factor;
    output[2] = (b * f - c * e) * factor;
    output[4] = (f * g - d * i) * factor;
    output[5] = (a * i - c * g) * factor;
    output[6] = (c * d - a * f) * factor;
    output[8] = (d * h - e * g) * factor;
    output[9] = (b * g - a * h) * factor;
    output[10] = (a * e - b * d) * factor;
}
