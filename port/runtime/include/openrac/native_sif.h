/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef OPENRAC_SIF_H
#define OPENRAC_SIF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int openrac_lib_sly1_sif_send_cmd(uint32_t command, uint32_t packet, int packet_size,
                                  uint32_t source, uint32_t destination, int data_size);

#ifdef __cplusplus
}
#endif

#endif /* OPENRAC_SIF_H */
