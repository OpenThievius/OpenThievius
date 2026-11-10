/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only resource reader reconstructed from NTSC-U retail instructions. */
#include "common.h"

s32 func_00137B90(void* reader);
s32 func_00137BE0(void* reader);
s32 func_00137D10(void* reader);
u32 func_00137C40(void* reader);
f32 func_00137DF8(void* reader);
void func_00137E70(void* reader, void* destination);
void func_00137EB0(void* reader, void* destination);
void func_00137FB0(void* reader, void* destination);
void* func_0018D500(s32 size);
void func_00160EA0(void* source, s32 mode, void* destination);
void func_00138278(void* reader, void* source, void* destination);
void func_001B9448(void* object, f32 scale);
void func_001289C0(void* object, void* reader);
void func_001B7C40(void* object, s32 enabled);
void func_00138F28(void* owner, void* object);
void sly1_native_call_method(u32 address, u32 object);
void sly1_native_trace_font(u32 stage, u32 a0, u32 a1, u32 a2, u32 a3,
                            u32 a4);

#define SLY1_MAX_RESOURCE_ARRAY_ITEMS 0x10000

#ifndef SLY1_RETAIL_ASM_FUNC_001BAC30
static s32 reader_failed(void* reader) {
    return *(s32*)((u8*)reader + 0x1Cu) < 0;
}

static void* allocate_reader_array(void* reader, s32 count, s32 item_size) {
    if (count <= 0) return 0;
    if (count > SLY1_MAX_RESOURCE_ARRAY_ITEMS ||
        count > 0x7FFFFFFF / item_size) {
        *(s32*)((u8*)reader + 0x1Cu) = -1;
        return 0;
    }
    return func_0018D500(count * item_size);
}

static void copy_mesh_header(void* source, void* destination) {
    const u32* in = (const u32*)source;
    u32* out = (u32*)destination;
    for (s32 i = 0; i < 9; ++i) out[i] = in[i];
}

static void trace_reader_phase(void* self, void* reader, u32 phase) {
    sly1_native_trace_font(64u, 0xBAC30000u | phase,
                           *(u32*)((u8*)reader + 0x18u),
                           (u32)*(s32*)((u8*)reader + 0x1Cu),
                           (u32)self, *(u32*)((u8*)self + 0x3B0u));
}

