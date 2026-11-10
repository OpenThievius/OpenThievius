// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only VIF UNPACK writer. The matching body uses an incomplete helper
// prototype; retain retail's 16-entry unpack-size table and copy semantics.
#include "common.h"
#include "v24a0.h"

void func_001EE668(V24A0* writer, s32 format, s32 count, const void* source,
                   s32 tag) {
    static const s32 unpack_bytes[16] = {
        4, 2, 1, -1, 8, 4, 2, -1, 12, 6, 3, -1, 16, 8, 4, 2,
    };
    if (format < 0 || format >= 16 || count < 0) return;

    const s32 byte_count = unpack_bytes[format] * count;
    u32* cursor = (u32*)writer->cur;
    u32 header = ((u32)format << 24) | ((u32)count << 16) |
                 0x6000C000u | (u32)tag;
    if (writer->f1C != 0) header |= 0x10000000u;
    *cursor++ = header;

    const s32 word_count = (byte_count + 3) >> 2;
    const u32* words = (const u32*)source;
    for (s32 i = 0; i < word_count; ++i) cursor[i] = words[i];
    writer->cur = (u8*)(cursor + word_count);
}
