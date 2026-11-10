/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only guest-memory draw-entry preparation and callback dispatch. */
#include "common.h"

void* func_001B5EE0(void* pool);
void* func_001BB540(void* object, s32 selector);
f32 func_00185C18(void* indices);
f32 func_00185CA0(void* indices);
s32 func_001278A8(void* object, s32 unused, void* input, void* output,
                  void* unused_a, s32 unused_b, s32 unused_c);
void func_00128018(void* first_transform, void* second_transform,
                   void* input, void* output);
void sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);

typedef struct SlyDrawIndexPair {
    s32 first;
    s32 second;
} SlyDrawIndexPair;

static void call_slot(u32 object, u32 offset, u32 entry, u32 part)
{
    u32 vtable;
    u32 method;
    if (object == 0) return;
    vtable = *(u32*)object;
    if (vtable == 0) return;
    method = *(u32*)(vtable + offset);
    if (method >= 0x00100000u && method < 0x00220000u)
        sly1_native_call_regs4(method, object, entry, part, 0);
}

void func_00185D20(f32 weight, void* owner, void* alternate,
                   const f32* input_position, const f32* basis,
                   s32 mode, s32 callback_data, s32 tail,
                   u32* entry_cursor)
{
    u8* owner_bytes = (u8*)owner;
    u32 world = *(u32*)(owner_bytes + 0x14);
    u8* entry = (u8*)func_001B5EE0((void*)(world + 0x1AFC));
    SlyDrawIndexPair indices;
    u32 part;

    if (entry == 0) return;
    *(f32*)(entry + 0x90) = weight;
    for (part = 0; part < 4; ++part) {
        *(f32*)(entry + 0x10 + part * 4) = input_position[part];
        *(f32*)(entry + 0x50 + part * 4) = input_position[part] - basis[part] * weight;
        *(f32*)(entry + 0x80 + part * 4) = basis[part];
    }

    indices.first = callback_data;
    indices.second = tail;
    *(f32*)(entry + 0x94) = func_00185C18(&indices);
    *(f32*)(entry + 0x98) = func_00185CA0(&indices);
    *(s32*)(entry + 0x9C) = mode;
    *(s32*)(entry + 0xA0) = callback_data;
    *(s32*)(entry + 0xA4) = tail;
    *(u32*)(entry + 0xD0) = entry_cursor != 0 ? *entry_cursor : 0;

    for (part = 0; part < 2; ++part) {
        u8* record = entry + part * 0x40;
        u32 transform = part == 0 ? (u32)owner : (u32)alternate;
        *(u32*)(record + 4) = transform;
        if (transform != 0) {
            *(u32*)(record + 0) = *(u32*)(transform + 0x50);
            *(s32*)(record + 8) = *(s8*)(transform + 0x2C8);
            func_00128018(0, (void*)transform, record + 0x10, record + 0x20);
        } else {
            u32 lane;
            *(u32*)(record + 0) = 0;
            *(s32*)(record + 8) = 0;
            for (lane = 0; lane < 4; ++lane)
                ((f32*)(record + 0x20))[lane] = ((f32*)(record + 0x10))[lane];
        }
        if (transform != 0) {
            u32 callback_object = (u32)func_001BB540((void*)transform, 0x100);
            call_slot(callback_object, 0x100, (u32)entry, part);
        }
    }

    func_001860A8((void*)(world + 0x1AFC), 1, (u32*)&entry);
    if (entry_cursor != 0) *entry_cursor = (u32)entry;
}

void func_001860A8(void* unused_queue, s32 count, u32* entry_addresses)
{
    s32 draw;
    (void)unused_queue;
    for (draw = 0; draw < count; ++draw) {
        u32 entry_address = entry_addresses[draw];
        u8* entry = (u8*)entry_address;
        u32 part;

        for (part = 0; part < 2; ++part) {
            u8* record = entry + part * 0x40;
            u32 object = *(u32*)(record + 4);
            if (object == 0) continue;
            (void)func_001278A8((void*)object, 0, record + 0x10,
                                record + 0x30, 0, 0, 0);
            call_slot(object, 0xFC, entry_address, part);
        }

        for (part = 0; part < 2; ++part) {
            u32 object = *(u32*)(entry + part * 0x40 + 4);
            u32 callback_object;
            if (object == 0) continue;
            callback_object = (u32)func_001BB540((void*)object, 0x104);
            call_slot(callback_object, 0x104, entry_address, part);
        }

        for (part = 0; part < 4; ++part) {
            f32 first = *(f32*)(entry + 0x30 + part * 4);
            f32 second = *(f32*)(entry + 0x70 + part * 4);
            *(f32*)(entry + 0xB0 + part * 4) = first - second;
        }
    }
}
