/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only Sly frame clock, using the native monotonic counter service. */
#include "common.h"

extern u8 D_00261858[];
extern f32 D_00261850;
extern f32 D_00261854;
u64 sly1_native_monotonic_ticks(void);

void func_00142F10(void* clock) {
    u8* state = (u8*)clock;
    s64 now = (s64)sly1_native_monotonic_ticks();
    s64 delta = now - *(s64*)(state + 0x18);
    f32 ticks = (f32)delta;
    f32 dt = ticks * 0.0000000033908420f;
    const f32 minimum = 0.016666668f;
    const f32 maximum = 0.033333335f;
    if (dt < minimum) dt = minimum;
    else if (dt > maximum) dt = maximum;

    *(f32*)(state + 0x14) = dt;
    f32 scaled = *(s32*)(state + 0x00) != 0 ? dt : 0.0f;
    scaled *= D_00261854 * D_00261850;
    if (scaled >= minimum) *(f32*)(state + 0x14) = scaled;

    f32 previous_dt = *(f32*)(state + 0x08);
    *(f32*)(state + 0x04) += scaled;
    *(f32*)(state + 0x0c) = previous_dt;
    *(f32*)(state + 0x08) = scaled;
    *(f32*)(state + 0x10) += *(f32*)(state + 0x14);
    *(s64*)(state + 0x18) = now;
}
