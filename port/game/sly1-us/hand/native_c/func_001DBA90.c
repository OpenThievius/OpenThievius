/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only native equivalent of the Sly four-lane vector store. */
#include "common.h"

void func_001DBA90(void* self, f32 value) {
    u32* vector = (u32*)((u8*)self + 0x1EE0);
    union { f32 f; u32 u; } bits;

    bits.f = value;
    vector[0] = 0;
    vector[1] = 0;
    vector[2] = bits.u;
    vector[3] = 0;
}
