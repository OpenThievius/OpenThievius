/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only world-list update dispatcher, kept separate from matching C. */
#include "common.h"

extern u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);
u32 sly1_native_vu0_mac(void);
u32 sly1_native_vu0_component(u32 index);
void sly1_native_trace_font(u32 stage, u32 a0, u32 a1, u32 a2, u32 a3,
                            u32 a4);
extern void* func_0018D500(s32 size);

/* D_0026AC10's retail cursor uses only its first word. Its known-zero padding
 * carries the reusable guest frame slab and recursion depth for this port. */
#define SLY1_UI_LIST_SCRATCH 0x0026AC14u
#define SLY1_UI_LIST_DEPTH   0x0026AC18u

static u32* push_frame(u32 address, u32 list, s32 link) {
    u32* frame = (u32*)address;
    frame[0] = list;
    frame[1] = 0;
    frame[2] = (u32)link;
    frame[3] = *(u32*)0x0027B314u;
    *(u32*)0x0027B314u = address;
    return frame;
}

static void pop_frame(u32* frame) {
    *(u32*)0x0027B314u = frame[3];
}

static void update_actor_list(u32 world, u32 frame_address, u32 list_offset,
                              u32 argument) {
    u32 list = world + list_offset;
    s32 link = *(s32*)(list + 8u);
    u32* frame = push_frame(frame_address, list, link);
    u32 node_address = *(u32*)list;
    frame[1] = node_address + (u32)link;

    while (node_address != 0) {
        u32 object = *(u32*)node_address;
        if (object != 0) {
            u32 method = *(u32*)(object + 0xA0u);
            sly1_native_trace_font(35, node_address, object, method,
                                   list_offset, argument);
            if (method == 0x00125C90u) {
                sly1_native_trace_font(41, node_address, argument,
                                       *(u32*)(node_address + 0x140u),
                                       *(u32*)(node_address + 0x144u),
                                       *(u32*)(node_address + 0x148u));
                sly1_native_trace_font(42, node_address,
                                       *(u32*)(node_address + 0x80u),
                                       *(u32*)(node_address + 0x2A8u),
                                       *(u32*)(argument + 0x40u),
                                       *(u32*)(argument + 0x44u));
            }
            sly1_native_trace_font(36, node_address,
                                   *(u32*)(node_address + 0x2C8u),
                                   *(u32*)(node_address + 0x2CCu),
                                   *(u32*)(node_address + 0x28Cu),
                                   *(u32*)(node_address + 0x298u));
            if (method != 0)
                (void)sly1_native_call_regs4(method, node_address,
                                             argument, 0, 0);
            if (method == 0x00125C90u)
                sly1_native_trace_font(45, node_address, object, method,
                                       sly1_native_vu0_mac(),
                                       *(u32*)(node_address + 0x2C8u));
            if (method == 0x00125C90u) {
                u32 lane;
                for (lane = 0; lane < 4; ++lane)
                    sly1_native_trace_font(37 + lane, object, lane,
                                           sly1_native_vu0_component(128 + lane),
                                           sly1_native_vu0_component(24 + lane),
                                           sly1_native_vu0_component(28 + lane));
            }
        }
        node_address = *(u32*)frame[1];
        frame[1] = node_address + (u32)link;
    }
    pop_frame(frame);
}

static void update_shape_list(u32 world, u32 frame_address, u32 list_offset) {
    u32 list = world + list_offset;
    s32 link = *(s32*)(list + 8u);
    u32* frame = push_frame(frame_address, list, link);
    u32 node = *(u32*)list;
    frame[1] = node + (u32)link;
    while (node != 0) {
        (void)sly1_native_call_regs4(0x00159D00u, node, 0, 0, 0);
        node = *(u32*)frame[1];
        frame[1] = node + (u32)link;
    }
    pop_frame(frame);
}

void func_0019D6C0(void* world_object, void* argument) {
    u32 world = (u32)world_object;
    u32 frame_address;
    u32* scratch = (u32*)SLY1_UI_LIST_SCRATCH;
    s32* depth_slot = (s32*)SLY1_UI_LIST_DEPTH;
    s32 depth = (*depth_slot)++;

    if (*scratch == 0) {
        *scratch = (u32)func_0018D500(0x30 * 64);
    }
    if (*scratch == 0 || depth >= 64) {
        --*depth_slot;
        return;
    }

    frame_address = *scratch + (u32)depth * 0x30u;
    *(u32*)0x0026AC10u = 0x005E4700u;
    update_actor_list(world, frame_address, 0x60u, (u32)argument);
    update_actor_list(world, frame_address + 0x10u, 0x78u, (u32)argument);
    update_shape_list(world, frame_address + 0x20u, 0x1CD8u);
    --*depth_slot;
}
