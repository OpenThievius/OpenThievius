/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only translations of Sly's generic intrusive-list primitives. */
#include "common.h"

typedef struct NativeList {
    void* head;
    void* tail;
    s32 offset;
} NativeList;

typedef struct NativeListLink {
    void* next;
    void* previous;
} NativeListLink;

extern u32 D_0027B314;
void sly1_native_trace_font(u32 stage, u32 a0, u32 a1, u32 a2, u32 a3,
                            u32 a4);

void func_00152438(NativeList* list, s32 link_offset)
{
    list->offset = link_offset;
}

void func_00152440(NativeList* list)
{
    list->head = 0;
    list->tail = 0;
}

static NativeListLink* node_link(NativeList* list, void* node)
{
    return (NativeListLink*)((u8*)node + list->offset);
}

u32 func_00152460(NativeList* list, void* node)
{
    return (u32)node + (u32)list->offset;
}

void func_00152600(NativeList* list, void* node)
{
    static u32 trace_count;
    NativeListLink* link;
    if (trace_count < 4096u) {
        sly1_native_trace_font(55u, (u32)list, (u32)node,
            D_0027B314, 0u, trace_count);
        ++trace_count;
    }
    if (list == 0 || node == 0) return;
    link = node_link(list, node);
    if (link->previous == 0) list->head = link->next;
    else node_link(list, link->previous)->next = link->next;
    if (link->next == 0) list->tail = link->previous;
    else node_link(list, link->next)->previous = link->previous;
    link->next = 0;
    link->previous = 0;
}

s32 func_00152720(NativeList* list)
{
    return list->head == 0;
}
