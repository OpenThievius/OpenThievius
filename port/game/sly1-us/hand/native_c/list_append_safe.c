/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only guard for unresolved resource references passed to list append. */
#include "common.h"

void func_0011A338(void* list, void* value)
{
    u32 count;

    if (list == 0) return;
    count = *(u32*)list;
    *(u32*)list = count + 1u;
    *(u32*)((u8*)list + count * 4u + 4u) = (u32)value;
}
