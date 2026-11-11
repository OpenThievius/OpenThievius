/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only FindFirstObjectById wrapper from the canonical decomp. */
#include "common.h"

extern s32 func_0015A198(void* world, s32 flags, s32 id, void* root,
                         s32 max_count, void* found);

void* func_0015A418(void* world, s32 flags, s32 id, void* root)
{
    void* found = 0;
    (void)func_0015A198(world, flags | 0x200, id, root, 1, &found);
    return found;
}
