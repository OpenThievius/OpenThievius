/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only color and layout constructors whose decomp units are not host-readable. */
#include "common.h"

int func_00136FE8(s32 initialize, s32 mode)
{
    static const u32 colors[10] = {
        0x80000000u, 0x807E5200u, 0x806E1E19u, 0x800A287Eu, 0x80282850u,
        0x8000F0F0u, 0x80002828u, 0x80FFFFFFu, 0x80E6E6E6u, 0x803E722Fu
    };
    u32 i;

    if (initialize == 0 || mode != -1) return 0;
    for (i = 0; i < 10; ++i)
        *(u32*)(0x002610D0u + i * 8u) = colors[i];

    {
        u8* style = (u8*)0x00261148u;
        u32 j;
        for (j = 0; j < 7; ++j) *(u32*)(style + j * 4u) = 0;
        *(u8*)(style + 0x04) = 45;
        *(f32*)(style + 0x08) = 2.0f;
        *(f32*)(style + 0x10) = 0.3f;
        *(f32*)(style + 0x14) = 0.3f;
        *(u32*)(style + 0x18) = 0xFF7D4B00u;
    }
    {
        u8* style = (u8*)0x00261168u;
        u32 j;
        for (j = 0; j < 7; ++j) *(u32*)(style + j * 4u) = 0;
        *(u8*)(style + 0x04) = 45;
        *(f32*)(style + 0x08) = 32.0f;
        *(f32*)(style + 0x0C) = 16.0f;
        *(f32*)(style + 0x10) = 0.3f;
        *(f32*)(style + 0x14) = 0.3f;
        *(u32*)(style + 0x18) = 0xFF7D4B00u;
    }
    return 0;
}

int func_00137270(void)
{
    return func_00136FE8(1, -1);
}

int func_001460B8(s32 initialize, s32 mode)
{
    if (initialize != 0 && mode == -1)
        *(u32*)0x002618B0u = 0x80303030u;
    return 0;
}

int func_001460E0(void)
{
    return func_001460B8(1, -1);
}
