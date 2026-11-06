// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only backfill of the retail DMA/display channel initialization.
#include "common.h"

u32 func_002027A0(u32 index);
void func_002027C8(u32 mode);
void func_002018E8(void);

extern s32 D_00261F14;
extern s32 D_00261F18;
extern s32 D_00261F1C;
extern s32 D_00261F20;
extern s32 D_00261F24;

void func_00152810(void) {
    /* Channel setup is a PS2 device operation; the native runtime owns those
     * services. Keep the retail reset ordering, then publish the channel
     * addresses from Sly's static channel table for the frame workers. */
    func_002027C8(1);
    func_002018E8();
    D_00261F14 = (s32)func_002027A0(0);
    D_00261F18 = (s32)func_002027A0(1);
    D_00261F1C = (s32)func_002027A0(2);
    D_00261F20 = (s32)func_002027A0(8);
    D_00261F24 = (s32)func_002027A0(9);
}
