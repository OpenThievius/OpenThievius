/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only C translation of the retail completion percentage query. */
#include "common.h"

extern u8 D_00247AF0[];

s32 func_00160B28(u8* root)
{
    s32 total = 0;
    s32 done = 0;
    u32 i;
    s32 k;
    if (root == 0) return 0;

    for (i = 0; i < 0x2Eu; ++i) {
        u8* entry = D_00247AF0 + i * 0x2Cu;
        u32 value = *(u32*)(entry + 0x20);
        s32 index = (s32)value >> 8;
        if (index != 0) {
            u32 flags = *(u32*)(entry + 0x28);
            u32* record = (u32*)(root + index * 0x44C + 0x10 +
                                 (value & 0xFFu) * 0x78u);
            u32 bit;
            ++total;
            done += (s32)(record[0] & 1u);
            for (bit = 2; (bit & 0xEu) != 0; bit <<= 1) {
                if ((flags & bit) != 0) {
                    ++total;
                    if ((record[0] & bit) != 0) ++done;
                }
            }
        }
    }
    for (k = 0; k < 5; ++k) {
        ++total;
        if ((*(u32*)(root + 0x8A4 + k * 0x44C) & 0x20u) != 0) ++done;
    }
    if (done == 0) return 0;
    if (done == total) return 100;
    if (total <= 0) return 0;
    done = done * 100 / total;
    if (done < 1) done = 1;
    if (done > 99) done = 99;
    return done;
}
