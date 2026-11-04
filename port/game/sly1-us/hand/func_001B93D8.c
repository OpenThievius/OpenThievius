/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only backfill: retail forwards a0-a2 to func_001261C0; the matching
 * source in sly1-decomp currently declares no arguments and drops them. */
#include "openrac/guest.h"

void func_001B93D8(gaddr a0, gaddr a1, gaddr a2) {
    const uint64_t ints[8] = {a0, a1, a2};
    const float floats[8] = {0};
    float result = 0.0f;
    (void)openrac_guest_call_regs(0x001261C0u, ints, floats, &result, 0);
}
