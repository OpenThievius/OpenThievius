/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only GatherObjectsByMode backfill from the canonical 2026-11 decomp. */
#include "common.h"

typedef struct ModeObject {
    const u32* desc;
    u8 pad04[4];
    s32 id;
    struct ModeObject* next_hash;
    u8 pad10[4];
    struct ModeObject* world;
    struct ModeObject* parent;
    struct ModeObject* next_child;
    struct ModeObject* previous_child;
    struct ModeObject* next_by_id;
    u32 pad28;
    u32 pad2C;
    u32 alias;
} ModeObject;

typedef struct ModeGroupMember {
    ModeObject* object;
    u32 pad04;
    struct ModeGroupMember* next;
} ModeGroupMember;

extern void* func_00159F20(void* world, s32 id);
extern void sly1_native_trace_font(u32 stage, u32 a0, u32 a1, u32 a2,
                                  u32 a3, u32 a4);

static s32 mode_is_attached(const ModeObject* object)
{
    const ModeObject* node = object;
    for (u32 guard = 0; node != 0 && guard < 0x1000u; ++guard) {
        const ModeObject* parent = node->parent;
        u32 list;
        if (parent != 0) {
            list = (u32)parent + 0x34u;
        } else {
            const u32 world = *(const u32*)((const u8*)node + 0x14u);
            if (world == 0) return 0;
            list = world + 0x48u;
        }
        const s32 offset = *(const s32*)(list + 8u);
        if (offset < 0 || offset > 0x10000) return 0;
        u32 item = *(const u32*)list;
        s32 found = 0;
        for (u32 links = 0; item != 0 && links < 0x10000u; ++links) {
            if (item == (u32)node) { found = 1; break; }
            item = *(const u32*)(item + (u32)offset);
        }
        if (!found) return 0;
        node = parent;
    }
    return node == 0;
}

static void mode_collect(ModeObject* object, s32 mode, s32 include_detached,
                         s32 group_member, ModeObject* root, s32 max_count,
                         s32* count, ModeObject** found, s32* minimum_level)
{
    s32 include = 0;
    if (object == 0 || object->desc == 0) return;
    switch (mode) {
    case 1: {
        if (object->alias != 0 && !group_member) return;
        ModeObject* ancestor = object->parent;
        if (ancestor == 0) return;
        do {
            const u32 level = (u32)(*(const u64*)((const u8*)ancestor + 0x2C8u) >> 42) & 0xFu;
            if (level != 0) { include = level == 1; break; }
            if (ancestor->alias != 0) return;
            ancestor = ancestor->parent;
        } while (ancestor != 0);
        break;
    }
    case 2:
        if (object->alias != 0 && !group_member) return;
        include = object->parent == root;
        break;
    case 3:
        include = (object->desc[2] & 1u) != 0 &&
                  ((*(const u64*)((const u8*)object + 0x2C8u) & ((u64)0xF000u << 30)) != 0);
        break;
    case 4: {
        ModeObject* ancestor = object;
        if ((object->desc[2] & 1u) == 0) ancestor = object->parent;
        ModeObject* previous = 0;
        for (u32 guard = 0; ancestor != 0 && guard < 0x1000u; ++guard) {
            const u64 bits = *(const u64*)((const u8*)ancestor + 0x2C8u);
            if ((bits & ((u64)0xF000u << 30)) != 0) {
                const s32 level = (s32)(bits >> 42) & 0xFu;
                if (*minimum_level == level) include = 1;
                else if (level < *minimum_level) {
                    *minimum_level = level;
                    *count = 0;
                    *found = 0;
                    include = 1;
                }
                if (include) break;
            }
            if (ancestor == previous) return;
            if (ancestor->alias != 0 && (ancestor != object || !group_member)) {
                s32 wanted_alias = -1;
                for (ModeObject* r = root; r != 0; r = r->parent) {
                    if (r->alias != 0) {
                        wanted_alias = *(const s32*)(r->alias + 4u);
                        break;
                    }
                }
                if (*(const s32*)(ancestor->alias + 4u) != wanted_alias) return;
                previous = ancestor->parent;
            }
            ancestor = ancestor->parent;
        }
        if (!include && *minimum_level != 0x7FFFFFFF) return;
        break;
    }
    case 5:
        include = 1;
        break;
    default:
        return;
    }
    if (!include || (!include_detached && !mode_is_attached(object))) return;
    if (*count < max_count) found[*count] = object;
    ++*count;
}

s32 func_0015A198(void* world_pointer, s32 flags, s32 id, void* root_pointer,
                  s32 max_count, void* found_pointer)
{
    ModeObject* root = (ModeObject*)root_pointer;
    ModeObject** found = (ModeObject**)found_pointer;
    ModeObject* mark = root;
    s32 mode = flags & 0xFF;
    s32 count = 0;
    s32 minimum_level = 0x7FFFFFFF;
    if (id == -1) return 0;
    if (world_pointer == 0 || found_pointer == 0) return 0;

    if (mode == 1 || mode == 3 || mode == 4) {
        if (mark != 0 && (mark->desc[2] & 1u) == 0) mark = mark->parent;
        for (ModeObject* p = mark; p != 0; p = p->parent) {
            u64* flags_word = (u64*)((u8*)p + 0x2C8u);
            const u64 level = (u64)((*flags_word >> 42) & 0xFu);
            *flags_word = (*flags_word & ~((u64)0xFu << 42)) |
                          ((level + 1u) << 42);
        }
        if (root == 0) {
            if (mode == 3) return 0;
            mode = 5;
        }
    }

    u32 slot = (u32)func_00159F20(world_pointer, id);
    ModeObject* object = slot != 0 ? *(ModeObject**)slot : 0;
    for (u32 guard = 0; object != 0 && guard < 0x10000u; ++guard) {
        if (object->id == id && object->desc != 0) {
            if ((object->desc[2] & 0x100u) != 0) {
                ModeGroupMember* member = *(ModeGroupMember**)((u8*)object + 0x2D0u);
                for (u32 members = 0; member != 0 && members < 0x10000u;
                     ++members, member = member->next) {
                    mode_collect(member->object, mode, (flags & 0x100) != 0, 1,
                                 root, max_count, &count, found, &minimum_level);
                }
            } else if (object->alias == 0 ||
                       *(const s32*)(object->alias + 4u) != object->id) {
                mode_collect(object, mode, (flags & 0x100) != 0, 0,
                             root, max_count, &count, found, &minimum_level);
            }
        }
        object = object->next_hash;
    }

    for (ModeObject* p = mark; p != 0; p = p->parent) {
        u64* flags_word = (u64*)((u8*)p + 0x2C8u);
        *flags_word &= ~((u64)0xFu << 42);
    }
    if ((flags & 0x200) == 0 && max_count < count) count = max_count;
    if (id == 0x25F)
        sly1_native_trace_font(177u, (u32)world_pointer, (u32)flags,
                              (u32)id, (u32)count,
                              count > 0 && found != 0 ? (u32)found[0] : 0u);
    return count;
}
