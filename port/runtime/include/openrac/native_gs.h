/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef OPENRAC_NATIVE_GS_H
#define OPENRAC_NATIVE_GS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The native EE device layer handles selected MMIO stores. Returns one when
 * it handled the store; other addresses use the ordinary guest-memory path. */
int openrac_guest_ee_mmio_store32(uint32_t address, uint32_t value);

/* Native GS frontends consume the ordered GIF qword stream from this FIFO. */
size_t openrac_guest_gif_fifo_read(void* destination, size_t capacity);
size_t openrac_guest_gif_fifo_pending(void);

#ifdef __cplusplus
}
#endif

#endif
