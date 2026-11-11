/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only host wrapper for the canonical FindFirstObjectByIdGathered at 0x0015A838. */
#include "common.h"

extern s32 func_0015A4F0(void* world, u32 flags, s32 kind, void* root,
                         s32 max_count, void* found);

s32 func_0015A838(void* world, u32 flags, s32 kind, void* root)
{
    s32 found = 0;
    func_0015A4F0(world, flags | 0x200u, kind, root, 1, &found);
    return found;
}
