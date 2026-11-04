/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef OPENRAC_NATIVE_WINDOW_H
#define OPENRAC_NATIVE_WINDOW_H

#ifdef __cplusplus
extern "C" {
#endif

int openrac_native_window_create(void);
int openrac_native_window_poll_quit(void);
void openrac_native_window_present(void);
void openrac_native_window_destroy(void);

#ifdef __cplusplus
}
#endif

#endif
