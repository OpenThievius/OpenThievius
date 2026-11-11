/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only compatibility translation of the canonical gathered-object search
 * at 0x0015A4F0. The adjacent func_0015A198 is a separate retail helper. */
#include "common.h"

typedef struct LookupBaseObject {
    const u32* desc;
    u8 pad04[4];
    s32 id;
    u8 pad0C[0x18 - 0x0C];
    struct LookupBaseObject* parent;
    struct LookupBaseObject* next_child;
    struct LookupBaseObject* next_by_id;
    u8 pad28[8];
    void* alias;
} LookupBaseObject;

typedef struct LookupPlacedObject {
    LookupBaseObject base;
    u32 child_head;
    u32 child_tail;
    s32 child_link_offset;
    u8 pad40[0x2C8 - 0x40];
    u64 flags;
} LookupPlacedObject;

typedef struct LookupGroupMember {
    LookupBaseObject* object;
    u32 pad04;
    struct LookupGroupMember* next;
} LookupGroupMember;

typedef struct LookupWorld {
    u8 pad00[0x1884];
    LookupBaseObject* by_id[0x95];
} LookupWorld;

typedef struct LookupClassKindNode {
    struct LookupClassKindNode* next;
    s32 value;
} LookupClassKindNode;

extern const void* D_00247448[0x95];
extern void sly1_native_trace_font(u32 stage, u32 a0, u32 a1, u32 a2, u32 a3, u32 a4);

static s32 lookup_is_in_list(u32 list, const LookupBaseObject* target) {
    if (list == 0) return 0;
    u32 item = *(const u32*)list;
    const s32 offset = *(const s32*)(list + 8u);
    if (offset < 0 || offset > 0x10000) return 0;
    for (u32 guard = 0; item != 0 && guard < 0x10000; ++guard) {
        if (item == (u32)target) return 1;
        item = *(const u32*)(item + (u32)offset);
    }
    return 0;
}

static s32 lookup_is_attached(const LookupBaseObject* object) {
    const LookupBaseObject* node = object;
    for (u32 guard = 0; node != 0 && guard < 0x1000; ++guard) {
        const LookupBaseObject* parent = node->parent;
        if (parent != 0) {
            if (!lookup_is_in_list((u32)parent + 0x34u, node)) return 0;
        } else {
            const u32 world = *(const u32*)((const u8*)node + 0x14u);
            if (world == 0 || !lookup_is_in_list(world + 0x48u, node)) return 0;
        }
        node = parent;
    }
    return node == 0;
}

