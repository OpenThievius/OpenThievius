/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only host form of retail QueueWorldLoad (0x001E4E80).
 * Replace this when file_1E08F8.cpp can be translated by hostgen. */
#include "common.h"

void func_001E4E80(gaddr request, gaddr key, s32 f8, s32 fC, s32 flags) {
    if (request == 0 || GREF(s32, request) != 0) return;

    GREF(s32, request) = 1;
    GREF(gaddr, request + 4) = key;
    GREF(s32, request + 8) = f8;
    GREF(s32, request + 0x0C) = fC;
    GREF(s32, request + 0x10) = flags;
}
