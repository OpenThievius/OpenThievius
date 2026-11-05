/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only level-clock update matching the NTSC-U decomp body. */
#include "common.h"

extern u8* D_002623C0;
extern u8* D_002623C4;
extern u8* D_002623C8;

void func_001607D0(f32 delta) {
    *(f32*)(D_002623C0 + 0x0c) += delta;
    *(f32*)(D_002623C4 + 0x444) += delta;
    *(f32*)(D_002623C8 + 0x04) += delta;
}
