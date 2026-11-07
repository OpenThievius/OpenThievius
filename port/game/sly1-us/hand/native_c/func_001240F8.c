/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only placed-object registration reconstructed from NTSC-U retail. */
#include "common.h"

u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);
f32 sly1_native_call_f32(u32 address, u32 a0, u32 a1, u32 a2, u32 a3,
                         f32 f12, f32 f13);
void func_00152470(void* list, void* item);
void func_0015F7D8(void* world, void* group);
void func_00129DF8(void* actor);
void sly1_native_trace_font(u32 stage, u32 a0, u32 a1, u32 a2, u32 a3,
                            u32 a4);

void func_00143AF8(void* camera_pointer, void* item_pointer)
{
    u32 camera = (u32)camera_pointer;
    u32 item = (u32)item_pointer;
    u32 count;

    if (camera == 0 || item == 0) return;
    count = *(u32*)(camera + 0x340u);
    if (count < 8u) {
        *(u32*)(camera + 0x344u + count * 4u) = item;
        *(u32*)(camera + 0x340u) = count + 1u;
    }
}

void func_001240F8(void* self_pointer)
{
    u32 self = (u32)self_pointer;
    u32 descriptor;
    u32 world;
    u32 parent;
    u32 method;

    if (self == 0) return;
    descriptor = *(u32*)self;
    world = *(u32*)(self + 0x14u);
    parent = *(u32*)(self + 0x18u);
    if (descriptor == 0 || world == 0) return;

    /* Identify the two object classes currently mistaken for level meshes. */
    {
        u32 class_id = *(u32*)(descriptor + 4u);
        if (class_id == 0x1Eu || class_id == 0x51u) {
            sly1_native_trace_font(55, self, class_id, world, parent,
                                   *(u32*)(self + 0x1F4u));
            sly1_native_trace_font(56, self, *(u32*)(self + 0x1ECu),
                                   *(u32*)(self + 0x1F0u),
                                   *(u32*)(self + 0x224u),
                                   *(u32*)(self + 0x2A8u));
        }
    }

    if (parent == 0) {
        *(u32*)(self + 0x50u) = self;
        if (*(u32*)(self + 0x294u) != 0) {
            func_00152470((void*)(world + 0x78u), (void*)self);
        } else {
            func_00152470((void*)(world + 0x54u), (void*)self);
            *(u64*)(self + 0x2C8u) |= 0x2000000000ULL;
            func_00152470((void*)(world + 0x60u), (void*)self);
            if ((*(u32*)(descriptor + 8u) & 2u) != 0)
                func_00152470((void*)(world + 0x6Cu), (void*)self);

            *(u32*)(self + 0x54u) = self;
            *(u32*)(self + 0x60u) = self;
            *(u32*)(self + 0x64u) = self;
            for (s32 i = *(s32*)(self + 0x6Cu) - 1; i >= 0; --i) {
                const u32 group = *(u32*)(self + 0x70u + (u32)i * 4u);
                if (group != 0)
                    func_0015F7D8((void*)world, (void*)group);
            }
        }
    } else {
        *(u32*)(self + 0x50u) = *(u32*)(parent + 0x50u);
        if ((*(u64*)(parent + 0x2C8u) & 0x4000000000ULL) != 0) {
            method = *(u32*)(descriptor + 0x60u);
            if (method != 0)
                (void)sly1_native_call_regs4(method, self, 1, 0, 0);
        }
    }

    /* Placement attachments supply the position and rotation used by the
     * descriptor's matrix setters. Their absent case is common for roots. */
    {
        u32 attachment = *(u32*)(self + 0x1ECu);
        if (attachment != 0) {
            f32 position_a[4] = {0};
            f32 position_b[4] = {0};
            u32 vtable = *(u32*)attachment;
            method = vtable != 0 ? *(u32*)(vtable + 0x10u) : 0;
            if (method != 0) {
                (void)sly1_native_call_f32(method, attachment,
                    (u32)position_a, (u32)position_b, 0, 0.0f, 0.0f);
            }
            method = *(u32*)(descriptor + 0x84u);
            if (method != 0)
                (void)sly1_native_call_regs4(method, self,
                    (u32)position_a, 0, 0);
            method = *(u32*)(descriptor + 0x90u);
            if (method != 0)
                (void)sly1_native_call_regs4(method, self,
                    (u32)position_b, 0, 0);
        }
    }
    {
        u32 attachment = *(u32*)(self + 0x1F0u);
        if (attachment != 0) {
            f32 rotation[4] = {0};
            f32 position[4] = {0};
            u32 vtable = *(u32*)attachment;
            method = vtable != 0 ? *(u32*)(vtable + 0x14u) : 0;
            if (method != 0) {
                (void)sly1_native_call_f32(method, attachment,
                    (u32)rotation, (u32)position, 0, 0.0f, 0.0f);
            }
            method = *(u32*)(descriptor + 0x88u);
            if (method != 0)
                (void)sly1_native_call_regs4(method, self,
                    (u32)rotation, 0, 0);
            method = *(u32*)(descriptor + 0x94u);
            if (method != 0)
                (void)sly1_native_call_regs4(method, self,
                    (u32)position, 0, 0);
        }
    }

    if ((*(u64*)(self + 0x2C8u) & 0x1000000000ULL) != 0)
        func_00143AF8((void*)0x00261990u, (void*)self);

    if (*(u32*)(self + 0x284u) != 0)
        func_00152470((void*)(world + 0x1C00u),
                      (void*)*(u32*)(self + 0x284u));

    method = *(u32*)(descriptor + 0x58u);
    if (method != 0)
        (void)sly1_native_call_regs4(method, self, 0, 0, 0);

    (void)sly1_native_call_regs4(0x001837E0u, self, 4, 0, 0);
    func_00129DF8((void*)self);
}
