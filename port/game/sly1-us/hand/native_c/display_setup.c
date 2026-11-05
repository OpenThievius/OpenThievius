/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only display setup backfills translated from the NTSC-U ELF. */
#include "common.h"

void func_001FFBE0(void* first, void* entry)
{
    u8* node = (u8*)entry;
    u32* head = (u32*)0x0062A7B8u;
    u32 prior = *head;

    *(u32*)(node + 0x0C) = (u32)first;
    *(u32*)(node + 0x14) = prior;
    *head = (u32)entry;
    *(u32*)(node + 0x08) = (u32)first;
    *(u32*)(node + 0x04) = 0;
    *(u32*)(node + 0x00) = 0;
    *(u32*)(node + 0x10) = 0;
}

int func_001C29C8(void)
{
    u32* first = (u32*)0x002748F0u;
    u32* second = (u32*)0x00274940u;
    u32 i;

    for (i = 0; i < 20; ++i) {
        first[i] = 0;
        second[i] = 0;
    }
    first[0] = 0x02605440u;
    first[1] = 0x70000000u;
    second[0] = 0x026054C0u;
    second[1] = 0x70002000u;
    return 0;
}
