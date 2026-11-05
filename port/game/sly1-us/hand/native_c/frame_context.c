/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only translation of the retail frame-context setup. */
#include "common.h"

typedef struct NativeFrameList {
    void* head;
    void* tail;
    s32 offset;
} NativeFrameList;

typedef struct NativeFrameContext {
    u8 pad00[0x04];
    union { u8* f04; s32 i04; };
    u8 pad08[0x04];
    union { u8* f0C; s32 i0C; };
    union { u8* f10; s32 i10; };
    u8 pad14[0x04];
    u32 flags;
    s32 f1C;
    NativeFrameList l20;
    NativeFrameList l2C;
    NativeFrameList l38;
    s32 f44;
} NativeFrameContext;

extern NativeFrameContext D_00262320[2];
extern NativeFrameContext* D_002622D8;
extern s32 D_00262318;
extern u8* D_00261990;
extern u8 D_00262280[];
extern u8 D_002622A8[];
extern s32 D_002622D0;
extern s32 D_00269BA4;
extern s32 D_002623B4;
extern s32 D_002623B0;

extern NativeFrameList D_002622E0;
extern NativeFrameList D_002622F0;
extern NativeFrameList D_00262300;

s32 func_001F6B60(s32 semaphore);
void func_00191E70(s32 value);
void func_0015EFE8(NativeFrameContext* context);
void func_001529C8(void* packet, s32 capacity, void* buffer);
void func_00152B38(void* packet);
void func_001F6C20(s32 channel);
void func_001F6B40(s32 semaphore);

void func_00191E70(s32 value)
{
    if (D_00269BA4 != value) {
        func_001F6C20(0);
        D_00269BA4 = value;
    }
}

void func_0015E9F0(void)
{
    s32 index = D_00262318;
    NativeFrameContext* context;

    func_00191E70(3);
    (void)func_001F6B60(D_002623B4);
    D_00262318 = index + 1;
    if ((u32)(index + 1) >= 2u) D_00262318 = 0;
    context = &D_00262320[index];
    D_002622D8 = context;

    /* Flush frame-owned transient lists before reusing their packet buffers. */
    func_0015EFE8(context);
    context->flags = 0;
    context->f44 = *(s32*)(D_00261990 + 0x21Cu);
    context->f1C = 0;
    func_001529C8(D_00262280, 0x8000, context->f04);
    func_001529C8(D_002622A8, 0x10000, context->f0C);
    D_002622D0 = context->i10;
    func_00152B38(D_00262280);
    func_00152B38(D_002622A8);
}

/* Retire an empty native frame and wake the retail display worker. The
 * matching object-list destructors are still assembly-only; keep queued
 * objects intact until their callbacks have a port implementation. */
void func_0015EC68(void)
{
    NativeFrameContext* context = D_002622D8;
    if (context == 0) return;
    if (D_002622E0.head != 0 || D_002622E0.tail != 0 ||
        D_002622F0.head != 0 || D_002622F0.tail != 0 ||
        D_00262300.head != 0 || D_00262300.tail != 0) return;

    context->l20 = D_002622E0;
    context->l2C = D_002622F0;
    context->l38 = D_00262300;
    D_002622D8 = 0;
    D_002622D0 = 0;
    func_001F6B40(D_002623B0);
}
