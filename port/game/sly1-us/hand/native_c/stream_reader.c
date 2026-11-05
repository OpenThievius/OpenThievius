/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only Sly stream-reader setup translated from NTSC-U retail behavior. */
#include "common.h"

void* func_00137290(void* self, s32 buffer_length, u32 buffer, s32 flags) {
    u8* bytes = (u8*)self;
    u32 aligned = (buffer + 0x3Fu) & ~0x3Fu;

    *(u32*)(bytes + 0x04) = 0xFFFFFFFFu;
    *(u32*)(bytes + 0x00) = (u32)flags;
    *(u32*)(bytes + 0x0C) = (u32)(buffer_length - (s32)(aligned - buffer));
    *(u32*)(bytes + 0x20) = 0;
    *(u32*)(bytes + 0x18) = 0;
    *(u32*)(bytes + 0x24) = 0;
    *(u32*)(bytes + 0x1C) = 0;
    *(u32*)(bytes + 0x30) = 0;
    *(u32*)(bytes + 0x28) = 0;
    *(u32*)(bytes + 0x2C) = 0;
    *(u32*)(bytes + 0x08) = aligned;
    return self;
}

s32 func_00137328(void* self, s32 sector, s32 byte_count) {
    u8* bytes = (u8*)self;
    s32 blocks = *(s32*)(bytes + 0x0C);
    s32 request_blocks;
    if (blocks < 0) blocks += 0xFFFFF;
    blocks >>= 20;
    request_blocks = blocks < 2 ? 2 : blocks;

    *(u32*)(bytes + 0x04) = 1;
    *(u32*)(bytes + 0x48) = (u32)sector;
    *(u32*)(bytes + 0x40) = (u32)byte_count;
    *(u32*)(bytes + 0x20) = *(u32*)(bytes + 0x08);
    *(u32*)(bytes + 0x10) = (u32)byte_count;
    *(u32*)(bytes + 0x14) = (u32)byte_count;
    *(u32*)(bytes + 0x38) = 0;
    *(u32*)(bytes + 0x44) = 0;
    *(u32*)(bytes + 0x3C) = 0;
    *(u32*)(bytes + 0x50) = 0;
    *(u32*)(bytes + 0x54) = 0;
    *(u32*)(bytes + 0x4C) = (u32)request_blocks;
    *(u32*)(bytes + 0x58) = (u32)(request_blocks - 1);
    return 1;
}

s32 func_001373B0(void* self, const s32* pair) {
    return func_00137328(self, pair[0], pair[1]);
}

extern s32 func_001F8C38(s32);
extern void func_00203E28(void);
extern void func_0011E2A0(void);

void func_001373D0(void* self) {
    u8* bytes = (u8*)self;
    s32 state = *(s32*)(bytes + 0x04);
    s32 result = -1;

    if (state == 0) {
        s32 callback = *(s32*)(bytes + 0x34);
        if (callback >= 0) (void)func_001F8C38(callback);
    } else if (state == 1 && *(s32*)(bytes + 0x3C) != 0) {
        if ((*(u32*)bytes & 2u) != 0) func_00203E28();
        else func_0011E2A0();
    }

    *(s32*)(bytes + 0x2C) = 0;
    *(s32*)(bytes + 0x04) = result;
    *(s32*)(bytes + 0x20) = 0;
    *(s32*)(bytes + 0x18) = 0;
    *(s32*)(bytes + 0x24) = 0;
    *(s32*)(bytes + 0x1C) = 0;
    *(s32*)(bytes + 0x28) = 0;
}
