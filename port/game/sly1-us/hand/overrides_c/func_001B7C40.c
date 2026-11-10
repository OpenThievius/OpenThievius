/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port override for the collider enabled-state transition. */
#include "common.h"

void func_00152470(void* list, void* item);
void func_00152600(void* list, void* item);
void func_00138BE0(void* unused, void* collider, u32 mask);
void sly1_native_call_method(u32 address, u32 object);

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
