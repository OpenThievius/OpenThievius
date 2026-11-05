/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only refill dispatch for Sly's buffered resource streams. */
#include "common.h"

s32 func_001F8DB8(void* stream, s32 destination, s32 byte_count);
void func_00137490(void* stream);
void func_00193288(void* progress, s32 value);

void func_001377C0(void* stream) {
    u8* bytes = (u8*)stream;
    s32 size = *(s32*)(bytes + 0x0C);
    s32 limit = *(s32*)(bytes + 0x14);

    *(s32*)(bytes + 0x20) = *(s32*)(bytes + 0x08);
    size = size > 0x8000 ? (limit < 0x8000 ? limit : 0x8000)
                         : (size < limit ? size : limit);
    *(s32*)(bytes + 0x24) = func_001F8DB8((void*)*(u32*)(bytes + 0x34),
                                           *(s32*)(bytes + 0x20), size);
}

void func_00137820(void* stream) {
    u8* bytes = (u8*)stream;
    s32 mode = *(s32*)(bytes + 0x04);

    if (mode == 0) func_001377C0(stream);
    else if (mode == 1) func_00137490(stream);

    s32 value = *(s32*)(bytes + 0x14) - *(s32*)(bytes + 0x24);
    if (value < 0) value = 0;
    *(s32*)(bytes + 0x14) = value;
    u32 progress = *(u32*)(bytes + 0x30);
    if (progress != 0) func_00193288((void*)progress, value);
}
