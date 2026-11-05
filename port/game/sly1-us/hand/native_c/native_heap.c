/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only initialization of Sly's guest heap for native backfills. */
#include "common.h"

extern u32 D_002699A4;
extern u32 D_002699A8;

void func_001E22C0(void* state);

void* func_0018D500(s32 size) {
    const u32 current = D_002699A4;
    const u32 aligned = (current + 15u) & ~15u;
    const u32 padding = aligned - current;
    const u32 remaining = D_002699A8;
    const u32 requested = (u32)size;

    if (size <= 0 || padding > remaining || requested > remaining - padding) {
        return 0;
    }
    D_002699A4 = aligned + requested;
    D_002699A8 = remaining - padding - requested;
    return (void*)aligned;
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
