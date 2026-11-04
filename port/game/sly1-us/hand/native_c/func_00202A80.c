// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only synchronous replacement for Sly's EE DMA send routine.
#include "common.h"

void openrac_native_renderer_submit_dma_chain(u32 source);

void func_00202A80(s32 dma, void* source) {
    /* The retail routine records the chain address and marks the channel
     * active before sending it. The native path consumes the chain inline,
     * so the active bit is cleared before returning to its caller. */
    u32* control = (u32*)((u8*)dma + 0x20);
    *(u32*)((u8*)dma + 0x30) = (u32)source;
    *control = (*control & ~0xdu) | 0x105u;
    openrac_native_renderer_submit_dma_chain((u32)source);
    *control &= ~0x100u;
}
