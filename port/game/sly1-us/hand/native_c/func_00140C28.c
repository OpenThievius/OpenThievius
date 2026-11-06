/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only synchronous disc-read adapter for the native ISO backend. */
#include "common.h"

s32 sly1_native_read_sectors(u32 sector, u32 count, u32 destination);

void func_00140C28(s32 sector, s32 count, s32 destination)
{
    (void)sly1_native_read_sectors((u32)sector, (u32)count,
                                    (u32)destination);
}
