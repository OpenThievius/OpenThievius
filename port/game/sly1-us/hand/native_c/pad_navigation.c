/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only directional repeat handlers, transcribed from the US decomp. */
#include "common.h"

extern f32 D_00261868[];
void func_0016EC20(void* state, s32 mask);

static f32 elapsed_time(void)
{
    return D_00261868[0];
}

static f32 repeat_period(f32 prior)
{
    f32 next = (prior + prior) * 0.33333334f;
    if (next < 0.1f) return 0.1f;
    if (next > 0.33329999446868896f) return 0.33329999446868896f;
    return next;
}

static f32 absolute_value(f32 value)
{
    return value < 0.0f ? -value : value;
}

s32 func_0016EED0(void* opaque)
{
    u8* self = (u8*)opaque;
    s32 direction = *(s32*)(self + 0x2C);
    f32 now = elapsed_time();
    f32 period;
    f32 base;

    if (direction == 0) {
        u16 flags = *(u16*)(self + 0xAA);
        if (flags & 0x1000u) {
            func_0016EC20(self, 0x1000);
            direction = -1;
        } else if (flags & 0x4000u) {
            func_0016EC20(self, 0x4000);
            direction = 1;
        } else if (*(f32*)(self + 0x48) >= 0.8f) {
            f32 vertical = absolute_value(*(f32*)(self + 0x44));
            f32 horizontal = absolute_value(*(f32*)(self + 0x40));
            if (horizontal < vertical)
                direction = *(f32*)(self + 0x44) > 0.0f ? -1 : 1;
        }
        if (direction == 0) return 0;
        period = 0.33329999446868896f;
        *(f32*)(self + 0x3C) = period;
        base = now;
    } else {
        s32 held = 0;
        u16 held_flags = *(u16*)(self + 0xA8);
        if (direction < 0) {
            if (held_flags & 0x1000u) held = 1;
            else if (*(f32*)(self + 0x48) >= 0.25f &&
                     absolute_value(*(f32*)(self + 0x40)) < absolute_value(*(f32*)(self + 0x44)) &&
                     *(f32*)(self + 0x44) > 0.0f) held = 1;
        } else {
            if (held_flags & 0x4000u) held = 1;
            else if (*(f32*)(self + 0x48) >= 0.25f &&
                     absolute_value(*(f32*)(self + 0x40)) < absolute_value(*(f32*)(self + 0x44)) &&
                     *(f32*)(self + 0x44) < 0.0f) held = 1;
        }
        if (!held) {
            *(s32*)(self + 0x2C) = 0;
            return 0;
        }
        if (*(f32*)(self + 0x38) > now) return 0;
        period = repeat_period(*(f32*)(self + 0x3C));
        *(f32*)(self + 0x3C) = period;
        base = now;
    }
    *(s32*)(self + 0x2C) = direction;
    *(f32*)(self + 0x38) = base + period;
    return direction;
}

s32 func_0016EC98(void* opaque)
{
    u8* self = (u8*)opaque;
    s32 direction = *(s32*)(self + 0x28);
    f32 now = elapsed_time();
    f32 period;
    f32 base;

    if (direction == 0) {
        u16 flags = *(u16*)(self + 0xAA);
        if (flags & 0x8000u) {
            func_0016EC20(self, 0x8000);
            direction = -1;
        } else if (flags & 0x2000u) {
            func_0016EC20(self, 0x2000);
            direction = 1;
        } else if (*(f32*)(self + 0x48) >= 0.8f) {
            f32 horizontal = absolute_value(*(f32*)(self + 0x40));
            f32 vertical = absolute_value(*(f32*)(self + 0x44));
            if (vertical < horizontal)
                direction = *(f32*)(self + 0x40) < 0.0f ? -1 : 1;
        }
        if (direction == 0) return 0;
        period = 0.33329999446868896f;
        *(f32*)(self + 0x34) = period;
        base = now;
    } else {
        s32 held = 0;
        u16 held_flags = *(u16*)(self + 0xA8);
        if (direction < 0) {
            if (held_flags & 0x8000u) held = 1;
            else if (*(f32*)(self + 0x48) >= 0.25f &&
                     absolute_value(*(f32*)(self + 0x40)) > absolute_value(*(f32*)(self + 0x44)) &&
                     *(f32*)(self + 0x40) < 0.0f) held = 1;
        } else {
            if (held_flags & 0x2000u) held = 1;
            else if (*(f32*)(self + 0x48) >= 0.25f &&
                     absolute_value(*(f32*)(self + 0x40)) > absolute_value(*(f32*)(self + 0x44)) &&
                     *(f32*)(self + 0x40) > 0.0f) held = 1;
        }
        if (!held) {
            *(s32*)(self + 0x28) = 0;
            return 0;
        }
        if (*(f32*)(self + 0x30) > now) return 0;
        period = repeat_period(*(f32*)(self + 0x34));
        *(f32*)(self + 0x34) = period;
        base = now;
    }
    *(s32*)(self + 0x28) = direction;
    *(f32*)(self + 0x30) = base + period;
    return direction;
}
