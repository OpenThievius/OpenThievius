/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only collision-loader helpers from retail class-0x03 routines. */
#include "common.h"

s32 func_00137BE0(void* stream);
void* func_0018D5C0(s32 size);
void func_00152470(void* list, void* item);
void func_00152600(void* list, void* item);
void sly1_native_call_method(u32 address, u32 object);

void func_00138278(void* stream, void* mesh, void* tree)
{
    u8* output = (u8*)tree;
    u32 nodes;
    s32 count = func_00137BE0(stream);
    s32 count2 = func_00137BE0(stream);
    s32 i;

    *(s32*)(output + 0) = count;
    *(s32*)(output + 4) = count2;
    nodes = (u32)func_0018D5C0(count * 12);
    *(u32*)(output + 8) = nodes;

    for (i = 0; i < count; ++i) {
        u8* node = (u8*)nodes + i * 12;
        s32 face = func_00137BE0(stream);
        s32 back = func_00137BE0(stream);
        s32 front = func_00137BE0(stream);
        *(u32*)(node + 0) = *(u32*)((u8*)mesh + 0x10) + ((u32)face << 5);
        if (((u32)back & 0xFFFFu) != 0xFFFFu)
            *(u32*)(node + 4) = nodes + (u32)back * 12u;
        if (((u32)front & 0xFFFFu) != 0xFFFFu)
            *(u32*)(node + 8) = nodes + (u32)front * 12u;
    }
}

void func_00138BE0(void* unused, void* collider, u32 mask)
{
    u8* root = (u8*)*(u32*)((u8*)collider + 0x50);
    if (root != 0)
        *(u32*)(root + 0x4B8) &= ~mask;
}

void func_001B7C40(void* collider, s32 enabled)
{
    u8* self = (u8*)collider;
    u64 flags = *(u64*)(self + 0x538);
    s32 was_enabled = (s32)((flags >> 50) & 1u);
    u8* root;
    u32 method;

    if ((enabled != 0) == (was_enabled != 0)) return;
    root = (u8*)*(u32*)(self + 0x50);
    if (root == 0) return;
    if (enabled != 0)
        func_00152470(root + 0x2D8, self);
    else
        func_00152600(root + 0x2D8, self);

    flags = (flags & ~((u64)1 << 50)) | ((u64)(enabled & 1) << 50);
    *(u64*)(self + 0x538) = flags;
    func_00138BE0((void*)*(u32*)(self + 0x14), self, 7);

    method = *(u32*)(*(u32*)root + 0xD8);
    if (method >= 0x00100000u && method < 0x00220000u)
        sly1_native_call_method(method, (u32)root);
}

void func_001BB4A0(void* collider)
{
    u8* self = (u8*)collider;
    u32 descriptor = *(u32*)self;
    u32 handler = 0;
    u8* child;
    if (descriptor != 0 &&
        (*(u32*)(descriptor + 0x114) != 0 ||
         *(u32*)(descriptor + 0x100) != 0 ||
         *(u32*)(descriptor + 0x104) != 0 ||
         *(u32*)(descriptor + 0x108) != 0)) {
        handler = (u32)self;
    } else {
        u32 parent = *(u32*)(self + 0x18);
        if (parent != 0) handler = *(u32*)((u8*)parent + 0x4E4);
    }
    *(u32*)(self + 0x4E4) = handler;

    child = (u8*)*(u32*)(self + 0x34);
    while (child != 0) {
        u32 child_descriptor = *(u32*)child;
        if (child_descriptor != 0 && (*(u32*)(child_descriptor + 8) & 2u) != 0)
            func_001BB4A0(child);
        child = (u8*)*(u32*)(child + 0x1C);
    }
}
