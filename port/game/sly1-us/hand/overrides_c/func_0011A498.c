/* SPDX-License-Identifier: AGPL-3.0-or-later */
/*
 * Port-only diagnostic replacement for func_0011A498. Its lookup and 16-byte
 * result copy match the decomp C. The trace captures each parent before the
 * retail recursive walk dereferences its child slots.
 */
#include "common.h"

typedef struct Sly1KeyTree_11A498 {
    s32 count;
    struct Sly1KeyTree_11A498* children[4];
    struct Sly1KeyNode_11A498* head;
    s32 busy;
} Sly1KeyTree_11A498;

typedef struct Sly1KeyNode_11A498 {
    s32 key;
    u32 value;
    u32 pad8;
    struct Sly1KeyNode_11A498* next;
} Sly1KeyNode_11A498;

void sly1_native_trace_font(u32 stage, u32 a0, u32 a1, u32 a2,
                            u32 a3, u32 a4);

static void* find_key_11a498(Sly1KeyTree_11A498* tree, s32 key, s32 recurse,
                             u32 depth)
{
    Sly1KeyNode_11A498* node;
    void* found = 0;
    s32 index;

    sly1_native_trace_font(90u, (u32)tree, (u32)tree->count,
                           (u32)tree->children[0], (u32)tree->children[1],
                           (u32)tree->children[2]);
    sly1_native_trace_font(91u, (u32)tree->children[3], (u32)tree->head,
                           (u32)tree->busy, (u32)key, depth);

    tree->busy = 1;
    for (node = tree->head; node != 0; node = node->next) {
        if (node->key == key) {
            found = &node->value;
            goto done;
        }
    }
    if (recurse != 0) {
        for (index = 0; index < tree->count; ++index) {
            Sly1KeyTree_11A498* child = tree->children[index];
            if (child->busy == 0) {
                found = find_key_11a498(child, key, recurse, depth + 1u);
                if (found != 0) goto done;
            }
        }
    }
done:
    tree->busy = 0;
    return found;
}

s32 func_0011A498(void* tree, s32 key, s32 recurse, void* result)
{
    void* value = find_key_11a498((Sly1KeyTree_11A498*)tree, key, recurse, 0);
    if (value != 0) {
        if (result != 0) {
            u32* destination = (u32*)result;
            const u32* source = (const u32*)value;
            destination[0] = source[0];
            destination[1] = source[1];
            destination[2] = source[2];
            destination[3] = source[3];
        }
        return 1;
    }
    return 0;
}
