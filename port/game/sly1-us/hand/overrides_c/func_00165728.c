/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only trace wrapper matching src/main/00164000.cpp. */
#include "common.h"

typedef void (*Sly1RenderObjectCallback_165728)(void* object);

typedef struct Sly1RenderObject_165728 {
    Sly1RenderObjectCallback_165728* vtable;
} Sly1RenderObject_165728;

typedef struct Sly1RenderEntry_165728 {
    u8 pad0[0x38];
    Sly1RenderObject_165728* object;
    u8 pad1[0x70 - 0x3C];
} Sly1RenderEntry_165728;

typedef struct Sly1RenderNode_165728 {
    u8 pad0[4];
    void* value;
    s32 key;
    u8 pad1[0x80 - 0x0C];
    struct Sly1RenderNode_165728* next;
} Sly1RenderNode_165728;

typedef struct Sly1RenderList_165728 {
    u8 pad0[0x0C];
    s32 count;
    Sly1RenderEntry_165728* entries;
    u8* subdata;
    u8 pad1[0x4C - 0x18];
    Sly1RenderNode_165728* nodes;
} Sly1RenderList_165728;

typedef struct Sly1RenderContext_165728 {
    u8 pad0[0x14];
    void* host;
} Sly1RenderContext_165728;

void* func_0015A418(void* host, s32 type, s32 key, void* context);
void func_001662A8(void* self, void* entry, void* subdata, void* context);
void func_001F2798(void* object, void* context, void* value);
u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);
void sly1_native_trace_font(u32 stage, u32 a0, u32 a1, u32 a2,
                            u32 a3, u32 a4);

void func_00165728(void* self_pointer, void* context_pointer)
{
    Sly1RenderList_165728* self = (Sly1RenderList_165728*)self_pointer;
    Sly1RenderContext_165728* context =
        (Sly1RenderContext_165728*)context_pointer;
    static u32 trace_calls;
    const s32 trace_this_call = trace_calls < 64u;
    s32 i;

    if (trace_this_call) {
        sly1_native_trace_font(86u, (u32)self, (u32)self->count,
            (u32)self->entries, (u32)self->subdata, (u32)self->nodes);
    }
    ++trace_calls;

    for (i = 0; i < self->count; ++i) {
        Sly1RenderEntry_165728* entry = self->entries + i;
        Sly1RenderObject_165728* object = entry->object;
        u8* subdata = self->subdata + (u32)i * 0x28u;

        if (trace_this_call) {
            u32 nested_count = *(u32*)((u8*)entry + 0x58u);
            u32 nested_items = *(u32*)((u8*)entry + 0x5Cu);
            u32 extra_count = *(u32*)((u8*)entry + 0x60u);
            u32 extra_items = *(u32*)((u8*)entry + 0x64u);

            sly1_native_trace_font(88u, (u32)entry,
                *(u32*)((u8*)entry + 0x00), *(u32*)((u8*)entry + 0x04),
                *(u32*)((u8*)entry + 0x08), *(u32*)((u8*)entry + 0x0Cu));
            sly1_native_trace_font(89u, (u32)entry,
                *(u32*)((u8*)entry + 0x10), *(u32*)((u8*)entry + 0x14),
                *(u32*)((u8*)entry + 0x18), *(u32*)((u8*)entry + 0x1Cu));
            sly1_native_trace_font(92u, (u32)entry,
                *(u32*)((u8*)entry + 0x20), *(u32*)((u8*)entry + 0x24),
                *(u32*)((u8*)entry + 0x28), *(u32*)((u8*)entry + 0x2Cu));
            sly1_native_trace_font(93u, (u32)entry,
                *(u32*)((u8*)entry + 0x30), *(u32*)((u8*)entry + 0x34),
                (u32)object, *(u32*)subdata);
            sly1_native_trace_font(94u, (u32)self,
                *(u32*)((u8*)self + 0x40), *(u32*)((u8*)self + 0x44),
                *(u32*)((u8*)self + 0x58), *(u32*)((u8*)self + 0x5Cu));
            sly1_native_trace_font(95u, (u32)entry,
                *(u32*)((u8*)entry + 0x58), *(u32*)((u8*)entry + 0x5Cu),
                *(u32*)((u8*)entry + 0x60), *(u32*)((u8*)entry + 0x64));
            sly1_native_trace_font(96u, (u32)subdata, *(u32*)subdata,
                *(u32*)(subdata + 4u), *(u32*)(subdata + 8u),
                *(u32*)(subdata + 0xCu));
            if (nested_count <= 32u && nested_items >= 0x10000u &&
                nested_items < 0x02000000u && nested_count <=
                    (0x02000000u - nested_items) / 0x50u) {
                u32 item;
                for (item = 0; item < nested_count; ++item) {
                    u32 address = nested_items + item * 0x50u;
                    sly1_native_trace_font(97u, (u32)i, item, address,
                        *(u32*)(address + 0x24u),
                        *(u32*)(address + 0x28u));
                }
            }
            if (extra_count <= 32u && extra_items >= 0x10000u &&
                extra_items < 0x02000000u && extra_count <=
                    (0x02000000u - extra_items) / 0x20u) {
                u32 item;
                for (item = 0; item < extra_count; ++item) {
                    u32 address = extra_items + item * 0x20u;
                    sly1_native_trace_font(98u, (u32)i, item, address,
                        *(u32*)(address + 4u),
                        *(u32*)(address + 8u));
                }
            }
        }
        if (object != 0) {
            u32 vtable = (u32)object->vtable;
            u32 callback = vtable != 0 ? *(u32*)(vtable + 4u) : 0;
            if (trace_this_call) {
                sly1_native_trace_font(87u, (u32)i, (u32)entry,
                    (u32)object, vtable, callback);
            }
            if (callback != 0)
                (void)sly1_native_call_regs4(callback, (u32)object, 0, 0, 0);
        }
        func_001662A8(self, entry, subdata, context);
    }

    {
        Sly1RenderNode_165728* node = self->nodes;
        while (node != 0) {
            void* object = func_0015A418(context->host, 0x104, node->key,
                                         context);
            if (object != 0)
                func_001F2798(object, context, node->value);
            node = node->next;
        }
    }
}
