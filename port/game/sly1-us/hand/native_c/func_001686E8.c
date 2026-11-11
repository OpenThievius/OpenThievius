// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only synchronous replacement for Sly's VIF1 display-list submission.
#include "common.h"

extern s32 D_002BE468;
void func_001F6C20(s32 unused);
void func_00202A80(s32 dma, void* source);
void sly1_native_trace_font(u32 stage, u32 a0, u32 a1, u32 a2, u32 a3,
                            u32 a4);

void func_001686E8(s32 dma, void* source) {
    static u32 traced;
    if (traced < 16u) {
        sly1_native_trace_font(184u, traced++, (u32)dma, (u32)source, 0, 0);
    }
    /* Retail masks interrupts, acknowledges VIF1 and waits on the DMA
     * completion semaphore. The port submits synchronously, so the host
     * handoff completes the stream before this function returns. */
    D_002BE468 = 1;
    func_001F6C20(0);
    func_00202A80(dma, source);
}
