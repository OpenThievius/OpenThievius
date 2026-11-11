/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only C trace of the retail frame-object transfer routine. */
#include "common.h"

typedef struct NativeList_139EC8 {
    u32 head;
    u32 tail;
    s32 link_offset;
} NativeList_139EC8;

typedef struct NativeLink_139EC8 {
    u32 next;
    u32 previous;
} NativeLink_139EC8;

typedef struct NativeListWalkFrame_139EC8 {
    u32 list;
    u32 link;
    s32 offset;
    u32 previous;
} NativeListWalkFrame_139EC8;

extern u32 D_0027B314;
void func_00139D28(void* node);
void func_00152600(void* list, void* node);
void func_00152470(void* list, void* node);
void func_001243E0(void* node);
void sly1_native_trace_font(u32 stage, u32 a0, u32 a1, u32 a2, u32 a3,
                            u32 a4);

static void trace_frame_node(u32 node, u32 phase)
{
    u32* link = (u32*)(node + 0x630u);
    u32 head = *(u32*)0x002622E0u;
    u32 head_next = head != 0 ? *(u32*)(head + 0x630u) : 0;
    sly1_native_trace_font(58u, node, head_next, link[0], link[1], phase);
}

void func_00139EC8(void* node_pointer)
{
    u32 node = (u32)node_pointer;
    NativeListWalkFrame_139EC8 iterator;
    NativeList_139EC8* children = (NativeList_139EC8*)(node + 0x624u);
    u32 link_offset = (u32)children->link_offset;

    iterator.list = (u32)children;
    iterator.link = (u32)children;
    iterator.offset = (s32)link_offset;
    iterator.previous = D_0027B314;
    D_0027B314 = (u32)&iterator;
    for (;;) {
        u32 child = *(u32*)iterator.link;
        if (child == 0) break;
        iterator.link = child + link_offset;
        func_00139D28((void*)child);
    }
    D_0027B314 = iterator.previous;

    func_00152600((void*)(*(u32*)(node + 0x14u) + 0x1CB4u), node_pointer);
    func_00152470((void*)0x002622E0u, node_pointer);
    trace_frame_node(node, 1u);
    func_001243E0(node_pointer);
    trace_frame_node(node, 2u);
}
