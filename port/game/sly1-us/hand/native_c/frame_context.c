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

/* RemoveFromList walks this exact four-word frame chain when it unlinks the
 * current item. Keep the frame in guest stack memory (hostgen lowers its
 * address-taken local) and use the retail field order. */
typedef struct NativeListWalkFrame {
    u32 list;
    u32 link;
    s32 offset;
    u32 previous;
} NativeListWalkFrame;

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
extern u8* D_00275710;
extern u32 D_0027B314;

extern NativeFrameList D_002622E0;
extern NativeFrameList D_002622F0;
extern NativeFrameList D_00262300;

s32 func_001F6B60(s32 semaphore);
void func_00191E70(s32 value);
void func_0015EFE8(NativeFrameContext* context);
void func_00152600(void* list, void* node);
void func_00152470(void* list, void* node);
void func_00152440(void* list);
void func_001B5F28(void* list, void* node);
void func_001529C8(void* packet, s32 capacity, void* buffer);
void func_00152B38(void* packet);
void func_0015EB20(void* packet, void* frame, s32 unused);
void func_0015EBF0(void* packet, void* frame, s32 unused);
void func_001F6C20(s32 channel);
int openrac_lib_sly1_signal_sema(u32 semaphore);
void sly1_native_trace_font(u32 stage, u32 a0, u32 a1, u32 a2, u32 a3,
                            u32 a4);
void sly1_native_trace_frame_lists(u32 context);


void func_0015EFE8(NativeFrameContext* context)
{
    NativeListWalkFrame iterator;
    NativeFrameList* lists[3];
    s32 list_index;
    if (context == 0) return;
    sly1_native_trace_frame_lists((u32)context);
    lists[0] = &context->l20;
    lists[1] = &context->l2C;
    lists[2] = &context->l38;

    for (list_index = 0; list_index < 3; ++list_index) {
        NativeFrameList* list = lists[list_index];
        u32 visited = 0;
        if (list->head == 0) continue;
        iterator.list = (u32)list;
        iterator.link = (u32)list;
        iterator.offset = list->offset;
        iterator.previous = D_0027B314;
        D_0027B314 = (u32)&iterator;
        for (;;) {
            u32 node = *(u32*)iterator.link;
            u32 root = (u32)D_00275710;
            if (node == 0 || visited++ >= 4096u) break;
            iterator.link = node + (u32)iterator.offset;
            if (list_index == 0) {
                u32 owner = *(u32*)(node + 0x14u);
                func_00152600(list, node);
                if (owner != 0) func_00152470((void*)(owner + 0x1CC0u), (void*)node);
            } else {
                u32 free_slot = *(u32*)(node + 0x1088u);
                if (list_index == 1 && free_slot != 0 && root != 0)
                    func_001B5F28((void*)(root + 0x1B4Cu), (void*)free_slot);
                func_00152600(list, node);
                if (root != 0)
                    func_001B5F28((void*)(root + (list_index == 1 ? 0x1B40u : 0x1BF4u)),
                                  (void*)node);
            }
        }
        if (list_index != 2) func_00152440(list);
        D_0027B314 = iterator.previous;
    }
}

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

    sly1_native_trace_font(180u, (u32)context,
                           *(u32*)(D_00262280 + 4u),
                           *(u32*)(D_00262280 + 0x0Cu),
                           *(u32*)(D_002622A8 + 4u),
                           *(u32*)(D_002622A8 + 0x0Cu));
}

/* Finish both packet writers before handing the frame to the display worker. */
void func_0015EC68(void)
{
    NativeFrameContext* context = D_002622D8;
    if (context == 0) return;

    sly1_native_trace_font(181u, (u32)context,
                           *(u32*)(D_00262280 + 4u),
                           *(u32*)(D_00262280 + 0x0Cu),
                           *(u32*)(D_002622A8 + 4u),
                           *(u32*)(D_002622A8 + 0x0Cu));

    func_0015EB20(D_002622A8, (u8*)context + 8u, 0);
    func_0015EBF0(D_00262280, context, 0);

    context->l20 = D_002622E0;
    context->l2C = D_002622F0;
    context->l38 = D_00262300;
    func_00152440(&D_002622E0);
    func_00152440(&D_002622F0);
    func_00152440(&D_00262300);
    sly1_native_trace_frame_lists((u32)context);
    D_002622D8 = 0;
    D_002622D0 = 0;
    (void)openrac_lib_sly1_signal_sema((u32)D_002623B0);
    sly1_native_trace_font(182u, (u32)context, (u32)context->i04,
                           (u32)context->i0C,
                           (u32)context->f04, (u32)context->f0C);
}

extern s32 D_002622D4;
extern s32 D_00274538;
extern s32 D_00261F1C;
void func_001B3488(s32 value);
void func_00202A80(s32 dma, void* source);

void func_0015ED50(void* frame)
{
    u8* bytes = (u8*)frame;
    if (bytes == 0) return;
    D_002622D4 = *(s32*)(bytes + 0x10u);
    func_001B3488(*(s32*)(bytes + 0x44u));
    if (*(s32*)(bytes + 0x1Cu) != 0 && D_00274538 != 0) {
        *(volatile s32*)0x10003010u = 4;
        func_00202A80(D_00261F1C, (void*)(u32)D_00274538);
    }
}
