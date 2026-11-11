/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only pool pop from the state-walker pool layout in the current decomp. */
#include "common.h"

typedef struct Sly1FreePool {
    u32 stride;
    u32 pad04;
    u32 head;
} Sly1FreePool;

void* func_001B5EA0(Sly1FreePool* pool)
{
    u32 node = pool->head;
    if (node != 0) pool->head = *(u32*)node;
    return (void*)node;
}
