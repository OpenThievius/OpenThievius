/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only contact-table bookkeeping for the US retail collider path. */
#include "common.h"

u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);

struct ContactRow_001385A0 {
    u32 contacts;
    u32 next;
    u32 paired;
    u32 other;
};

struct ContactTable_001385A0 {
    u32 rows;
    u32 owner;
    u32 next;
    u32 previous;
};

static u32 pool_pop(u32 world, u32 pool_offset)
{
    u32* head = (u32*)(world + pool_offset + 8);
    u32 node = *head;
    if (node != 0) *head = *(u32*)node;
    return node;
}

static void pool_push(u32 world, u32 pool_offset, u32 node)
{
    if (node == 0) return;
    *(u32*)node = *(u32*)(world + pool_offset + 8);
    *(u32*)(world + pool_offset + 8) = node;
}

u32 func_00138738(u32 world, u32 table, u32 other_table)
{
    u32 row;
    if (world == 0 || table == 0 || other_table == 0) return 0;
    row = pool_pop(world, 0x1AF0);
    if (row == 0) return 0;
    *(u32*)(row + 0) = 0;
    *(u32*)(row + 0x0C) = *(u32*)(other_table + 4);
    *(u32*)(row + 4) = *(u32*)table;
    *(u32*)table = row;
    return row;
}

u32 func_00138788(u32 unused, u32 head, u32 other_table)
{
    u32* link = (u32*)head;
    u32 row;
    u32 owner;
    if (link == 0 || other_table == 0) return 0;
    owner = *(u32*)(other_table + 4);
    while ((row = *link) != 0) {
        if (*(u32*)(row + 0x0C) == owner) {
            *link = *(u32*)(row + 4);
            return row;
        }
        link = (u32*)(row + 4);
    }
    return 0;
}

static void free_contact_chain(u32 world, u32 contact)
{
    while (contact != 0) {
        u32 next = *(u32*)(contact + 0xD0);
        pool_push(world, 0x1AFC, contact);
        contact = next;
    }
}

void func_00129DF8(void* actor)
{
    u32 object = (u32)actor;
    u32 root;
    if (object == 0) return;
    root = *(u32*)(object + 0x50);
    if (root != 0) *(s32*)(root + 0x228) = 0;
}

void func_001385A0(void* world_ptr, void* a_ptr, void* b_ptr,
                   s32 flags_a, s32 flags_b)
{
    u32 world = (u32)world_ptr;
    u32 a = (u32)a_ptr;
    u32 b = (u32)b_ptr;
    u32 tables;
    u32 pairs;
    s32 ia, ib;
    u8* ab;
    u8* ba;
    u8 flags;

    if (world == 0 || a == 0 || b == 0) return;
    tables = *(u32*)(world + 0x1AE0);
    pairs = *(u32*)(world + 0x1ADC);
    if (tables == 0 || pairs == 0) return;

    ia = (s32)(a - tables) >> 4;
    ib = (s32)(b - tables) >> 4;
    ab = (u8*)(pairs + (ia << 8) + ib);
    ba = (u8*)(pairs + (ib << 8) + ia);
    flags = (u8)(((u8)flags_a | *ab) & ~(u8)flags_b);
    *ab = flags;
    *ba = flags;

    if ((flags & 0x0F) == 0x0F || (flags & 0x40) != 0) {
        u32 first, second;
        if ((flags & 0x80) != 0) return;
        flags |= 0x80;
        *ab = flags;
        *ba = flags;
        first = func_00138738(world, a, b);
        second = func_00138738(world, b, a);
        if (first != 0 && second != 0) {
            *(u32*)(first + 8) = second;
            *(u32*)(second + 8) = first;
        } else {
            if (first != 0) (void)func_00138788(0, a, b);
            if (second != 0) (void)func_00138788(0, b, a);
            pool_push(world, 0x1AF0, first);
            pool_push(world, 0x1AF0, second);
            flags &= 0x7F;
            *ab = flags;
            *ba = flags;
        }
    } else if ((flags & 0x80) != 0) {
        u32 first, second, head;
        flags &= 0x7F;
        *ab = flags;
        *ba = flags;
        first = func_00138788(0, a, b);
        second = func_00138788(0, b, a);
        if (first == 0 || second == 0) return;
        head = *(u32*)first;
        if (head != 0) {
            const u64 notify_mask = (u64)1 << 63;
            u32 first_owner = *(u32*)(first + 0x0C);
            u32 second_owner = *(u32*)(second + 0x0C);
            free_contact_chain(world, head);
            if (first_owner != 0 &&
                (*(u64*)(first_owner + 0x538) & notify_mask) == 0)
                func_00129DF8((void*)first_owner);
            if (second_owner != 0 &&
                (*(u64*)(second_owner + 0x538) & notify_mask) == 0)
                func_00129DF8((void*)second_owner);
        }
        pool_push(world, 0x1AF0, first);
        pool_push(world, 0x1AF0, second);
    }
}

static u32 collider_accepts(u32 collider, u32 other)
{
    u32 descriptor = *(u32*)collider;
    u32 method = descriptor != 0 ? *(u32*)(descriptor + 0xF0) : 0;
    if (method < 0x00100000u || method >= 0x00220000u) return 0;
    return sly1_native_call_regs4(method, collider, other, 0, 0);
}

void func_00138F28(void* world_ptr, void* collider_ptr)
{
    u32 world = (u32)world_ptr;
    u32 collider = (u32)collider_ptr;
    u32 current;
    if (world == 0 || collider == 0) return;
    *(s32*)(collider + 0x4B8) = 0;
    current = *(u32*)(world + 0x1AE8);
    while (current != 0) {
        u32 other;
        if (*(u32*)(current + 4) != collider) {
            u32 accepts;
            other = *(u32*)(current + 4);
            accepts = other != 0 && collider_accepts(collider, other);
            if (!accepts && other != 0)
                accepts = collider_accepts(other, collider);
            func_001385A0(world_ptr, (void*)(collider + 0x480),
                          (void*)current, accepts ? 0 : 8,
                          accepts ? 8 : 0);
        }
        current = *(u32*)(current + 8);
    }
}
