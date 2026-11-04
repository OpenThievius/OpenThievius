/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only byte copy for the retail aligned/unaligned copy routine. */
#include "common.h"

void func_0018D800(u8* destination, u8* source, s32 byte_count) {
    s32 index;

    for (index = 0; index < byte_count; ++index) {
        destination[index] = source[index];
    }
}
