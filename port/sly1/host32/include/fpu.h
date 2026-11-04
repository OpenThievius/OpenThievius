/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Host-native replacement for the retail EE's inline sqrt.s helper. */
#ifndef SLY1_HOST32_FPU_H
#define SLY1_HOST32_FPU_H
#include "common.h"
struct Fpu {
    static f32 Sqrt(f32 value) { return __builtin_sqrtf(value); }
};
#endif
