/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only replacement for the still assembly-only retail byte-copy helper. */
#include "common.h"

void* func_001F5A58(void* destination, const void* source, s32 size)
{
    if (size <= 0) return destination;
    return __builtin_memmove(destination, source, (u32)size);
}
