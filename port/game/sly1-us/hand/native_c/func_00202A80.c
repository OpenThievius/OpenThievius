// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only synchronous replacement for Sly's EE DMA send routine.
#include "common.h"

int openrac_guest_vif1_mmio_store32(u32 address, u32 value);

void func_00202A80(s32 dma, void* source) {
    /* Let the native VIF1 path expand UNPACK and VU1 work before forwarding
     * DIRECT GIF packets to the GS renderer. */
    *(u32*)0x1000E000u |= 1u;
    *(u32*)0x10009030u = (u32)source;
    (void)dma;
    (void)openrac_guest_vif1_mmio_store32(0x10009000u, 0x105u);
}
