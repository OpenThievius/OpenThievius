/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only HSV-to-RGB backfill for the native renderer. */
#include "sly1/host_abi.h"

struct Quad_001B4580 { f32 v[4]; };

static f32 clamp_unit(f32 value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

extern "C" void func_001B4330(Quad_001B4580* source, f32* destination)
    __asm__("_Z13func_001B4330P13Quad_001B4580");

extern "C" void func_001B4330(Quad_001B4580* source, f32* destination) {
    f32 hue = source->v[0];
    const f32 saturation = clamp_unit(source->v[1]);
    const f32 value = clamp_unit(source->v[2]);
    f32 red = value;
    f32 green = value;
    f32 blue = value;

    if (saturation != 0.0f) {
        if (hue < 0.0f) hue = -1.0f;
        if (hue > 360.0f) hue = 360.0f;
        if (hue == 360.0f) hue = 0.0f;

        const f32 sector_position = hue / 60.0f;
        const s32 sector = static_cast<s32>(sector_position);
        const f32 fraction = sector_position - static_cast<f32>(sector);
        const f32 p = value * (1.0f - saturation);
        const f32 q = value * (1.0f - saturation * fraction);
        const f32 t = value * (1.0f - saturation * (1.0f - fraction));

        switch (sector) {
        case 0: red = value; green = t; blue = p; break;
        case 1: red = q; green = value; blue = p; break;
        case 2: red = p; green = value; blue = t; break;
        case 3: red = p; green = q; blue = value; break;
        case 4: red = t; green = p; blue = value; break;
        default: red = value; green = p; blue = q; break;
        }
    }

    destination[0] = red;
    destination[1] = green;
    destination[2] = blue;
    destination[3] = value;
}

extern "C" void func_001B4580(Quad_001B4580* source, f32* destination)
    __asm__("_Z13func_001B4580P13Quad_001B4580Pf");

extern "C" void func_001B4580(Quad_001B4580* source, f32* destination) {
    Quad_001B4580 converted = *source;
    converted.v[1] *= 0.003921569f;
    converted.v[2] *= 0.003921569f;
    func_001B4330(&converted, destination);
    destination[0] *= 255.0f;
    destination[1] *= 255.0f;
    destination[2] *= 255.0f;
}
