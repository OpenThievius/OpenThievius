/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only backfill copied from the current NTSC-U decompilation. */
#include "common.h"

extern void* D_00275710;
void func_00144008(void* object);
void func_001EEBB0(void* context, void* input, s32* output);
void sly1_native_trace_font(u32 stage, u32 a0, u32 a1, u32 a2, u32 a3,
                            u32 a4);

void func_00144270(void* object) {
    u8* bytes = (u8*)object;
    void* root;
    u32 input[4];
    u32 context = 0;
    u32 linked_items = 0;
    /* The transform helper is still unavailable in the port; default its
     * output so the existing no-op guest stub cannot leak an indeterminate
     * stack value into the camera object. */
    s32 output = 0;

    func_00144008(object);
    /* Retail loads D_00275710 itself as the current-world pointer. */
    root = D_00275710;
    if ((u32)root >= 0x00100000u && (u32)root < 0x02000000u)
        context = *(u32*)((u8*)root + 0x1EC4u);
    if (context >= 0x00100000u && context < 0x02000000u)
        linked_items = *(u32*)(context + 0x38u);
    sly1_native_trace_font(153, (u32)object, (u32)D_00275710, (u32)root,
                           context, linked_items);
    if ((u32)root < 0x00100000u || (u32)root >= 0x02000000u) return;
    /* The native loader can expose an uninitialized collision pointer here.
     * Retail reaches this helper only with a guest EE object. */
    if (context != 0 &&
        (context < 0x00100000u || context >= 0x02000000u)) return;
    /* The loaded value-list object can be only partially initialized when
     * the canonical loader leaves its linked-item pointer unset. */
    if (context != 0 && linked_items != 0 &&
        (linked_items & 0x80000000u) == 0 &&
        (linked_items < 0x00100000u || linked_items >= 0x02000000u)) return;

    __builtin_memcpy(input, bytes + 0x40, sizeof(input));
    func_001EEBB0((void*)context, input, &output);
    if (output != 0) *(s32*)(bytes + 0x21C) = output;
}
