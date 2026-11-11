/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only Sly UI frame dispatch, kept outside matching C. */
#include "common.h"

extern u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);
extern void sly1_native_trace_font(u32 stage, u32 a0, u32 a1, u32 a2,
                                   u32 a3, u32 a4);

static void dispatch_ui_slot(u32 slot_offset) {
    s32 index;
    for (index = 0; index < 0x25; ++index) {
        u32 object = *(u32*)(0x002486B0u + (u32)index * 4u);
        u32 vtable;
        u32 method;
        if (object == 0 || *(s32*)(object + 0x250u) == 0) continue;
        vtable = *(u32*)object;
        method = vtable != 0 ? *(u32*)(vtable + slot_offset) : 0;
        if (method != 0) {
            sly1_native_trace_font(188u, (u32)index, object,
                                   *(u32*)(object + 0x250u), vtable, method);
            if (method < 0x00100000u || method >= 0x00211E10u ||
                (method & 3u) != 0) {
                sly1_native_trace_font(189u, (u32)index, object, vtable,
                                       method, slot_offset);
                continue;
            }
            (void)sly1_native_call_regs4(method, object, 0, 0, 0);
        }
    }
}

void func_001AA110(void) {
    sly1_native_trace_font(195u, 0x001AA110u, 0, 0, 0, 0);
    dispatch_ui_slot(0x28); /* virtual slot 10: update */
}

void func_001AA178(void) {
    sly1_native_trace_font(196u, 0x001AA178u, 0, 0, 0, 0);
    sly1_native_trace_font(187u, *(u32*)0x002486B0u,
                           *(u32*)0x002486B4u,
                           *(u32*)0x002721D0u,
                           *(u32*)0x002701F8u, 0u);
    dispatch_ui_slot(0x24); /* virtual slot 9: draw */
    (void)sly1_native_call_regs4(0x001EFEE0u, 0x00275F80u, 0, 0, 0);
    (void)sly1_native_call_regs4(0x001AE820u, 0x00273EB0u, 0, 0, 0);
}

void func_001E9668(void) {
    sly1_native_trace_font(197u, 0x001E9668u, 0, 0, 0, 0);
    func_001AA110();
}

void func_001E9648(void) {
    sly1_native_trace_font(198u, 0x001E9648u, 0, 0, 0, 0);
    func_001AA178();
}
