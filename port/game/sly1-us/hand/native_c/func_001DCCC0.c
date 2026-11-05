/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only resource list skip from retail func_001DCCC0. */
#include "common.h"

extern s32 D_00275714;

s32 func_00137C40(void* reader);
void func_00138518(void* reader);

void func_001DCCC0(void* reader)
{
    s32 count = (s32)func_00137C40(reader);
    s32 index;

    /* Every skipped entry consumes at least a 16-bit length. This mirrors the
     * native parser's resource-size bound while preserving the valid path. */
    if (count < 0 || count > 0x08000000) return;
    if (D_00275714 == 0) return;
    for (index = 1; index < count; ++index) func_00138518(reader);
}
