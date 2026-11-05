/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only replacement for a small UI state reset still unmatched upstream. */
#include "common.h"

void func_0016EC88(void* object)
{
    u8* state = (u8*)object;
    if (state == 0) return;
    *(s32*)(state + 0x28) = 0;
    *(s32*)(state + 0x2C) = 0;
}
