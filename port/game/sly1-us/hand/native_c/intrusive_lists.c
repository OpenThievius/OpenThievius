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

void func_00152470(NativeList* list, void* node)
{
    NativeListLink* link = node_link(list, node);
    if (list->tail == 0) {
        list->head = node;
    } else {
        NativeListLink* tail_link = node_link(list, list->tail);
        link->previous = list->tail;
        tail_link->next = node;
    }
    list->tail = node;
}

s32 func_00152720(NativeList* list)
{
    return list->head == 0;
}
