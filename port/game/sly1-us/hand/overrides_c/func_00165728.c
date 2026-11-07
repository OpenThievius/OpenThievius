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
    const s32 trace_scene_group = (u32)self == 0x00847ACCu ||
                                 (u32)self == 0x0084990Cu ||
                                 (u32)self == 0x00FE903Cu ||
                                 (u32)self == 0x00FE942Cu ||
                                 (u32)self == 0x00FE981Cu ||
                                 (u32)self == 0x00FE9C0Cu ||
                                 (u32)self == 0x00FE9FFCu;
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
                if (trace_scene_group) {
                    sly1_native_trace_font(111u, (u32)self, (u32)i,
                        (u32)entry, (u32)subdata, nested_count);
                    sly1_native_trace_font(117u, (u32)subdata,
                        *(u32*)subdata, *(u32*)(subdata + 4u),
                        *(u32*)(subdata + 8u), *(u32*)(subdata + 0xCu));
                    sly1_native_trace_font(118u, (u32)subdata,
                        *(u32*)(subdata + 0x10u), *(u32*)(subdata + 0x14u),
                        *(u32*)(subdata + 0x18u), *(u32*)(subdata + 0x1Cu));
                    sly1_native_trace_font(119u, (u32)subdata,
                        *(u32*)(subdata + 0x20u), *(u32*)(subdata + 0x24u),
                        0, 0);
                    sly1_native_trace_font(120u, (u32)entry,
                        *(u32*)((u8*)entry + 0x38u),
                        *(u32*)((u8*)entry + 0x3Cu),
                        *(u32*)((u8*)entry + 0x40u),
                        *(u32*)((u8*)entry + 0x44u));
                    sly1_native_trace_font(121u, (u32)entry,
                        *(u32*)((u8*)entry + 0x48u),
                        *(u32*)((u8*)entry + 0x4Cu),
                        *(u32*)((u8*)entry + 0x50u),
                        *(u32*)((u8*)entry + 0x54u));
                    sly1_native_trace_font(139u, (u32)entry,
                        *(u32*)((u8*)entry + 0x28u),
                        *(u32*)((u8*)entry + 0x38u),
                        *(u32*)((u8*)entry + 0x58u),
                        *(u32*)((u8*)entry + 0x5Cu));
                }
                for (item = 0; item < nested_count; ++item) {
                    u32 address = nested_items + item * 0x50u;
                    u32 word;
                    sly1_native_trace_font(97u, (u32)i, item, address,
                        *(u32*)(address + 0x24u),
                        *(u32*)(address + 0x28u));
                    for (word = 0; word < 0x50u / 4u; word += 5u) {
                        sly1_native_trace_font(99u + word / 5u,
                            *(u32*)(address + word * 4u),
                            *(u32*)(address + (word + 1u) * 4u),
                            *(u32*)(address + (word + 2u) * 4u),
                            *(u32*)(address + (word + 3u) * 4u),
                            *(u32*)(address + (word + 4u) * 4u));
                    }
                    {
                        u32 payload = *(u32*)(address + 0x18u);
                        if (payload >= 0x10000u &&
                            payload <= 0x02000000u - 0x2Cu) {
                            sly1_native_trace_font(106u, (u32)i, item,
                                address, payload, *(u32*)payload);
                            sly1_native_trace_font(107u,
                                *(u32*)(payload + 4u),
                                *(u32*)(payload + 8u),
                                *(u32*)(payload + 0xCu),
                                *(u32*)(payload + 0x10u),
                                *(u32*)(payload + 0x14u));
                        if (trace_scene_group) {
                            sly1_native_trace_font(122u, item, payload,
                                *(u32*)payload, *(u32*)(payload + 4u),
                                *(u32*)(payload + 8u));
                            sly1_native_trace_font(123u, item,
                                *(u32*)(payload + 0xCu),
                                *(u32*)(payload + 0x10u),
                                *(u32*)(payload + 0x14u),
                                *(u32*)(payload + 0x18u));
                            sly1_native_trace_font(124u, item,
                                *(u32*)(payload + 0x1Cu),
                                *(u32*)(payload + 0x20u),
                                *(u32*)(payload + 0x24u),
                                *(u32*)(payload + 0x28u));
                            {
                                u32 pointer_a = *(u32*)(payload + 0x14u);
                                u32 pointer_b = *(u32*)(payload + 0x1Cu);
                                u32 pointers[2] = { pointer_a, pointer_b };
                                u32 pointer_index;
                                for (pointer_index = 0; pointer_index < 2u;
                                     ++pointer_index) {
                                    u32 pointer = pointers[pointer_index];
                                    u32 stage = pointer_index == 0u ? 126u : 130u;
                                    if (pointer >= 0x10000u &&
                                        pointer <= 0x02000000u - 0x40u) {
                                        sly1_native_trace_font(stage, item,
                                            pointer, *(u32*)pointer,
                                            *(u32*)(pointer + 4u),
                                            *(u32*)(pointer + 8u));
                                        sly1_native_trace_font(stage + 1u, item,
                                            *(u32*)(pointer + 0xCu),
                                            *(u32*)(pointer + 0x10u),
                                            *(u32*)(pointer + 0x14u),
                                            *(u32*)(pointer + 0x18u));
                                        sly1_native_trace_font(stage + 2u, item,
                                            *(u32*)(pointer + 0x1Cu),
                                            *(u32*)(pointer + 0x20u),
                                            *(u32*)(pointer + 0x24u),
                                            *(u32*)(pointer + 0x28u));
                                        sly1_native_trace_font(stage + 3u, item,
                                            *(u32*)(pointer + 0x2Cu),
                                            *(u32*)(pointer + 0x30u),
                                            *(u32*)(pointer + 0x34u),
                                            *(u32*)(pointer + 0x38u));
                                    }
                                }
                                if (trace_scene_group && (u32)self == 0x00847ACCu &&
                                    item == 0u && pointer_a >= 0x10000u &&
                                    pointer_a <= 0x02000000u - 0x40u) {
                                    u32 target_index;
                                    u32 targets[2] = {
                                        *(u32*)(pointer_a + 0x24u),
                                        *(u32*)(pointer_a + 0x34u),
                                    };
                                    for (target_index = 0; target_index < 2u;
                                         ++target_index) {
                                        u32 target = targets[target_index];
                                        u32 word;
                                        if (target < 0x10000u ||
                                            target > 0x02000000u - 0x40u)
                                            continue;
                                        sly1_native_trace_font(160u + target_index,
                                            target, item, pointer_a, 0x40u, 0);
                                        for (word = 0; word < 16u; word += 4u) {
                                            sly1_native_trace_font(
                                                162u + target_index * 4u + word / 4u,
                                                target + word * 4u,
                                                *(u32*)(target + word * 4u),
                                                *(u32*)(target + (word + 1u) * 4u),
                                                *(u32*)(target + (word + 2u) * 4u),
                                                *(u32*)(target + (word + 3u) * 4u));
                                        }
                                        if (target_index == 1u) {
                                            u32 section;
                                            for (section = 0; section < 3u; ++section) {
                                                u32 descriptor = target + section * 0x18u;
                                                u32 data = *(u32*)(descriptor + 8u);
                                                if (data >= 0x10000u &&
                                                    data <= 0x02000000u - 0x20u) {
                                                    sly1_native_trace_font(170u + section * 2u,
                                                        data,
                                                        *(u32*)data,
                                                        *(u32*)(data + 4u),
                                                        *(u32*)(data + 8u),
                                                        *(u32*)(data + 0xCu));
                                                    sly1_native_trace_font(171u + section * 2u,
                                                        data + 0x10u,
                                                        *(u32*)(data + 0x10u),
                                                        *(u32*)(data + 0x14u),
                                                        *(u32*)(data + 0x18u),
                                                        *(u32*)(data + 0x1Cu));
                                                }
                                            }
                                        }
                                    }
                                }
                                if (item == 0u && pointer_b >= 0x10000u &&
                                    pointer_b <= 0x02000000u - 8u) {
                                    u32 record_count = *(u32*)pointer_b;
                                    u32 records = *(u32*)(pointer_b + 4u);
                                    if (record_count <= 16u &&
                                        records >= 0x10000u &&
                                        records <= 0x02000000u -
                                            record_count * 0x10u) {
                                        u32 record;
                                        for (record = 0; record < record_count;
                                             ++record) {
                                            u32 address = records + record * 0x10u;
                                            sly1_native_trace_font(134u,
                                                (u32)i, record, *(u32*)address,
                                                *(u32*)(address + 4u),
                                                *(u32*)(address + 8u));
                                            sly1_native_trace_font(135u,
                                                (u32)i, record,
                                                *(u32*)(address + 0xCu),
                                                address, records);
                                        }
                                    }
                                }
                            }
                        }
                        }
                    if (trace_scene_group) {
                        for (word = 0; word < 0x50u / 4u; word += 4u) {
                            sly1_native_trace_font(112u + word / 4u, item,
                                *(u32*)(address + word * 4u),
                                *(u32*)(address + (word + 1u) * 4u),
                                *(u32*)(address + (word + 2u) * 4u),
                                *(u32*)(address + (word + 3u) * 4u));
                        }
                    }
                    }
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
            sly1_native_trace_font(104u, *(u32*)subdata,
                *(u32*)(subdata + 4u), *(u32*)(subdata + 8u),
                *(u32*)(subdata + 0xCu), *(u32*)(subdata + 0x10u));
            sly1_native_trace_font(105u, *(u32*)(subdata + 0x14u),
                *(u32*)(subdata + 0x18u), *(u32*)(subdata + 0x1Cu),
                *(u32*)(subdata + 0x20u), *(u32*)(subdata + 0x24u));
            {
                u32 payload = *(u32*)(subdata + 4u);
                if (payload >= 0x10000u &&
                    payload <= 0x02000000u - 0x30u) {
                    sly1_native_trace_font(108u, payload,
                        *(u32*)payload, *(u32*)(payload + 4u),
                        *(u32*)(payload + 8u), *(u32*)(payload + 0xCu));
                    sly1_native_trace_font(109u,
                        *(u32*)(payload + 0x10u),
                        *(u32*)(payload + 0x14u),
                        *(u32*)(payload + 0x18u),
                        *(u32*)(payload + 0x1Cu),
                        *(u32*)(payload + 0x20u));
                    sly1_native_trace_font(110u,
                        *(u32*)(payload + 0x24u),
                        *(u32*)(payload + 0x28u),
                        *(u32*)(payload + 0x2Cu), 0, 0);
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
            if (callback != 0) {
                (void)sly1_native_call_regs4(callback, (u32)object, 0, 0, 0);
                if (trace_scene_group &&
                    callback == 0x001B4AD0u) {
                    sly1_native_trace_font(125u, (u32)i, (u32)object,
                        *(u32*)((u8*)object + 0x0Cu),
                        *(u32*)((u8*)object + 0x10u),
                        *(u32*)((u8*)object + 0x14u));
                }
            }
        }
        if (trace_scene_group) {
            sly1_native_trace_font(136u, (u32)self,
                *(u32*)((u8*)self + 0x40u),
                *(u32*)((u8*)entry + 0x58u),
                *(u32*)((u8*)entry + 0x5Cu),
                *(u32*)((u8*)entry + 0x60u));
            sly1_native_trace_font(137u, (u32)subdata,
                *(u32*)(subdata + 0x18u), *(u32*)(subdata + 0x1Cu),
                *(u32*)(subdata + 0x20u), *(u32*)(subdata + 0x24u));
        }
        func_001662A8(self, entry, subdata, context);
        if (trace_scene_group) {
            sly1_native_trace_font(138u, (u32)subdata,
                *(u32*)(subdata + 0x18u), *(u32*)(subdata + 0x1Cu),
                *(u32*)(subdata + 0x20u), *(u32*)(subdata + 0x24u));
            if ((u32)self == 0x00847ACCu && i == 0) {
                u32 pass;
                sly1_native_trace_font(159u, (u32)subdata,
                    *(u32*)(subdata + 0x18u), *(u32*)(subdata + 0x1Cu),
                    *(u32*)(subdata + 0x20u), *(u32*)(subdata + 0x24u));
                for (pass = 0; pass < 2u; ++pass) {
                    u32 count = *(u32*)(subdata + 0x18u + pass * 4u);
                    u32 buffer = *(u32*)(subdata + 0x20u + pass * 4u);
                    u32 qword;
                    if (count > 8u) count = 8u;
                    if (buffer < 0x10000u || buffer > 0x02000000u - 0x80u)
                        continue;
                    for (qword = 0; qword < count; ++qword) {
                        u32 address = buffer + qword * 0x10u;
                        sly1_native_trace_font(143u + pass * 8u + qword,
                            address, *(u32*)address, *(u32*)(address + 4u),
                            *(u32*)(address + 8u), *(u32*)(address + 0xCu));
                    }
                }
            }
        }
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
