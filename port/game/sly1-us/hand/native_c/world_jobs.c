/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only deferred world-job drain translated from NTSC-U retail C. */
#include "common.h"

u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);

void func_001DBE48(void* world) {
    u8* object = (u8*)world;

    while (*(u32*)(object + 0x1EC8) != 0) {
        u32 node = *(u32*)(object + 0x1EC8);
        u32 tail = *(u32*)(object + 0x1ECC);
        u32 next = *(u32*)(node + 0x10);
        u32 callback = *(u32*)(node + 0x00);
        u32 arg0 = *(u32*)(node + 0x04);
        u32 arg1 = *(u32*)(node + 0x08);
        u32 arg2 = *(u32*)(node + 0x0C);

        if (node == tail) *(u32*)(object + 0x1ECC) = 0;
        *(u32*)(object + 0x1EC8) = next;
        *(u32*)(node + 0x10) = 0;

        if (callback != 0)
            (void)sly1_native_call_regs4(callback, arg0, arg1, arg2, 0);

        /* Pool6C at +0x1B14 stores its free-list head at +8. */
        *(u32*)node = *(u32*)(object + 0x1B1Cu);
        *(u32*)(object + 0x1B1Cu) = node;
    }
}
