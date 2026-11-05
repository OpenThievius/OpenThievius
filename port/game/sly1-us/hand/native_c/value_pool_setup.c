/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only value-pool startup backfills, kept separate from matching C. */
#include "common.h"

void func_0011C128(void* pool, s32 item_size, s32 count);
void func_0011CA90(void);

void func_0011A6A8(void* counters) {
    u8* bytes = (u8*)counters;
    *(s32*)(bytes + 0x00) = 0;
    *(s32*)(bytes + 0x504) = 0;
}

void func_0011A670(void) {
}

void func_0011A6B8(void) {
}

void func_0011BF70(void) {
    func_0011A6A8((void*)0x00259F38u);

    func_0011C128((void*)0x0025C250u, 0x0C, 0x2000);
    *(u32*)0x0025C268u = 0x0011AF78u;

    func_0011C128((void*)0x0025C270u, 0x1C, 0x0190);
    *(u32*)0x0025C288u = 0x0011A670u;

    func_0011C128((void*)0x0025C290u, 0x10, 0x0800);
    *(u32*)0x0025C2A8u = 0x0011C4E8u;

    func_0011C128((void*)0x0025C2B0u, 0x14, 0x0800);
    func_0011CA90();

    func_0011C128((void*)0x0025C2D0u, 0x0C, 0x0080);
}
