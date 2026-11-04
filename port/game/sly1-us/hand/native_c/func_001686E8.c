// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only synchronous replacement for Sly's VIF1 display-list submission.
#include "common.h"

extern s32 D_002BE468;
void func_001F6C20(s32 unused);
void func_00202A80(s32 dma, void* source);

void func_001686E8(s32 dma, void* source) {
    /* Retail masks interrupts, acknowledges VIF1 and waits on the DMA
     * completion semaphore. The port submits synchronously, so the host
     * handoff completes the stream before this function returns. */
    D_002BE468 = 1;
    func_001F6C20(0);
    func_00202A80(dma, source);
}
