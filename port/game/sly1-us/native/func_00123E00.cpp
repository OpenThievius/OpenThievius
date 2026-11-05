// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only equivalent of the EE squared-length epsilon check.
#include "common.h"

extern "C" s32 func_00123E00(const f32* vector) {
    const f32 length_squared = vector[0] * vector[0]
                             + vector[1] * vector[1]
                             + vector[2] * vector[2];
    return length_squared < 0.0004f ? 0 : 1;
}
