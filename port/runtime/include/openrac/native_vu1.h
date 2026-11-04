/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef OPENRAC_NATIVE_VU1_H
#define OPENRAC_NATIVE_VU1_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Translate one uploaded VU1 display-list program into native host work. */
int openrac_guest_vu1_execute(uint32_t entry, uint32_t* unsupported_word,
                              uint32_t* stopped_pc, uint32_t* stopped_pipeline,
                              uint32_t* paired_word);
void openrac_guest_vu1_set_itop(uint32_t value);

#ifdef __cplusplus
}
#endif

#endif
