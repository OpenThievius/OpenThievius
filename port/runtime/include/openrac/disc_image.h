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

/* Sly startup's native replacements for its IOP boot and CD status calls. */
int openrac_lib_sly1_iop_boot(void);
int openrac_lib_sly1_iop_initialize(int mode);
int openrac_lib_sly1_cd_status(void);
int openrac_lib_sly1_cd_disk_ready(int mode);
int openrac_lib_sly1_cd_read_sectors(uint32_t sector, uint32_t count, uint32_t destination,
                                    uint32_t mode);
int openrac_lib_sly1_cd_read_wait(int mode);
int openrac_lib_sly1_cd_read_error(void);
int openrac_lib_sly1_cd_file_open(uint32_t guest_path);
int openrac_lib_sly1_cd_file_ready(void);
int openrac_lib_sly1_cd_file_close(void);

/* Locate an ISO 9660 path and report its first logical sector and byte size. */
int openrac_guest_disc_file_info(const char* path, uint32_t* first_sector, uint32_t* byte_size);

/* Read a byte range from an ISO 9660 file into guest RAM. */
int openrac_guest_disc_read_file(const char* path, uint32_t file_offset, uint32_t byte_count,
                                 uint32_t guest_address);

#ifdef __cplusplus
}
#endif

#endif
