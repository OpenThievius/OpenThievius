/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only state-notifier transition backfill. Replace with matched C when
 * func_00147AB0 is hostable from sly1-decomp. */
#include "common.h"

extern u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1,
                                  u32 a2, u32 a3);
extern float sly1_native_call_f32(u32 address, u32 a0, u32 a1,
                                  u32 a2, u32 a3, float f12, float f14);
extern void sly1_native_trace_font(u32 stage, u32 a0, u32 a1, u32 a2,
                                  u32 a3, u32 a4);

static void notifier_call_method(u32 object, u32 slot, u32 arg1)
{
    u32 descriptor;
    u32 method;
    if (object == 0) return;
    descriptor = *(u32*)object;
    if (descriptor < 0x10000u || descriptor >= 0x02000000u) return;
    method = *(u32*)(descriptor + 0x10u + slot * 4u);
    sly1_native_trace_font(175u, object, descriptor, slot, method, arg1);
    if (method == 0) return;
    (void)sly1_native_call_regs4(method, object, arg1, 0, 0);
}

void func_00147AB0(void* self_pointer, s32 state)
{
    u8* self = (u8*)self_pointer;
    s32 old_state;
    s32 event = -1;
    u32 owner;
    u32 walker;

    if (self == 0) return;
    old_state = *(s32*)(self + 0x2D0u);
    if (old_state == state) return;

    if (state != 0 && state != 6)
        notifier_call_method((u32)self, 0x15u, 0u);

    /* Preserve the source's transition snapshot for the states used by the
     * notifier and camera state machines. */
    if (old_state == -1 || old_state == 2 || old_state == 4 || old_state == 5) {
        {
            u32 i;
            for (i = 0; i < 4; ++i) {
                ((u32*)(self + 0x2F0u))[i] = ((u32*)(self + 0x100u))[i];
                ((u32*)(self + 0x300u))[i] = ((u32*)(self + 0x150u))[i];
            }
        }
    }

    switch (state) {
    case 0:
        {
            u32 i;
            for (i = 0; i < 4; ++i)
                ((u32*)(self + 0x310u))[i] = ((u32*)0x00248D30u)[i];
        }
        event = *(s32*)(self + 0x2DCu);
        if (event == -1) event = 0x261;
        break;
    case 2:
        if (*(s32*)(self + 0x334u) == 0)
            *(s32*)(self + 0x338u) = 1;
        notifier_call_method((u32)self, 3u, 0u);
        break;
    case 3:
        {
            u32 i;
            for (i = 0; i < 4; ++i)
                ((u32*)(self + 0x310u))[i] = ((u32*)0x002619F0u)[i];
        }
        event = 0x260;
        break;
    case 6:
        {
            u32 i;
            for (i = 0; i < 4; ++i)
                ((u32*)(self + 0x310u))[i] = ((u32*)0x00248D30u)[i];
        }
        event = 0x263;
        break;
    case 7:
        {
            u32 i;
            for (i = 0; i < 4; ++i)
                ((u32*)(self + 0x310u))[i] = ((u32*)0x00248D30u)[i];
        }
        event = 0x262;
        break;
    default:
        break;
    }

    owner = *(u32*)(self + 0x2ECu);
    if (state == 0 || state == 6)
        notifier_call_method(owner, 2u, 0u);
    else if (owner != 0)
        notifier_call_method(owner, 3u, 0u);

    *(s32*)(self + 0x2D0u) = state;
    *(float*)(self + 0x2D4u) = *(float*)0x0026185Cu;

    walker = *(u32*)(self + 0x2E8u);
    if (event != -1) {
        if (walker == 0) {
            walker = sly1_native_call_regs4(0x001B6290u,
                *(u32*)(self + 0x2E4u), (u32)self, (u32)event, 0);
            *(u32*)(self + 0x2E8u) = walker;
            if (*(float*)(self + 0x2E0u) > 0.0f && walker != 0) {
                (void)sly1_native_call_f32(0x001B6CF0u, walker, 0, 0, 0,
                                           *(float*)(self + 0x2E0u), 0.0f);
            }
        } else {
            (void)sly1_native_call_regs4(0x001B64D8u, walker,
                                         (u32)event, 0, 0);
        }
    } else if (walker != 0) {
        (void)sly1_native_call_regs4(0x001B6438u, walker, 0, 0, 0);
        *(u32*)(self + 0x2E8u) = 0;
    }

    sly1_native_trace_font(174u, (u32)self, (u32)old_state,
                           (u32)state, owner, (u32)event);
}
