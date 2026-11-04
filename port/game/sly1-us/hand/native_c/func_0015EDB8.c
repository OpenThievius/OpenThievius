// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only implementation of the retail executable checksum guard.
#include "common.h"

void func_0015EDB8(void) {
    u32 crc = 0xFFFFFFFFu;
    for (u32 offset = 0; offset < 0x8D74u; ++offset) {
        const u32 byte = *((u8*)(0x00100000u + offset));
        const u32 table_index = (crc ^ byte) & 0xFFu;
        crc = *((u32*)(0x002C3B00u + table_index * 4u)) ^ (crc >> 8);
    }
    if (crc != 0) *((u32*)0x00262310u) = 1;
}
