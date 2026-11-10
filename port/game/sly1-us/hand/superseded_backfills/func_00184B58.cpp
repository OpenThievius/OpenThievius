// SPDX-License-Identifier: AGPL-3.0-or-later
// The retail camera's VU0 update returns without effects when C51::value is
// zero. Keep that startup fast path native; fail loudly if a later scene needs
// the unimplemented nonzero VU0 path.
#include "common.h"
#include "c51.h"

extern "C" [[noreturn]] void sly1_missing_function(u32 address, u32 caller,
                                                    u32 argument0);

void func_00184B58(C51* self) {
    if (self->value == 0) return;
    sly1_missing_function(0x00184B58u, 0, (u32)self);
}
