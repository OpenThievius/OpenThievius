/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Read logical 2048-byte sectors from a player's Sly 1 disc image. */
#ifndef OPENRAC_DISC_IMAGE_H
#define OPENRAC_DISC_IMAGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opens a cooked ISO or a raw 2352-byte BIN containing an ISO 9660 volume. */
int openrac_guest_disc_open(const char* path);

/* Copies `count` logical sectors into guest RAM at `guest_address`. */
int openrac_guest_disc_read(uint32_t first_sector, uint32_t count, uint32_t guest_address);
uint32_t openrac_guest_disc_sector_count(void);

#ifdef __cplusplus
}
#endif

#endif
