/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port ABI view of the retail class 0x51 camera object used by host backfills. */
#ifndef SLY1_PORT_C51_H
#define SLY1_PORT_C51_H

#include "common.h"
#include "shapes.h"

struct C51 {
    u8 pad000[0x2D0];
    s32 w2d0;
    u8 pad2D4[0x0C];
    u32 f2E0[4];
    f32 f2F0;
    f32 f2F4;
    f32 f2f8;
    f32 f2fc;
    f32 f300;
    s32 value;
    u8 pad308[0x18];
    Quad16 q320;
};

#endif
