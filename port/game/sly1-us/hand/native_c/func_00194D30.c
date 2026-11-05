/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only startup menu routing, kept outside matching C. */
#include "common.h"

extern u8 D_00275BC0[];
extern void func_001E9498(void* stack, void* node);
extern u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);

void func_001BE998(void);

static void menu_page_transition(void* object, s32 old_page, s32 new_page) {
    s32 sound = *(s32*)((u8*)object + 0x288);

    if (old_page == new_page) return;
    if (sound != 0) {
        func_001BE998();
        *(s32*)((u8*)object + 0x288) = 0;
    }

    /* The first boot transition is from page -1 to page 0. Other page-specific
     * audio and caption effects remain in the retail func_00193EE8 path. */
}

static void menu_set_page(void* object, s32 page) {
    if (*(s32*)((u8*)object + 0x260) == page) return;
    *(s32*)((u8*)object + 0x260) = page;
    if (page != -1) {
        (void)sly1_native_call_regs4(0x001936A0u, (u32)object,
                                     (u32)page, 0, 0);
    }
}

void func_00194D30(void* object, s32 layer, s32 value) {
    s32 target = value;
    s32 current;
    s32 mode;
    s32 i;
    u8* bytes = (u8*)object;

    if (layer < 0 || layer >= 3) return;
    *(s32*)(bytes + 0x268 + layer * 4) = value;
    for (i = 0; i < 3; ++i) {
        s32 requested = *(s32*)(bytes + 0x268 + i * 4);
        if (requested != -1) target = requested;
    }

    current = *(s32*)(bytes + (*(s32*)(bytes + 0x274) != 0 ? 0x278 : 0x260));
    menu_page_transition(object, current, target);
    mode = *(s32*)(bytes + 0x250);

    if (current != target) {
        if (mode == 0) {
            menu_set_page(object, target);
        } else if (mode >= 0 && mode < 4) {
            if (target == -1) {
                *(s32*)(bytes + 0x274) = 0;
                *(s32*)(bytes + 0x278) = -1;
            } else {
                *(s32*)(bytes + 0x278) = target;
                *(s32*)(bytes + 0x274) = 1;
                u32 vtable = *(u32*)object;
                u32 method = vtable != 0 ? *(u32*)(vtable + 0x40) : 0;
                if (method != 0)
                    (void)sly1_native_call_regs4(method, (u32)object, 3, 0, 0);
            }
        }
    }

    if (target != -1 && *(s32*)(bytes + 0x25C) == 0)
        func_001E9498(D_00275BC0, object);
}
