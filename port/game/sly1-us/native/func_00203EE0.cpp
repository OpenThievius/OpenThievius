// SPDX-License-Identifier: AGPL-3.0-or-later
// A mounted host ISO is stable for this native session. Replace the PS2 IOP
// change poll with its successful no-change result; keep game logic in the
// decompilation untouched so a native IOP service can supersede this later.
#include "sly1/host_abi.h"

extern "C" s32 func_00203EE0(s32, s32* out) {
    if (out != nullptr) *out = 0;
    return 0;
}
