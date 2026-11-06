/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only object activation walk reconstructed from NTSC-U retail. */
#include "common.h"

u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);
void sly1_native_trace_font(u32 stage, u32 a0, u32 a1, u32 a2, u32 a3,
                            u32 a4);

typedef struct NativeChildIterator {
    u32 list;
    u32 link;
    s32 offset;
    u32 saved;
} NativeChildIterator;

void func_00124050(void* self_pointer)
{
    u32 self = (u32)self_pointer;
    u32 descriptor;
    u32 method;
    NativeChildIterator iterator;

    if (self == 0) return;
    descriptor = *(u32*)self;
    if (descriptor == 0) return;

    iterator.list = self + 0x34u;
    iterator.link = iterator.list;
    iterator.offset = *(s32*)(iterator.list + 8u);
    iterator.saved = *(u32*)0x0027B314u;
    *(u32*)0x0027B314u = (u32)&iterator;

    /* Descriptor slot 6 runs before walking the object's child list. */
    method = *(u32*)(descriptor + 0x28u);
    if (method != 0)
        (void)sly1_native_call_regs4(method, self, 0, 0, 0);

    for (;;) {
        u32 object = *(u32*)iterator.link;
        u32 child_descriptor;
        if (object == 0) break;

        iterator.link = object + (u32)iterator.offset;
        child_descriptor = *(u32*)object;
        if (child_descriptor == 0) continue;
        method = *(u32*)(child_descriptor + 0x20u);
        if (method != 0)
            (void)sly1_native_call_regs4(method, object, 0, 0, 0);
    }

    /* Descriptor slot 14 notifies this object that its children are active. */
    method = *(u32*)(descriptor + 0x48u);
    if (method != 0)
        (void)sly1_native_call_regs4(method, self, 1, self, 0);

    sly1_native_trace_font(34, self, descriptor, iterator.offset,
                           *(u32*)(descriptor + 0x28u), method);

    *(u32*)0x0027B314u = iterator.saved;
}
