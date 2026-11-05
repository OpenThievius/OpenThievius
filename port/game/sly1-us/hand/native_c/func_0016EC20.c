/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only guest input-flag acknowledgement, matching the retail bit clear. */
#include "common.h"

void func_0016EC20(void* state, s32 mask) {
    u16* flags = (u16*)((u8*)state + 0xAA);
    *flags &= (u16)~mask;
}
