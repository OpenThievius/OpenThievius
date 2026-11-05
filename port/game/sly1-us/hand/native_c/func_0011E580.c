/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only Sly engine object factory, derived from the NTSC-U retail call path. */
#include "common.h"

void* func_0018D5C0(s32 size);
void func_001DB480(void* self);
void* func_001C1390(s32 listener_id, void* owner);
void* func_00159F20(void* owner, s32 mode);
void func_00152470(void* list, void* object);
void sly1_native_call_constructor(u32 address, u32 object);

void* func_0011E580(s32 type, void* owner, void* parameter, s32 mode,
                    s32 listener_id) {
    u32 descriptor_address;
    u32 allocation_size;
    u32 constructor_address;
    void* descriptor;
    void* object;
    void* attachment;
    void* owner_list;

    /* D_00247448 is the 149-entry retail descriptor table (through 0x94). */
    if (type < 0 || type >= 0x95 || (type != 0x6C && owner == 0)) return 0;

    descriptor_address = *(u32*)(0x00247448u + (u32)type * 4u);
    if (descriptor_address == 0) return 0;
    descriptor = (void*)descriptor_address;
    allocation_size = *(u32*)((u8*)descriptor + 0x0C);
    if (allocation_size == 0) return 0;

    object = func_0018D5C0((s32)allocation_size);
    if (object == 0) return 0;

    *(u32*)((u8*)object + 0x00) = descriptor_address;
    *(u32*)((u8*)object + 0x08) = (u32)mode;

    if (type == 0x6C) {
        owner = object;
        func_001DB480(object);
    }
    *(u32*)((u8*)object + 0x18) = (u32)parameter;
    *(u32*)((u8*)object + 0x14) = (u32)owner;

    /* Retail resolves listener IDs through the owning world, not the parent. */
    attachment = func_001C1390(listener_id, owner);
    *(u32*)((u8*)object + 0x2C) = (u32)attachment;
    owner_list = func_00159F20(owner, mode);
    func_00152470(owner_list, object);

    *(u32*)((u8*)object + 0x24) =
        *(u32*)((u8*)owner + 0x1884 + (u32)type * 4u);
    *(u32*)((u8*)owner + 0x1884 + (u32)type * 4u) = (u32)object;

    constructor_address = *(u32*)((u8*)descriptor + 0x10);
    if (constructor_address != 0) {
        sly1_native_call_constructor(constructor_address, (u32)object);
    }
    return object;
}