static void collect_lookup_object(LookupBaseObject* object, s32 mode,
                                  s32 include_detached, s32 group_member,
                                  LookupBaseObject* root, s32 max_count,
                                  s32* count, LookupBaseObject** found,
                                  s32* min_level) {
    s32 include = 0;
    if (object == 0) return;
    switch (mode) {
    case 1: {
        if (object->alias != 0 && !group_member) return;
        LookupBaseObject* ancestor = object->parent;
        if (ancestor == 0) return;
        do {
            const u32 level = (u32)(((const LookupPlacedObject*)ancestor)->flags >> 42) & 0xFu;
            if (level != 0) {
                include = level == 1;
                break;
            }
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
                  ((((const LookupPlacedObject*)object)->flags & ((u64)0xF000u << 30)) != 0);
        break;
    case 4: {
        LookupBaseObject* ancestor = object;
        if ((object->desc[2] & 1u) == 0) ancestor = object->parent;
        LookupBaseObject* previous = 0;
        for (u32 guard = 0; ancestor != 0 && guard < 0x1000; ++guard) {
            const u64 bits = ((const LookupPlacedObject*)ancestor)->flags;
            if ((bits & ((u64)0xF000u << 30)) != 0) {
                const s32 level = (s32)(bits >> 42) & 0xFu;
                if (*min_level == level) {
                    include = 1;
                } else if (level < *min_level) {
                    *min_level = level;
                    *count = 0;
                    *found = 0;
                    include = 1;
                }
                if (include) break;
            }
            if (ancestor == previous) return;
            if (ancestor->alias != 0 && (ancestor != object || !group_member)) {
                s32 wanted_alias = -1;
                for (LookupBaseObject* r = root; r != 0; r = r->parent) {
                    if (r->alias != 0) {
                        wanted_alias = *(const s32*)((const u8*)r->alias + 4u);
                        break;
                    }
                }
                if (*(const s32*)((const u8*)ancestor->alias + 4u) != wanted_alias) return;
                previous = ancestor->parent;
            }
            ancestor = ancestor->parent;
        }
        if (!include && *min_level != 0x7FFFFFFF) return;
        break;
    }
    case 5:
        include = 1;
        break;
    default:
        return;
    }
    if (!include) return;
    if (!include_detached && !lookup_is_attached(object)) return;
    if (*count < max_count) found[*count] = object;
    ++*count;
}

s32 func_0015A4F0(void* world_ptr, u32 flags, s32 id, void* root_ptr,
                  s32 max_count, void* found_ptr) {
    if (world_ptr == 0 || found_ptr == 0 || id == -1) return 0;
    if (root_ptr == 0 && (flags & 5u) == 0) return 0;

    s32 mode = (s32)(flags & 0xFFu);
    LookupBaseObject* root = (LookupBaseObject*)root_ptr;
    LookupBaseObject** found = (LookupBaseObject**)found_ptr;
    s32 count = 0;
    s32 min_level = 0x7FFFFFFF;
    LookupBaseObject* mark = root;
    if (id == 0x75 && (flags & 0xFFu) == 1u) {
        sly1_native_trace_font(170u, (u32)world_ptr, flags, (u32)id,
                              (u32)root_ptr, (u32)max_count);
    }
    if (mode == 1 || mode == 3 || mode == 4) {
        if (mark != 0 && (mark->desc[2] & 1u) == 0) mark = mark->parent;
        s32 level = 0;
        for (LookupBaseObject* p = mark; p != 0; p = p->parent) {
            ++level;
            LookupPlacedObject* placed = (LookupPlacedObject*)p;
            placed->flags = (placed->flags & ~((u64)0xFu << 42)) | ((u64)level << 42);
        }
        if (root == 0) {
            if (mode == 3) return 0;
            mode = 5;
        }
    }

    LookupWorld* world = (LookupWorld*)world_ptr;
    const u32 start = id < 0 ? 0u : (u32)id;
    for (u32 i = start; i < 0x95u; ++i) {
        const LookupClassKindNode* class_node =
            ((const LookupClassKindNode* const*)D_00247448)[i];
        s32 accepted = 0;
        while (class_node != 0) {
            if (class_node->value == id) { accepted = 1; break; }
            class_node = class_node->next;
        }
        if (!accepted) break;

        LookupBaseObject* object = world->by_id[i];
        if (id == 0x75 && i == 0x75u) {
            sly1_native_trace_font(171u, (u32)class_node,
                                  class_node != 0 ? (u32)class_node->value : 0u,
                                  (u32)object, (u32)root, (u32)mode);
        }
        for (u32 guard = 0; object != 0 && guard < 0x10000; ++guard) {
            if (object->desc != 0) {
                if (id == 0x75 && i == 0x75u) {
                    sly1_native_trace_font(172u, (u32)object, (u32)object->id,
                        (u32)object->parent, (u32)object->desc[2],
                        (u32)lookup_is_attached(object));
                }
                if ((object->desc[2] & 0x100u) != 0) {
                    LookupGroupMember* member = *(LookupGroupMember**)((u8*)object + 0x2D0u);
                    for (u32 member_guard = 0; member != 0 && member_guard < 0x10000u;
                         ++member_guard, member = member->next) {
                        collect_lookup_object(member->object, mode, (flags & 0x100u) != 0,
                                              1, root, max_count, &count, found, &min_level);
                    }
                } else if (object->alias == 0 || *(const s32*)object->alias != object->id) {
                    collect_lookup_object(object, mode, (flags & 0x100u) != 0,
                                          0, root, max_count, &count, found, &min_level);
                }
            }
            object = object->next_by_id;
        }
    }

    for (LookupBaseObject* p = mark; p != 0; p = p->parent) {
        LookupPlacedObject* placed = (LookupPlacedObject*)p;
        placed->flags &= ~((u64)0xFu << 42);
    }
    if ((flags & 0x200u) == 0 && max_count < count) count = max_count;
    if (id == 0x75 && (flags & 0xFFu) == 1u) {
        sly1_native_trace_font(173u, (u32)count,
            count > 0 ? (u32)found[0] : 0u, (u32)min_level,
            (u32)mode, (u32)flags);
    }
    return count;
}
