/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only Sly UI frame dispatch, kept outside matching C. */
#include "common.h"

extern u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);

static void dispatch_ui_slot(u32 slot_offset) {
    s32 index;
    for (index = 0; index < 0x25; ++index) {
        u32 object = *(u32*)(0x002486B0u + (u32)index * 4u);
        u32 vtable;
        u32 method;
        if (object == 0 || *(s32*)(object + 0x250u) == 0) continue;
        vtable = *(u32*)object;
        method = vtable != 0 ? *(u32*)(vtable + slot_offset) : 0;
        if (method != 0)
            (void)sly1_native_call_regs4(method, object, 0, 0, 0);
    }
}

void func_001AA110(void) {
    dispatch_ui_slot(0x28); /* virtual slot 10: update */
}

void func_001AA178(void) {
    dispatch_ui_slot(0x24); /* virtual slot 9: draw */
    (void)sly1_native_call_regs4(0x001EFEE0u, 0x00275F80u, 0, 0, 0);
    (void)sly1_native_call_regs4(0x001AE820u, 0x00273EB0u, 0, 0, 0);
}

void func_001E9668(void) {
    func_001AA110();
}

void func_001E9648(void) {
    func_001AA178();
}
