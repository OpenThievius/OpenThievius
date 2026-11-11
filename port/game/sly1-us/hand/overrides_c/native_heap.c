/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only bounds checks for Sly's retail guest-heap allocator. */
#include "common.h"

extern u32 D_002C2A1C;
void func_0018D4F8(s32 size);
void func_001F5C0C(void* destination, s32 value, s32 byte_count);
void sly1_native_loader_stage(u32 stage, u32 value, u32 current);

void* func_0018D500(s32 size) {
    u32 current;
    u32 allocation;
    const u32 heap_base = *(u32*)0x002699A4u;
    const u32 heap_limit = *(u32*)0x002699A8u;

    if (size <= 0) return 0;
    current = D_002C2A1C;
    allocation = ((u32)size + 15u) & ~15u;
    if (current < heap_base || current > heap_limit ||
        allocation > heap_limit - current) {
        /* Stage 160 reports the guest heap frontier and configured limit. */
        sly1_native_loader_stage(160u, (u32)size, current);
        return 0;
    }

    if (size >= 0x1000 && current >= 0x01000000u) {
        /* Port-only bounded trace of large allocations in the level-load tail. */
        sly1_native_loader_stage(163u, (u32)size, current);
    }
    func_0018D4F8(size);
    D_002C2A1C = current + allocation;
    return (void*)current;
}

void* func_0018D5C0(u32 size) {
    void* memory;
    if (size > 0x7FFFFFFFu) return 0;
    memory = func_0018D500((s32)size);
    if (memory != 0) {
        func_001F5C0C(memory, 0, (s32)size);
        if (size == 0x13290u) {
            u8* row = (u8*)memory + 239u * 0x28u;
            extern void sly1_native_trace_font(u32, u32, u32, u32, u32, u32);
            sly1_native_trace_font(79u, (u32)memory, size, (u32)row,
                                  *(u32*)row, *(u32*)(row + 4u));
        }
    }
    return memory;
}
