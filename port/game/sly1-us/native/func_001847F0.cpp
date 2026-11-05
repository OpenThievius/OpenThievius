// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only camera-vector setter. Retail's Quad16 source is aligned, but the
// native linker may place the retail data symbol at an 8-byte boundary.
#include "common.h"
#include "c51.h"

void func_001841D0(C51* self);

void func_001847F0(C51* self, const void* source) {
    u8* destination = (u8*)self + 0x2E0;
    const u8* bytes = (const u8*)source;
    for (u32 i = 0; i < 16; ++i) destination[i] = bytes[i];
    func_001841D0(self);
}
