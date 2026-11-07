/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only copies of the retail packed-vector readers. */
#include "common.h"

void func_00137A60(void* reader, s32 size, void* destination);

void func_00137E90(void* reader, void* destination) {
    func_00137A60(reader, 0x10, destination);
}

void func_00137F08(void* reader, u8* destination) {
    for (s32 i = 0; i < 4; ++i) {
        u8* vector = destination + (u32)i * 0x10u;
        func_00137A60(reader, 12, vector);
        *(u32*)(vector + 12) = i == 3 ? 0x3F800000u : 0;
    }
}
