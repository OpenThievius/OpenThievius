/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only object-list backfills translated from the NTSC-U retail C path. */
#include "common.h"

void* func_00152460(void* list, u32 item);
void func_00152470(void* list, void* item);
void sly1_native_call_method(u32 address, u32 object);

s32 func_001526D0(void* list, void* item) {
    u8* slot = (u8*)func_00152460(list, (u32)item);
    if (*(u32*)slot != 0) return 1;
    return *(u32*)((u8*)list + 4) == (u32)item;
}

s32 func_00182FC8(void* object) {
    u32 node = (u32)object;

    while (node != 0) {
        u32 parent = *(u32*)(node + 0x18);
        u32 list;

        if (parent != 0) {
            /* C02::children follows the 0x34-byte C01 base object. */
            list = parent + 0x34;
        } else {
            u32 world = *(u32*)(node + 0x14);
            if (world == 0) return 0;
            /* C6C::list48 is the root object's world-membership list. */
            list = world + 0x48;
        }

        if (func_001526D0((void*)list, (void*)node) == 0) return 0;
        node = parent;
    }
    return 1;
}

void func_00182C80(void* object) {
    u32 node = (u32)object;
    u32 parent = *(u32*)(node + 0x18);
    u32 list;
    u32 descriptor;
    u32 activate;

    if (parent != 0) {
        list = parent + 0x34;
    } else {
        u32 world = *(u32*)(node + 0x14);
        if (world == 0) return;
        list = world + 0x48;
    }

    if (func_001526D0((void*)list, object) != 0) return;
    func_00152470((void*)list, object);

    if (func_00182FC8(object) == 0) return;
    descriptor = *(u32*)node;
    if (descriptor == 0) return;
    activate = *(u32*)(descriptor + 0x20);
    if (activate != 0) sly1_native_call_method(activate, node);
}
