// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only C backfill for Sly's one-time constructor-table dispatcher.
#include "common.h"

void func_001FAE18(void) {
    u32* constructors = (u32*)0x0027B130u;
    s32 count = (s32)constructors[0];

    // Retail uses -1 when it should find the last non-null constructor itself.
    if (count == -1) {
        count = 0;
        while (count < 1024 && constructors[count + 1] != 0) ++count;
    }
    if (count < 0 || count > 1024) return;

    // The retail guard is set before any constructor is called, preventing a
    // constructor from recursively dispatching the same table.
    u32* initialized = (u32*)0x0062A76Cu;
    if (*initialized != 0) return;
    *initialized = 1;

    while (count > 0) {
        void (*constructor)(void) = (void (*)(void))constructors[count];
        if (constructor != 0) constructor();
        --count;
    }
}
