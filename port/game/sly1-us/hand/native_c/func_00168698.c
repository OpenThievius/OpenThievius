/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only GS signal callback. The native DMA adapter dispatches this after
 * its synchronous GIF-chain completion so retail's waiting display worker can
 * receive the same semaphore signal as the console interrupt path. */
#include "common.h"

extern s32 D_002BE464;
extern s32 D_002BE468;
s32 func_001F6B50(s32 semaphore);

void func_00168698(void)
{
    if (D_002BE468 == 0) return;
    if ((*(volatile u64*)0x12001000u & 2u) == 0) return;

    D_002BE468 = 0;
    (void)func_001F6B50(D_002BE464);
}
