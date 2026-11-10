// SPDX-License-Identifier: AGPL-3.0-or-later
// Temporary native boundary for the retail camera-context setup routine.
// Its matrix/projection work has not yet been ported to the SDL renderer.
#include "sly1/host_abi.h"

extern "C" void func_00144BE8(void* context, s32 mode, void* source,
                               s32 source_mode, s32 flags,
                               f32 a, f32 b, f32 c, f32 d, f32 e, f32 f) {
    (void)context;
    (void)mode;
    (void)source;
    (void)source_mode;
    (void)flags;
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    (void)e;
    (void)f;
}
