/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only synchronous completion for the native GIF/DMA submission path. */
#include "common.h"

extern u32 D_002BE46C;

void func_001683B8(void) {
    /* Retail waits on the GIF worker's semaphore here. Native submissions are
     * completed synchronously by the host renderer, so release the same gate
     * without waiting for a PS2 interrupt thread. */
    D_002BE46C = 1;
}
