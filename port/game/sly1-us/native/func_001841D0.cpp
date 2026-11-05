// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only first camera mode derived from the US retail routine's scalar and
// VIF packet path. The decomp project keeps the handwritten VU0 function open.
#include "common.h"
#include "c51.h"
#include "v24a0.h"

void func_001841B0(C51* self);
V24A0* func_001EE318(V24A0* writer);
void func_001529C8(V24A0* writer, s32 count, u8* base);
void func_001EE668(V24A0* writer, s32 format, s32 count, const void* source,
                   s32 tag);
void func_001B4580(void* source, f32* output);
void func_001280C0(C51* self, s32 index, void* source, void* output);
void func_001EE3F0(V24A0* writer, s32 size);
void func_001EE480(V24A0* writer);
void func_001529E0(V24A0* writer, u32* out_count, u8** out_begin);

void func_001840F0(f32* out0, f32* out1, f32 a, f32 b, f32 c, f32 d);

void func_001841D0(C51* self) {
    const f32 threshold = 0.0087266471f;
    const f32 f2e8 = *(const f32*)((const u8*)self + 0x2E8);
    s32 viewport_mode = 0;
    if (self->f2f8 > threshold && f2e8 > threshold) {
        viewport_mode = 2;
    } else if (self->f2fc > threshold && self->f2F0 > threshold) {
        viewport_mode = 1;
    }
    *(s32*)((u8*)self + 0x2D4) = viewport_mode;

    // The startup camera uses this path. The other w2d0 branches consume
    // larger VU0-built display lists and remain port work in their own right.
    if (self->w2d0 != 0 || viewport_mode != 0) return;

    V24A0 packet;
    func_001EE318(&packet);
    func_001841B0(self);
    func_001529C8(&packet, 32, (u8*)self + 0x420);

    f32 normalized_color[4] = {};
    if (f2e8 > threshold) {
        func_001B4580((u8*)self + 0x2E0, normalized_color);
        for (s32 i = 0; i < 4; ++i) normalized_color[i] /= f2e8;
    }
    func_001EE668(&packet, 12, 1, normalized_color, 0x16);

    f32 coefficients[8] = {};
    func_001840F0(&coefficients[6], &coefficients[2], self->f2f8, 0.0f,
                  0.0f, f2e8);
    func_001840F0(&coefficients[5], &coefficients[1], self->f2fc, 0.0f,
                  0.0f, self->f2F0);
    func_001840F0(&coefficients[4], &coefficients[0], 180.0f - self->f2f8,
                  0.0f, 0.0f, self->f2F4);
    func_001EE668(&packet, 12, 2, (const u32*)coefficients, 0x17);

    u32 viewport[4] = {};
    func_001280C0(self, 0, &self->q320, viewport);
    func_001EE668(&packet, 8, 1, viewport, 0x19);
    func_001EE3F0(&packet, 0xBF0);

    func_001EE480(&packet);
    func_001529E0(&packet, (u32*)((u8*)self + 0x418), 0);
}
