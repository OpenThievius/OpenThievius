/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only overlay stack insertion from the retail menu startup path. */
#include "common.h"

extern u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);

void func_001E9498(void* stack, void* node) {
    u8* bytes = (u8*)stack;
    s32 count = *(s32*)(bytes + 0x2C);
    u32 node_vtable = *(u32*)node;

    if (count >= 2) {
        u32 previous = *(u32*)(bytes + 0x0C + (count - 1) * 4);
        if (previous != 0) {
            u32 vtable = *(u32*)previous;
            u32 top_changed = vtable != 0 ? *(u32*)(vtable + 0x0C) : 0;
            if (top_changed != 0)
                (void)sly1_native_call_regs4(top_changed, previous, 0, 0, 0);
        }
    }

    if (node_vtable != 0) {
        u32 top_changed = *(u32*)(node_vtable + 0x0C);
        if (top_changed != 0)
            (void)sly1_native_call_regs4(top_changed, (u32)node, 1, 0, 0);
    }

    *(u32*)(bytes + 0x0C + count * 4) = (u32)node;
    *(s32*)(bytes + 0x2C) = count + 1;

    if (node_vtable != 0) {
        u32 activated = *(u32*)(node_vtable + 0x18);
        if (activated != 0)
            (void)sly1_native_call_regs4(activated, (u32)node, 0, 0, 0);
    }
}
