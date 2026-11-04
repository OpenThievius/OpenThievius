// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only compatibility for a one-word record/scalar return disagreement.
#include "common.h"

void* func_0018D500(s32 size);
void* func_00137B90(void* source);
s32 func_00137C40(void* source);

void func_00154EE0(void* self, s32 count, void* unused, void* source) {
    (void)unused;
    const s32 capacity = count < 32 ? count : 32;
    *(s32*)((u8*)self + 0x4C) = capacity;
    s32* items = (s32*)func_0018D500(capacity * 4);
    *(s32**)((u8*)self + 0x50) = items;
    *(void**)((u8*)self + 0x54) = func_00137B90(source);
    for (s32 i = 0; i < count; ++i) {
        const s32 value = func_00137C40(source);
        if (i < capacity) items[i] = value;
    }
}
