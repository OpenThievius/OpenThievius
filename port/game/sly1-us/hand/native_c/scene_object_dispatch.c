/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only scene-object virtual dispatch helpers from US retail. */
#include "common.h"

u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);

void func_00125108(void* object)
{
    u32 vtable = object != 0 ? *(u32*)object : 0;
    u32 method = vtable != 0 ? *(u32*)(vtable + 0x5C) : 0;
    if (method != 0)
        (void)sly1_native_call_regs4(method, (u32)object, 0, 0, 0);
}

void func_00182D00(void* object)
{
    u32 vtable = object != 0 ? *(u32*)object : 0;
    u32 method = vtable != 0 ? *(u32*)(vtable + 0x28) : 0;
    if (method != 0)
        (void)sly1_native_call_regs4(method, (u32)object, 0, 0, 0);

    method = vtable != 0 ? *(u32*)(vtable + 0x48) : 0;
    if (method != 0)
        (void)sly1_native_call_regs4(method, (u32)object, 1,
                                     (u32)object, 0);
}
