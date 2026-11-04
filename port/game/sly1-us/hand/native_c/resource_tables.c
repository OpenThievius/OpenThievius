/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only initialization for Sly's loader resource tables. */
#include "common.h"

void func_001B5DD0(void* table, s32 stride, s32 count);
void* func_0018D5C0(u32 size);

void func_0011CA90(void) {
    u32 count0 = *(u32*)0x0025C2F0u;
    u32 count1 = *(u32*)0x0025C2F4u;
    u32 count2 = *(u32*)0x0025C2F8u;
    u32 count3 = *(u32*)0x0025C2FCu;
    u32 count4 = *(u32*)0x0025C300u;

    func_001B5DD0((void*)0x0062C558u, 0x10, (s32)count0);
    *(u32*)0x0027B300u = (u32)func_0018D5C0(count0);

    func_001B5DD0((void*)0x0062C568u, 0x40, (s32)count1);
    *(u32*)0x0027B304u = (u32)func_0018D5C0(count1);

    func_001B5DD0((void*)0x0062C578u, 0x10, (s32)count2);
    *(u32*)0x0027B308u = (u32)func_0018D5C0(count2);

    func_001B5DD0((void*)0x0062C588u, 0x08, (s32)count3);
    *(u32*)0x0027B30Cu = (u32)func_0018D5C0(count3);

    func_001B5DD0((void*)0x0062C598u, 0x0C, (s32)count4);
    *(u32*)0x0027B310u = (u32)func_0018D5C0(count4);
}
