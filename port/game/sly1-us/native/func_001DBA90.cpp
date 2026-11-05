// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only equivalent of Sly's four-lane vector store.
#include "sly1/host_abi.h"

extern "C" void func_001DBA90(void* self, f32 value) {
    u32* vector = reinterpret_cast<u32*>(static_cast<u8*>(self) + 0x1EE0);
    union FloatBits {
        f32 f;
        u32 u;
    } bits;

    bits.f = value;
    vector[0] = 0;
    vector[1] = 0;
    vector[2] = bits.u;
    vector[3] = 0;
}
