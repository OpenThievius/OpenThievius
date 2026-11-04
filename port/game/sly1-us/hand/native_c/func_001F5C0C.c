/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only byte fill based on NTSC-U retail assembly at 0x001F5C0C.
 * Keep this separate from the matching decompilation. */
#include "common.h"

u8* func_001F5C0C(u8* destination, int value, int byte_count) {
    u8* bytes = destination;
    u32 remaining = (u32)byte_count;
    const u8 fill = (u8)value;

    while (remaining != 0) {
        *bytes++ = fill;
        --remaining;
    }
    return destination;
}

/* Some source units declare this retail function as void. Hostgen keeps that
 * spelling as a separate thunk, so route it through the same implementation. */
void func_001F5C0C__openrac_decl_109(u8* destination, int value, int byte_count) {
    (void)func_001F5C0C(destination, value, byte_count);
}
