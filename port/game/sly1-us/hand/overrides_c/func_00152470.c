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
void sly1_native_loader_stage(u32 stage, u32 object, u32 reader);
void sly1_native_trace_list_append(u32 list, u32 node, s32 offset,
                                  u32 head, u32 tail, u32 phase);

void func_00152470(void* list_pointer, void* node_pointer)
{
    NativeList_152470* list = (NativeList_152470*)list_pointer;
    const u32 node = (u32)node_pointer;
    NativeListLink_152470* link;

    /* Port guard for malformed resource records. Retail assumes both values
     * point into EE RAM; reject sentinels before forming a host guest-memory
     * reference, while preserving the normal list operation. */
    if ((u32)list_pointer < 0x10000u || (u32)list_pointer >= 0x02000000u ||
        node < 0x10000u || node >= 0x02000000u ||
        list->link_offset < 0 || list->link_offset > 0x10000) {
        sly1_native_loader_stage(157, node, (u32)list_pointer);
        return;
    }
    sly1_native_trace_list_append((u32)list, node, list->link_offset,
                                  list->head, list->tail, 0u);
    if (list->tail != 0 &&
        (list->tail < 0x10000u || list->tail >= 0x02000000u ||
         list->tail > 0x02000000u - (u32)list->link_offset - 8u)) {
        /* Some class constructors leave a sentinel in this list field; make
         * the first valid append establish a fresh head/tail pair. */
        list->head = 0;
        list->tail = 0;
    }
    link = (NativeListLink_152470*)(node + (u32)list->link_offset);

    if (list->tail == 0) {
        list->head = node;
    } else {
        NativeListLink_152470* tail_link = (NativeListLink_152470*)
            (list->tail + (u32)list->link_offset);
        link->previous = list->tail;
        tail_link->next = node;
    }
    list->tail = node;
    sly1_native_trace_list_append((u32)list, node, list->link_offset,
                                  list->head, list->tail, 1u);

    if ((u32)list_pointer == 0x002622E0u) {
        sly1_native_trace_font(56u, (u32)list_pointer, node,
            (u32)list->link_offset, list->head, list->tail);
    }
}
