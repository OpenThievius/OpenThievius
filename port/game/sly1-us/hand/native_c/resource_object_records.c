/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only resource object record loader, kept separate from matched C. */
#include "common.h"

s32 func_00137D10(void* reader);
void* func_0011E580(s32 slot, void* world, void* parameter, s32 key, s32 flag);
s32 func_0011ECA0(s32 key);
void func_00182ED8(void* object);
u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);
void func_0011A6C0(void* list, s32 value);

void* func_00159F20(void* base, s32 index)
{
    const u32 slot = ((u32)(index * 0x95675) & 0x1FFu) * 12u + 0x84u;
    return (void*)((u32)base + slot);
}

void func_001DB480(void* object)
{
    s32 index;
    for (index = 0; index < 0x200; ++index) {
        func_00152438((u8*)object + 0x84u + (u32)index * 0xCu, 0xCu);
    }
}

void* func_001C1390(s32 index, void* context)
{
    u8* table;
    void* value;
    if (index == -1) return 0;
    table = *(u8**)((u8*)context + 0x1EF4u);
    value = *(void**)(table + (u32)index * 8u + 4u);
    if (value != 0) func_0011A6C0((void*)0x00259F38u, (s32)value);
    return value;
}

s32 func_0011ECA0(s32 value) {
    s32 index = value - 12;
    return (u32)index > 30u ? -1 : index;
}

static u32 object_is_linked(void* object) {
    u8* node = (u8*)object;
    while (node != 0) {
        u8* parent = (u8*)*(u32*)(node + 0x18);
        u8* list = parent != 0 ? parent + 0x34 : (u8*)*(u32*)(node + 0x14) + 0x48;
        if (list < (u8*)0x48 ||
            sly1_native_call_regs4(0x001526D0u, (u32)list, (u32)node, 0, 0) == 0) {
            return 0;
        }
        node = parent;
    }
    return 1;
}

void func_00182ED8(void* object) {
    u32 descriptor;
    u32 method;
    if (object == 0 || !object_is_linked(object)) return;
    descriptor = *(u32*)object;
    if (descriptor == 0) return;
    method = *(u32*)(descriptor + 0x4Cu);
    if (method != 0) sly1_native_call_regs4(method, (u32)object, 0, 0, 0);
    method = *(u32*)(descriptor + 0x50u);
    if (method != 0) sly1_native_call_regs4(method, (u32)object, 0, 0, 0);
    method = *(u32*)(descriptor + 0x1Cu);
    if (method != 0) sly1_native_call_regs4(method, (u32)object, 0, 0, 0);
}

void func_0011ECB8(void* world, void* parameter, void* reader) {
    s32 count = func_00137D10(reader);
    while (count-- > 0) {
        s32 slot = func_00137D10(reader);
        s32 key = func_00137D10(reader);
        s32 flag = func_00137D10(reader);
        void* object = func_0011E580(slot, world, parameter, key, flag);

        if (object == 0) continue;
        u32 vtable = *(u32*)object;
        u32 read_method = vtable != 0 ? *(u32*)(vtable + 14u * 4u) : 0;
        if (read_method != 0) {
            sly1_native_call_regs4(read_method, (u32)object, (u32)reader, 0, 0);
        }

        s32 index = func_0011ECA0(key);
        if (index >= 0 && index < 31) {
            *(u32*)((u8*)world + 0x1CE8u + (u32)index * 4u) = (u32)object;
            func_00182ED8(object);
        }
    }
}
