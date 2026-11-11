/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only backfill of the retail front-heap reset routine. */
#include "common.h"

void func_0018D550(void) {
    /* ResetFrontMemory restores the transient allocation cursor to the mark
     * last set by ReservePermanentMemory. Keep this separate from decomp C so
     * the upstream implementation can replace it when matched. */
    *(u32*)0x002C2A1Cu = *(u32*)0x002C2A18u;
}
