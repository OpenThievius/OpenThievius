/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only copy of retail RemoveFromList with a gated frame-link trace. */
#include "common.h"

typedef struct NativeListFrame_152600 {
    u32 list;
    u32 link;
    s32 offset;
    u32 previous;
} NativeListFrame_152600;

extern u32 D_0027B314;
void sly1_native_trace_list_remove(u32 list, u32 item, s32 offset,
                                   u32 next, u32 previous, u32 phase);

static u32 list_link_address(u32 list, u32 item)
{
    return item + *(s32*)(list + 8u);
}

void func_00152600(void* list_pointer, void* item_pointer)
{
    u32 list = (u32)list_pointer;
    u32 item = (u32)item_pointer;
    s32 offset = *(s32*)(list + 8u);
    u32 link = list_link_address(list, item);
    u32 next = *(u32*)link;
    u32 previous = *(u32*)(link + 4u);
    u32 frame = D_0027B314;

    sly1_native_trace_list_remove(list, item, offset, next, previous, 0u);
    while (frame != 0) {
        NativeListFrame_152600* it = (NativeListFrame_152600*)frame;
        if (it->link == link) {
            it->link = previous != 0 ? list_link_address(list, previous) : list;
        }
        frame = it->previous;
    }
    if (previous != 0) *(u32*)list_link_address(list, previous) = next;
    else *(u32*)list = next;
    if (next != 0) *(u32*)(list_link_address(list, next) + 4u) = previous;
    else *(u32*)(list + 4u) = previous;
    *(u32*)link = 0;
    *(u32*)(link + 4u) = 0;
    sly1_native_trace_list_remove(list, item, offset, 0u, 0u, 1u);
}
