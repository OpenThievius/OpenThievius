// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only equivalent of the retail relative floating-point comparison.
#include "sly1/host_abi.h"

extern "C" s32 func_001EAC68(f32 a, f32 b, f32 epsilon) {
    const f32 difference = a - b;
    const f32 magnitude = a < 0.0f ? -a : a;
    const f32 denominator = magnitude > 1.0f ? magnitude : 1.0f;
    const f32 normalized = (difference < 0.0f ? -difference : difference) / denominator;
    return normalized < epsilon;
}
