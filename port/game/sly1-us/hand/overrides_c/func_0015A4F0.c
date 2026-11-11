/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only compatibility translation of the canonical gathered-object search
 * at 0x0015A4F0. The adjacent func_0015A198 is a separate retail helper. */
#include "common.h"

typedef struct LookupBaseObject {
    const u32* desc;
    u8 pad04[4];
    s32 id;
    struct LookupBaseObject* next_hashed;
    u8 pad10[8];
    struct LookupBaseObject* parent;
    struct LookupBaseObject* next_child;
    u8 pad24[8];
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

typedef struct LookupWorld {
    u8 pad00[0x1884];
    LookupBaseObject* by_id[0x95];
} LookupWorld;

typedef struct LookupClassKindNode {
    struct LookupClassKindNode* next;
    s32 value;
} LookupClassKindNode;

extern const void* D_00247448[0x95];
extern s32 func_00182FC8(void* object);

static s32 lookup_is_kind(const LookupBaseObject* object, s32 wanted) {
    if (object == 0 || object->desc == 0) return 0;
    const u32 class_id = object->desc[1];
    if (class_id >= 0x95u) return 0;

    const LookupClassKindNode* node =
        ((const LookupClassKindNode* const*)D_00247448)[class_id];
    while (node != 0) {
        if (node->value == wanted) return 1;
        node = node->next;
    }
    return 0;
}

static s32 lookup_contains(const LookupPlacedObject* parent,
                          const LookupBaseObject* target) {
    if (parent == 0 || parent->child_link_offset < 0 ||
        parent->child_link_offset > 0x10000) return 0;
    u32 item = parent->child_head;
    for (u32 guard = 0; item != 0 && guard < 0x10000; ++guard) {
        if (item == (u32)target) return 1;
        item = *(const u32*)(item + (u32)parent->child_link_offset);
    }
    return 0;
}

static s32 lookup_is_attached(const LookupBaseObject* object) {
    const LookupBaseObject* node = object;
    for (u32 guard = 0; node != 0 && guard < 0x1000; ++guard) {
        const LookupBaseObject* parent = node->parent;
        if (parent != 0) {
            if (!lookup_contains((const LookupPlacedObject*)parent, node)) return 0;
        } else {
            const u32 world = *(const u32*)((const u8*)node + 0x14u);
            if (world == 0 || !lookup_contains((const LookupPlacedObject*)(world + 0x48u), node)) return 0;
        }
        node = parent;
    }
    return node == 0;
}

static void collect_lookup_object(LookupBaseObject* object, s32 mode,
                                  s32 include_detached, s32 root_member,
                                  LookupBaseObject* root, s32* count,
                                  LookupBaseObject** found) {
    s32 include = 0;
    if (object == 0) return;
    switch (mode) {
    case 1: {
        if (object->alias != 0 && !root_member) return;
        LookupBaseObject* ancestor = object->parent;
        while (ancestor != 0) {
            const u64 mark = ((const LookupPlacedObject*)ancestor)->flags;
            const u32 level = (u32)((mark >> 42) & 0xFu);
            if (level != 0) {
                include = level == 1;
                break;
            }
            if (ancestor->alias != 0) return;
            ancestor = ancestor->parent;
        }
        break;
    }
    case 2:
        if (object->alias != 0 && !root_member) return;
        include = object->parent == root;
        break;
    case 3:
        include = (object->desc[2] & 1u) != 0 &&
                  ((((const LookupPlacedObject*)object)->flags & 0x7800000000000000ull) != 0);
        break;
    case 4: {
        LookupBaseObject* ancestor = object;
        if ((object->desc[2] & 1u) == 0) ancestor = object->parent;
        for (u32 guard = 0; ancestor != 0 && guard < 0x1000; ++guard) {
            const u64 bits = ((const LookupPlacedObject*)ancestor)->flags;
            if ((bits & 0x7800000000000000ull) != 0) {
                include = 1;
                break;
            }
            if (ancestor->alias != 0 && (ancestor != object || !root_member)) {
                const s32 alias_id = *(const s32*)ancestor->alias;
                if (root == 0 || root->alias == 0 || alias_id != *(const s32*)root->alias)
                    return;
            }
            ancestor = ancestor->parent;
        }
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
    if (*count == 0) *found = object;
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
    if (mode == 1 || mode == 3 || mode == 4) {
        if (root == 0 && mode == 3) return 0;
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
        for (u32 guard = 0; object != 0 && guard < 0x10000; ++guard) {
            if (object->id == id && object->desc != 0) {
                if ((object->desc[2] & 0x100u) != 0) {
                    /* Object groups own a separate member-cell list at +0x2D0;
                     * not needed for notifier class 0x75, keep absent safely. */
                } else if (object->alias == 0 || *(const s32*)object->alias != object->id) {
                    collect_lookup_object(object, mode, (flags & 0x100u) != 0,
                                          0, root, &count, found);
                }
            }
            object = object->next_hashed;
        }
    }

    if ((flags & 0x200u) == 0 && max_count < count) count = max_count;
    return count;
}
