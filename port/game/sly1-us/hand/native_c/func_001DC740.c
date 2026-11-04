/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only initialization of the 256-entry Sly resource-node pool. */
#include "common.h"

void* func_0018D5C0(s32 size);

void func_001DC740(void* self) {
    u8* object = (u8*)self;
    u32 nodes = (u32)func_0018D5C0(0x1000);
    u32 index;

    *(u32*)(object + 0x1AE0) = nodes;
    *(u32*)(object + 0x1AE4) = nodes;
    for (index = 0; index < 0x100; ++index) {
        u32 node = nodes + index * 0x10;
        *(u32*)(node + 0x08) = index < 0xFF ? node + 0x10 : 0;
        *(u32*)(node + 0x0C) = index != 0 ? node - 0x10 : 0;
    }
}
