/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only pool setup matching Sly's resource pool layout. */
#include "common.h"

void* func_0018D500(s32 size);
u8* func_001F5C0C(u8* destination, int value, int byte_count);

void* func_0011C288(void* self, s32 index) {
    u8* pool = (u8*)self;
    u32 stride = *(u32*)pool;
    u32 base = *(u32*)(pool + 8);
    return (void*)(base + (u32)index * stride);
}

void func_0011C128(void* self, s32 stride, s32 count) {
    u8* pool = (u8*)self;
    u32 slots = (u32)stride + 8u;
    u32 bytes = slots * (u32)count;
    u32 memory = (u32)func_0018D500((s32)bytes);
    void* node;
    s32 index;

    *(u32*)(pool + 0x00) = slots;
    *(u32*)(pool + 0x04) = (u32)count;
    *(u32*)(pool + 0x08) = memory;
    node = func_0011C288(self, 0);
    *(u32*)(pool + 0x0C) = (u32)node;

    for (index = 1; index < count; ++index) {
        void* next = func_0011C288(self, index);
        *(u32*)node = (u32)next;
        node = next;
    }
    if (node != 0) *(u32*)node = 0;
    *(u32*)(pool + 0x10) = 0;
    *(u32*)(pool + 0x14) = 0;
}

/* Port-only pool allocation. A pool slot starts with an 8-byte link/header;
 * callers receive the payload immediately after it. */
void* func_0011C1D8(void* self) {
    u8* pool = (u8*)self;
    u32 node = *(u32*)(pool + 0x0C);
    if (node == 0) return 0;

    *(u32*)(pool + 0x0C) = *(u32*)node;
    *(u32*)node = *(u32*)(pool + 0x10);
    *(u32*)(pool + 0x10) = node;
    return (void*)(node + 8u);
}

void* func_0011C240(void* self) {
    void* result = func_0011C1D8(self);
    if (result != 0) {
        u32 payload_size = *(u32*)self - 8u;
        func_001F5C0C(result, 0, payload_size);
    }
    return result;
}
