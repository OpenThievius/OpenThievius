/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only initialization of Sly's guest heap for native backfills. */
#include "common.h"

extern u32 D_002C2A1C;

void func_001E22C0(void* state);
u8* func_001F5C0C(u8* destination, int value, int byte_count);

void* func_0018D500(s32 size) {
    u32 current;
    u32 allocation;
    const u32 heap_base = 0xE0C00000u;
    const u32 heap_limit = heap_base + 0x02000000u;
    if (size <= 0) return 0;

    current = D_002C2A1C;
    allocation = ((u32)size + 15u) & ~15u;
    if (current < heap_base || current > heap_limit || allocation > heap_limit - current)
        return 0;
    D_002C2A1C = current + allocation;
    return (void*)current;
}

void* func_0018D5C0(u32 size) {
    void* memory = func_0018D500((s32)size);
    if (memory != 0) (void)func_001F5C0C((u8*)memory, 0, (s32)size);
    return memory;
}

void func_0018D430(void) {
    /* Keep a 4 MB bump heap after the 8 MB per-thread scratch arenas. */
    const u32 heap_base = 0xE0C00000u;

    *(u32*)0x002C2A10u = heap_base;
    *(u32*)0x002C2A14u = heap_base;
    *(u32*)0x002C2A18u = heap_base;
    *(u32*)0x002C2A1Cu = heap_base;
    *(u32*)0x002C2A20u = 0x02000000u;
    *(u32*)0x002C2A24u = 0x02000000u;
    *(u32*)0x002C2A34u = 0xFFFFFFFFu;
    *(u32*)0x002699A4u = heap_base;
    *(u32*)0x002699A8u = 0x02000000u;

    func_001E22C0((void*)0x002C2A28u);
}
