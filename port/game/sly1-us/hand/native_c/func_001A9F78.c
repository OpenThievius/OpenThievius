// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * Port-only backfill for InitScreenElements. The matching decomp source has
 * this loop, but US c_status.tsv still marks 0x001A9F78 as no-c, so hostgen
 * emits a no-op stub. Remove this file and manifest entry when that status is
 * updated and the matching C is included in the host build.
 */
#include "common.h"

u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);
void func_001A9F78(void) {
    for (s32 index = 0x24; index >= 0; --index) {
        u32 object = *(u32*)(0x002486B0u + (u32)index * 4u);
        u32 vtable = *(u32*)object;
        u32 constructor = *(u32*)(vtable + 4u);

        if (constructor != 0)
            sly1_native_call_regs4(constructor, object, 0, 0, 0);
    }
}
