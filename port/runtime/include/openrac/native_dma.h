/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Native completion for Sly's synchronous EE-to-EE DMA helpers. */
#ifndef OPENRAC_NATIVE_DMA_H
#define OPENRAC_NATIVE_DMA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void openrac_lib_sly1_dma_reset(int unused);
int openrac_lib_sly1_dma_status(uint32_t handle);
uint32_t openrac_lib_sly1_dma_submit(uint32_t descriptors, int count);

#ifdef __cplusplus
}
#endif

#endif
