// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only C backfill for the Sly camera/viewport setup routine.
#include "sly1/host_abi.h"

extern "C" f32 func_002054F0(f32 angle);
extern "C" void func_00189F58(void* state, f32 x_scale, f32 y_scale,
                                f32 x_offset, f32 y_offset,
                                f32 near_plane, f32 far_plane);
extern "C" void func_001434C8(void* self, f32* out, f32 amount);
extern "C" void func_00144540(void* self);

extern "C" void func_00143560(void* self) {
    u8* bytes = static_cast<u8*>(self);
    const f32 angle = *reinterpret_cast<f32*>(bytes + 0x1C4);
    const f32 half_sine = func_002054F0(angle * 0.5f);
    const f32 scale = *reinterpret_cast<f32*>(bytes + 0x1E0);
    const f32 inverse_angle = 1.0f / angle;
    const f32 square_plus_one = scale * scale + 1.0f;

    *reinterpret_cast<f32*>(bytes + 0x1F4) = half_sine;
    *reinterpret_cast<f32*>(bytes + 0x1F0) = half_sine * scale;
    const f32 basis = *reinterpret_cast<f32*>(bytes + 0x1C0) * inverse_angle;
    *reinterpret_cast<f32*>(bytes + 0x00B0) = basis < 1.0f ? 1.0f : basis;
    *reinterpret_cast<f32*>(bytes + 0x01EC) =
        half_sine * inverse_angle * square_plus_one + 1.0f;

    const f32 projection_x = 640.0f / (*reinterpret_cast<f32*>(bytes + 0x1F0) * 4096.0f);
    const f32 projection_y = 600.0f / (half_sine * 4096.0f);
    func_00189F58(bytes + 0xC0, projection_x, projection_y, 0.0f, 0.0f,
                   *reinterpret_cast<f32*>(bytes + 0x1E4),
                   *reinterpret_cast<f32*>(bytes + 0x1E8));
    func_001434C8(self, reinterpret_cast<f32*>(bytes + 0x208), 1.0f);
    func_00144540(self);
}
