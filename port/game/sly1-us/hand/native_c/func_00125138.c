/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only scalar translation of the retail VU object transform/update. */
#include "common.h"

u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);
f32 sly1_native_call_f32(u32 address, u32 a0, u32 a1, u32 a2, u32 a3,
                         f32 f12, f32 f13);

static void copy_vectors(u32 destination, u32 source, u32 count)
{
    for (u32 i = 0; i < count; ++i)
        __builtin_memcpy((void*)(destination + i * 16u),
                         (const void*)(source + i * 16u), 16);
}

/* Retail stores the three basis columns as packed vectors and translation
 * separately. The VU instructions evaluate Mx*x + My*y + Mz*z (+ T). */
static void transform_vector(u32 matrix_base, u32 source, u32 destination,
                             s32 include_translation)
{
    const f32 x = *(f32*)(source + 0);
    const f32 y = *(f32*)(source + 4);
    const f32 z = *(f32*)(source + 8);
    for (s32 lane = 0; lane < 3; ++lane) {
        f32 value = *(f32*)(matrix_base + 0x110u + (u32)lane * 4u) * x;
        value += *(f32*)(matrix_base + 0x120u + (u32)lane * 4u) * y;
        value += *(f32*)(matrix_base + 0x130u + (u32)lane * 4u) * z;
        if (include_translation)
            value += *(f32*)(matrix_base + 0x140u + (u32)lane * 4u);
        *(f32*)(destination + (u32)lane * 4u) = value;
    }
    *(f32*)(destination + 12u) = include_translation
        ? *(f32*)(source + 12u) + *(f32*)(matrix_base + 0x14Cu)
        : *(f32*)(source + 12u);
}

static void update_transform(u32 self, u32 base, s32 position, s32 basis)
{
    if (position) {
        if (base != 0) transform_vector(base, self + 0x100u,
                                        self + 0x140u, 1);
        else __builtin_memcpy((void*)(self + 0x140u),
                              (const void*)(self + 0x100u), 16);
    }
    if (basis) {
        if (base != 0) {
            transform_vector(base, self + 0xD0u, self + 0x110u, 0);
            transform_vector(base, self + 0xE0u, self + 0x120u, 0);
            transform_vector(base, self + 0xF0u, self + 0x130u, 0);
        } else {
            copy_vectors(self + 0x110u, self + 0xD0u, 3);
        }
    }
}

void func_00125138(void* object)
{
    const u32 self = (u32)object;
    const u32 parent = *(u32*)(self + 0x18u);
    const u32 frame = *(u32*)(self + 0x224u);
    u32 frame_flags = 0;

    if (frame != 0) frame_flags = *(u32*)(frame + 0xB0u);
    if (frame != 0 && (frame_flags & 0xCu) != 0) {
        const u32 position_base = (frame_flags & 4u) != 0 ? frame : parent;
        const u32 basis_base = (frame_flags & 8u) != 0 ? frame : parent;
        update_transform(self, position_base != 0 ? position_base + 0x60u : 0,
                         1, 0);
        update_transform(self, basis_base != 0 ? basis_base + 0x60u : 0,
                         0, 1);
    } else {
        const u32 base = parent != 0 ? parent + 0x60u : 0;
        update_transform(self, base, 1, 1);
    }

    /* A transform mode of 2 asks the world for the object's spatial cell. */
    if (((*(u32*)(self + 0x2CBu)) & 3u) == 2u) {
        const u32 world = *(u32*)(self + 0x14u);
        const u32 query = world != 0 ? *(u32*)(world + 0x1EC4u) : 0;
        if (query != 0) {
            (void)sly1_native_call_f32(0x001EEB60u, query, self + 0x140u,
                                       self + 0x88u, 0,
                                       *(f32*)(self + 0x2A8u), 0.0f);
        } else {
            *(s32*)(self + 0x88u) = -1;
        }
    }

    const u32 collision = *(u32*)(self + 0x284u);
    (void)sly1_native_call_regs4(0x001B1DE0u, collision,
                                 self + 0x140u, 0, 0);
    if (collision != 0) {
        const u32 vtable = *(u32*)collision;
        if (vtable != 0 && *(u8*)vtable == 3u) {
            f32 scratch[12];
            *(f32*)&scratch[0] = -1.0f;
            (void)sly1_native_call_regs4(0x001B1EC8u, collision,
                                         (u32)scratch, 0, 0);
            (void)sly1_native_call_regs4(0x001B2068u, collision,
                                         self + 0x120u, 0, 0);
        }
    }

    /* Retail calls each child object's virtual update slot 0x5C. */
    u32 child = *(u32*)(self + 0x34u);
    while (child != 0) {
        const u32 vtable = *(u32*)child;
        const u32 method = vtable != 0 ? *(u32*)(vtable + 0x5Cu) : 0;
        if (method != 0)
            (void)sly1_native_call_regs4(method, child, 0, 0, 0);
        child = *(u32*)(child + 0x1Cu);
    }

    if (frame != 0 && (frame_flags & 0x8020u) == 0x8020u) {
        const u32 first = *(u32*)(frame + 0x80u);
        const u32 second = *(u32*)(frame + 0x84u);
        if (first != 0) {
            const u32 nested = *(u32*)(first + 0x224u);
            if (nested != 0) *(s32*)(nested + 0x64u) = 1;
        }
        if (second != 0) {
            const u32 nested = *(u32*)(second + 0x224u);
            if (nested != 0) *(s32*)(nested + 0x64u) = 1;
        }
    }
}
