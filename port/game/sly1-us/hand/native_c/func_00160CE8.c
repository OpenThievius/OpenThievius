// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * Port-only backfill for the service counter reset. The decomp checkout has
 * this body, but hostgen still sees US status "no-c" and emits a no-op. Remove
 * this file and its manifest entries when matching C is included in hostgen.
 */
#include "common.h"

typedef struct Counter_160CE8 {
    u8 pad[4];
    s32 count;
} Counter_160CE8;

void func_00160CE8(Counter_160CE8* counter) {
    counter->count = 0;
}
