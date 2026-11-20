/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef OPENRAC_NATIVE_VU1_H
#define OPENRAC_NATIVE_VU1_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Translate one uploaded VU1 display-list program into native host work. */
/* stopped_pipeline: 0 upper/control, 1 lower, 2 device command, 3 cycle limit. */
int openrac_guest_vu1_execute(uint32_t entry, uint32_t* unsupported_word,
                              uint32_t* stopped_pc, uint32_t* stopped_pipeline,
                              uint32_t* paired_word);
void openrac_guest_vu1_set_itop(uint32_t value);
void openrac_guest_vu1_set_trace_context(uint32_t submission_id,
                                         uint32_t vif_command_offset);
void openrac_guest_vu1_set_trace_target_submission(uint32_t submission_id,
                                                  uint32_t stream_begin,
                                                  uint32_t stream_end);
int openrac_guest_vu1_trace_target_context_matches(void);

#ifdef __cplusplus
}
#endif

#endif
