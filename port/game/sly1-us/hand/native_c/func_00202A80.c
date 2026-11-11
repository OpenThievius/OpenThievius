// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only synchronous replacement for Sly's EE DMA send routine.
#include "common.h"

int openrac_guest_vif1_mmio_store32(u32 address, u32 value);
void openrac_native_renderer_submit_dma_chain(u32 source);
void openrac_native_renderer_trace_dma_call(u32 channel, u32 source);

void func_00202A80(s32 dma, void* source) {
    const u32 channel = (u32)dma;
    const u32 address = (u32)source;
    openrac_native_renderer_trace_dma_call(channel, address);

    /* Sly's channel 2 writer emits GIF packets (including IMAGE tags), not a
     * VIF stream. Hand that DMA chain to the native GIF renderer directly. */
    if (channel == 0x1000A000u) {
        *(u32*)0x1000E000u |= 1u;
        *(u32*)0x1000A030u = address;
        *(u32*)0x1000A020u = (*(u32*)0x1000A020u & ~0xDu) | 0x105u;
        openrac_native_renderer_submit_dma_chain(address);
        *(u32*)0x1000A020u &= ~0x100u;
        return;
    }

    /* Other VIF1 submissions must pass through VIF/VU translation first. */
    if (channel != 0x10009000u) return;
    *(u32*)0x1000E000u |= 1u;
    *(u32*)0x10009030u = address;
    /* The game sets TTE on this channel before submission. Preserve it so
     * the VIF1 handler receives the VIF code stored in each DMA tag. */
    const u32 chcr = (*(u32*)0x10009000u & 0x40u) | 0x105u;
    (void)openrac_guest_vif1_mmio_store32(0x10009000u, chcr);
}
