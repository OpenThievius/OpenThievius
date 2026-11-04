/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Native thread-local replacement for Sly's nested temporary packet arena. */
#include "common.h"

void sly1_native_temp_begin(void);
u32 sly1_native_temp_alloc(s32 size);
void sly1_native_temp_end(void);

void func_0018D608(void) {
    sly1_native_temp_begin();
}

void* func_0018D658(s32 size) {
    return (void*)sly1_native_temp_alloc(size);
}

void func_0018D6F0(void) {
    sly1_native_temp_end();
}