void func_001BAC30(void* self, void* reader) {
    u8* object = (u8*)self;
    u64 state = *(u64*)(object + 0x2C8u);
    const s32 selector = func_00137B90(reader) & 1;
    trace_reader_phase(self, reader, 0u);
    state = (state & 0xFFFFFFFDFFFFFFFFull) | ((u64)(u32)selector << 32);
    *(u64*)(object + 0x2C8u) = state;

    func_00137FB0(reader, object + 0x380u);
    if (reader_failed(reader)) return;
    {
        const f32 radius = *(f32*)(object + 0x380u);
        *(f32*)(object + 0x3CCu) = radius;
        *(f32*)(object + 0x3D0u) = radius;
    }

    if ((state & 0x8000000000000000ull) != 0) {
        copy_mesh_header(object + 0x380u, object + 0x3A4u);
    } else {
        func_00160EA0(object + 0x380u, 0, object + 0x3A4u);
    }
    func_00138278(reader, object + 0x3A4u, object + 0x3F0u);
    if (reader_failed(reader)) {
        trace_reader_phase(self, reader, 1u);
        return;
    }

    *(f32*)(object + 0x368u) = func_00137DF8(reader);
    func_00137EB0(reader, object + 0x2F0u);
    func_00137E70(reader, object + 0x4D0u);
    if (reader_failed(reader)) {
        trace_reader_phase(self, reader, 2u);
        return;
    }
    func_001B9448(self, 1.0f);

    {
        const s32 count = func_00137BE0(reader);
        void* records = allocate_reader_array(reader, count, 8);
        *(s32*)(object + 0x3FCu) = count;
        *(u32*)(object + 0x400u) = (u32)records;
        if (count > SLY1_MAX_RESOURCE_ARRAY_ITEMS) return;
        for (s32 i = 0; i < count; ++i) {
            u8* record = (u8*)records + (u32)i * 8u;
            *(s32*)(record + 0x00u) = func_00137D10(reader);
            *(u32*)(record + 0x04u) = (u32)func_00137BE0(reader);
            if (reader_failed(reader)) return;
        }
        trace_reader_phase(self, reader, 3u);
    }

    {
        const s32 count = (s32)func_00137C40(reader);
        void* values = allocate_reader_array(reader, count, 4);
        *(u32*)(object + 0x404u) = (u32)values;
        if (count > SLY1_MAX_RESOURCE_ARRAY_ITEMS) return;
        for (s32 i = 0; i < count; ++i) {
            *(s32*)((u8*)values + (u32)i * 4u) = func_00137D10(reader);
            if (reader_failed(reader)) return;
        }
        trace_reader_phase(self, reader, 4u);
    }

    {
        const s32 count = (s32)func_00137C40(reader);
        void* pairs = allocate_reader_array(reader, count, 8);
        *(u32*)(object + 0x408u) = (u32)pairs;
        if (count > SLY1_MAX_RESOURCE_ARRAY_ITEMS) return;
        for (s32 i = 0; i < count; ++i) {
            u8* pair = (u8*)pairs + (u32)i * 8u;
            *(s32*)(pair + 0x00u) = func_00137D10(reader);
            *(s32*)(pair + 0x04u) = func_00137D10(reader);
            if (reader_failed(reader)) return;
        }
        trace_reader_phase(self, reader, 5u);
    }

    {
        const s32 count = *(s32*)(object + 0x3B0u);
        if (count >= 0) {
            if (count >= SLY1_MAX_RESOURCE_ARRAY_ITEMS) {
                *(s32*)((u8*)reader + 0x1Cu) = -1;
                return;
            }
            void* indices = allocate_reader_array(reader, count + 1, 4);
            *(u32*)(object + 0x410u) = (u32)indices;
            for (s32 i = 0; i <= count; ++i) {
                *(s32*)((u8*)indices + (u32)i * 4u) = func_00137D10(reader);
                if (reader_failed(reader)) return;
            }
        }
    }
    trace_reader_phase(self, reader, 6u);

    func_00137FB0(reader, object + 0x4E8u);
    if (reader_failed(reader)) return;
    if ((state & 0x8000000000000000ull) != 0) {
        copy_mesh_header(object + 0x4E8u, object + 0x50Cu);
    } else {
        func_00160EA0(object + 0x4E8u, 0, object + 0x50Cu);
    }
    func_00138278(reader, object + 0x50Cu, object + 0x530u);
    if (reader_failed(reader)) {
        trace_reader_phase(self, reader, 9u);
        return;
    }
    trace_reader_phase(self, reader, 9u);

    func_001289C0(self, reader);
    {
        const u32 model = *(u32*)(object + 0x22Cu);
        const u32 count = model != 0 ? *(u32*)(model + 0x0Cu) : 0;
        const u32 parts = model != 0 ? *(u32*)(model + 0x10u) : 0;
        const u32 first_strips = parts != 0 ? *(u32*)(parts + 0x58u) : 0;
        sly1_native_trace_font(66u, model, count, parts, first_strips,
                               *(u32*)(object + 0x3B0u));
    }
    trace_reader_phase(self, reader, 7u);
    func_001B7C40(self, 1);
    {
        const u32 descriptor = *(u32*)object;
        if (descriptor != 0) {
            sly1_native_call_method(*(u32*)(descriptor + 0x120u), (u32)self);
            if (*(u32*)(object + 0x18u) == 0)
                sly1_native_call_method(*(u32*)(descriptor + 0xD8u), (u32)self);
        }
    }
    func_00138F28((void*)*(u32*)(object + 0x14u), self);
    trace_reader_phase(self, reader, 8u);
}
#endif
