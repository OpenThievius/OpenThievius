/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only deferred draw dispatcher, kept separate from matching decomp C. */
#include "common.h"

typedef struct DeferredRequestNative {
    u32 callback;
    f32 sort_value;
    s32 group;
    u8 pad[0x74];
} __attribute__((aligned(16))) DeferredRequestNative;

typedef char check_DeferredRequestNative[
    sizeof(DeferredRequestNative) == 0x80 ? 1 : -1];

struct V24A0;
extern struct V24A0 D_002622A8;

extern void func_00184DE8(void* world);
extern s32 func_0019D4B0(s32 count, DeferredRequestNative* requests,
                         void** out);
extern void func_00152D70(void* writer);
extern void func_00152B38(void* writer);
extern void func_001EE7D0(void* writer, s32 qwords, void* source, s32 flag);
extern u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2,
                                  u32 a3);
extern s32 D_00604780;
extern s32 D_00604784;
extern u32 D_00604788;
extern u32 D_0060478C;
extern s32 D_00262474;
extern u8 D_002BDC20[];

#define SLY1_DEFERRED_CAPACITY 0x412u

void func_0019DC98(void* world, void* camera) {
    void* ordered[SLY1_DEFERRED_CAPACITY];
    DeferredRequestNative* requests = (DeferredRequestNative*)0x005E4700u;
    u32 cursor = *(u32*)0x0026AC10u;
    u32 first = 0x005E4700u;
    s32 count = 0;

    (void)camera;
    func_00184DE8(world);
    if (cursor >= first && ((cursor - first) & 0x7Fu) == 0) {
        u32 pending = (cursor - first) >> 7;
        if (pending <= SLY1_DEFERRED_CAPACITY - 1u)
            count = (s32)pending;
    }

    D_00604784 = 0;
    D_00262474 = 0x214;
    D_00604780 = 0;
    D_00604788 = 0;
    D_0060478C = 0;

    s32 ordered_count = func_0019D4B0(count, requests, ordered);
    *(u32*)((u8*)&D_002622A8 + 0x20u) = 1;
    func_00152D70(&D_002622A8);
    for (s32 i = 0; i < ordered_count; ++i) {
        void* entry = ordered[i];
        u32 callback = *(u32*)entry;
        if (callback != 0)
            (void)sly1_native_call_regs4(callback, (u32)entry, 0, 0, 0);
    }

    func_00152B38(&D_002622A8);
    if (ordered_count == 0)
        func_001EE7D0(&D_002622A8, 0x2C, D_002BDC20, 0);
    *(u32*)0x0026AC10u = 0;
}

