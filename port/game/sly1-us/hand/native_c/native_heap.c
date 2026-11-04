/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only initialization of Sly's guest heap for native backfills. */
#include "common.h"

void func_001E22C0(void* state);

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
