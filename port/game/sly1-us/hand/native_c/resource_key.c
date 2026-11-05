/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only translation of the retail eight-byte resource-key initializer. */
#include "common.h"

void func_0018D410(void* key)
{
    u32* words = (u32*)key;
    words[0] = 0;
    words[1] = 0;
}
