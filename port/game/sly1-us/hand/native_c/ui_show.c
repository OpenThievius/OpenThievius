/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only UI show/mode routines, kept separate from matching C. */
#include "common.h"

extern u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);

void func_00194F00(void* object);
void func_00194E60(void* object, s32 mode);

void func_001AA8F0(void* object) {
    u8* self = (u8*)object;
    s32 mode = *(s32*)(self + 0x250);
    if (mode == 2) {
        u32 clock = *(u32*)(self + 0x258);
        if (clock != 0) *(f32*)(self + 0x254) = *(f32*)clock;
    } else if (mode == 0 || mode == 3) {
        func_00194E60(object, 1);
    }
}

void func_00194F00(void* object) {
    u8* self = (u8*)object;
    if (*(s32*)(self + 0x250) == 3 && *(s32*)(self + 0x274) != 0) return;
    func_001AA8F0(object);
}

void func_001AA998(void* object, s32 mode) {
    u8* self = (u8*)object;
    s32 old_mode = *(s32*)(self + 0x250);
    s32 notify = 0;
    if (old_mode == mode) return;

    switch (mode) {
        case 1:
            *(f32*)(self + 0x24C) = *(f32*)(self + 0x23C) * *(f32*)(self + 0x240);
            if (old_mode == 0 || old_mode == 2) notify = 1;
            break;
        case 3:
            *(f32*)(self + 0x24C) = (1.0f - *(f32*)(self + 0x23C)) * *(f32*)(self + 0x248);
            if (old_mode == 0 || old_mode == 2) notify = 1;
            break;
        case 2:
            notify = 1;
            *(f32*)(self + 0x21C) = *(f32*)(self + 0x224);
            *(f32*)(self + 0x220) = *(f32*)(self + 0x228);
            *(f32*)(self + 0x23C) = 1.0f;
            *(f32*)(self + 0x24C) = 0.0f;
            break;
        case 0:
            notify = 1;
            *(f32*)(self + 0x21C) = *(f32*)(self + 0x22C);
            *(f32*)(self + 0x220) = *(f32*)(self + 0x230);
            *(f32*)(self + 0x23C) = 0.0f;
            *(f32*)(self + 0x24C) = 0.0f;
            break;
        default:
            break;
    }
    *(s32*)(self + 0x250) = mode;
    u32 clock = *(u32*)(self + 0x258);
    *(f32*)(self + 0x254) = clock != 0 ? *(f32*)clock : 0.0f;
    if (notify)
        (void)sly1_native_call_regs4(0x001AAB60u, (u32)object, 0, 0, 0);
}

void func_00194E60(void* object, s32 mode) {
    u8* self = (u8*)object;
    s32 flag = mode;
    s32 current_mode = *(s32*)(self + 0x250);
    s32 page = *(s32*)(self + 0x260);
    if (current_mode == mode) return;

    if (current_mode == 0 && page == 0)
        (void)sly1_native_call_regs4(0x001936A0u, (u32)object, (u32)page, 0, 0);

    if (mode == 0) {
        if (*(s32*)(self + 0x274) != 0) {
            s32 queued_page = *(s32*)(self + 0x278);
            if (page != queued_page) {
                *(s32*)(self + 0x260) = queued_page;
                if (queued_page != -1)
                    (void)sly1_native_call_regs4(0x001936A0u, (u32)object,
                                                 (u32)queued_page, 0, 0);
            }
            flag = 1;
            *(s32*)(self + 0x274) = 0;
            *(s32*)(self + 0x278) = -1;
        } else if (page == 0) {
            (void)sly1_native_call_regs4(0x001936A0u, (u32)object, (u32)page, 0, 0);
        }
    }
    func_001AA998(object, flag);
}
