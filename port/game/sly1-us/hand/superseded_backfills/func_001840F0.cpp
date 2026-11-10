// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only projection helper with the retail argument order. The current
// decompilation candidate shifts the output pointers behind the float args.
#include "common.h"

f32 func_00205210(f32 value);
void func_00183F40(f32* out0, f32* out1, f32 a, f32 b, f32 c, f32 d);

void func_001840F0(f32* out0, f32* out1, f32 a, f32 b, f32 c, f32 d) {
    const f32 radians = 0.0087266471f;
    const f32 reciprocal = 0.0039215689f;

    f32 x = func_00205210(a * radians);
    x = x + x * x * x;

    f32 y = func_00205210(c * radians);
    y = y + y * y * y;

    func_00183F40(out0, out1, x, b * reciprocal, y, d * reciprocal);
}
