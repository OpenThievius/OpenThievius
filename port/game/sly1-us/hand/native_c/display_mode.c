/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only replacements for retail display setup and its two kernel wrappers.
 * The mode setup follows the current NTSC-U decompilation; remove these mappings once hostgen
 * consumes those matching sources directly. */
#include "common.h"

s32 sly1_native_display_kernel_call(u32 service, u32 a0, u32 a1, u32 a2);
u32 func_00201BA8(void);
s32 func_001F6D40(s32 value);
s32 func_001F6960(s32 channel, s32 value);

s32 func_001F6910(s32 interlace, s32 mode, s32 field)
{
    return sly1_native_display_kernel_call(0x02u, (u32)interlace,
                                           (u32)mode, (u32)field);
}

s32 func_001F6C70(u64 value)
{
    return sly1_native_display_kernel_call(0x71u, (u32)value, 0, 0);
}

typedef struct Rec_201A18 {
    s16 x;
    s16 y;
    s16 flag;
    s16 ext;
    s32 f8;
    s32 fC;
} Rec_201A18;

void func_00201A18(s16 mode, s16 x, s16 y, s16 flag)
{
    Rec_201A18* config;
    u16 ext;

    switch (mode) {
    case 0:
        config = (Rec_201A18*)func_00201BA8();
        *(volatile u64*)0x12001000u = 0x200u;
        config->x = x;
        ext = (u16)((*(volatile u64*)0x12001000u >> 16) & 0xFFu);
        config->y = y;
        config->ext = ext;
        (void)func_001F6C70(0xFF00u);
        config->flag = (flag != 0);
        if (config->f8 != 0) {
            (void)func_001F6D40(2);
            (void)func_001F6960(2, config->fC);
            config->f8 = 0;
            config->fC = 0;
        }
        (void)func_001F6910(x & 1, (u8)y, flag & 1);
        return;
    case 1:
        *(volatile u64*)0x12001000u = 0x100u;
        return;
    case 5:
        config = (Rec_201A18*)func_00201BA8();
        config->flag = (flag != 0);
        config->x = x;
        config->y = y;
        config->ext = (u16)((*(volatile u64*)0x12001000u >> 16) & 0xFFu);
        (void)func_001F6910(x & 1, (u8)y, flag & 1);
        return;
    default:
        return;
    }
}
