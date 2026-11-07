/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only equivalent of the matched intrusive-list append, with a queue probe. */
#include "common.h"

typedef struct NativeList_152470 {
    u32 head;
    u32 tail;
    s32 link_offset;
} NativeList_152470;

typedef struct NativeListLink_152470 {
    u32 next;
    u32 previous;
} NativeListLink_152470;

void sly1_native_trace_font(u32 stage, u32 a0, u32 a1, u32 a2, u32 a3,
                            u32 a4);

void func_00152470(void* list_pointer, void* node_pointer)
{
    NativeList_152470* list = (NativeList_152470*)list_pointer;
    const u32 node = (u32)node_pointer;
    NativeListLink_152470* link = (NativeListLink_152470*)
        (node + (u32)list->link_offset);

    if (list->tail == 0) {
        list->head = node;
    } else {
        NativeListLink_152470* tail_link = (NativeListLink_152470*)
            (list->tail + (u32)list->link_offset);
        link->previous = list->tail;
        tail_link->next = node;
    }
    list->tail = node;

    if ((u32)list_pointer == 0x002622E0u) {
        sly1_native_trace_font(56u, (u32)list_pointer, node,
            (u32)list->link_offset, list->head, list->tail);
    }
}
