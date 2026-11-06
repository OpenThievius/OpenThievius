/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only collision mesh loader from the US retail class-0x03 method. */
#include "common.h"

s32 func_00137B90(void* stream);
void func_00137FB0(void* stream, void* mesh);
void func_00138278(void* stream, void* mesh, void* tree);
f32 func_00137DF8(void* stream);
void func_00137EB0(void* stream, void* matrix);
void func_00137E70(void* stream, void* vector);
s32 func_00137BE0(void* stream);
s32 func_00137D10(void* stream);
s32 func_00137C40(void* stream);
void* func_0018D500(s32 size);
void func_00160EA0(void* source_mesh, s32 mode, void* destination_mesh);
void func_001B9448(void* collider, f32 scale);
void func_001289C0(void* collider, void* stream);
void func_001B7C40(void* collider, s32 enabled);
void func_00138F28(void* world, void* collider);
void sly1_native_call_method(u32 address, u32 object);
void sly1_native_loader_stage(u32 stage, u32 object, u32 reader);

static void trace_reader(u32 stage, void* object, void* reader) {
    sly1_native_loader_stage(stage, (u32)object, (u32)reader);
}

static void copy_words(u8* destination, const u8* source, u32 count)
{
    u32 i;
    for (i = 0; i < count; ++i)
        ((u32*)destination)[i] = ((const u32*)source)[i];
}

static void call_descriptor_method(u8* object, u32 offset)
{
    u32 descriptor = *(u32*)object;
    u32 method;
    if (descriptor == 0) return;
    method = *(u32*)(descriptor + offset);
    if (method >= 0x00100000u && method < 0x00220000u)
        sly1_native_call_method(method, (u32)object);
}

void func_001BAC30(void* object, void* stream)
{
    u8* self = (u8*)object;
    u8* mesh;
    u8* mesh_copy;
    u8* tree;
    s32 i;
    s32 count;
    s32 flag = func_00137B90(stream) & 1;
    u64 object_flags = *(u64*)(self + 0x538);
    object_flags = (object_flags & ~((u64)1 << 33)) | ((u64)(u32)flag << 33);
    *(u64*)(self + 0x538) = object_flags;

    mesh = self + 0x380;
    mesh_copy = self + 0x3A4;
    tree = self + 0x3F0;
    trace_reader(70, object, stream);
    func_00137FB0(stream, mesh);
    trace_reader(71, object, stream);
    *(f32*)(self + 0x3D0) = *(f32*)mesh;
    *(f32*)(self + 0x3CC) = *(f32*)mesh;
    if ((object_flags & ((u64)1 << 33)) != 0)
        copy_words(mesh_copy, mesh, 9);
    else
        func_00160EA0(mesh, 0, mesh_copy);
    trace_reader(72, object, stream);
    func_00138278(stream, mesh_copy, tree);
    trace_reader(73, object, stream);

    *(f32*)(self + 0x368) = func_00137DF8(stream);
    func_00137EB0(stream, self + 0x2F0);
    func_00137E70(stream, self + 0x4D0);
    func_001B9448(self, 1.0f);

    count = func_00137BE0(stream);
    trace_reader(74, object, stream);
    *(s32*)(self + 0x3FC) = count;
    *(u32*)(self + 0x400) = (u32)func_0018D500(count * 8);
    for (i = 0; i < count; ++i) {
        u8* pair = (u8*)*(u32*)(self + 0x400) + i * 8;
        *(s32*)(pair + 0) = func_00137D10(stream);
        *(s32*)(pair + 4) = func_00137BE0(stream);
    }

    count = func_00137C40(stream);
    trace_reader(75, object, stream);
    *(u32*)(self + 0x404) = (u32)func_0018D500(count * 4);
    for (i = 0; i < count; ++i)
        ((s32*)*(u32*)(self + 0x404))[i] = func_00137D10(stream);

    count = func_00137C40(stream);
    trace_reader(76, object, stream);
    *(s32*)(self + 0x408) = count;
    *(u32*)(self + 0x40C) = (u32)func_0018D500(count * 8);
    for (i = 0; i < count; ++i) {
        u8* pair = (u8*)*(u32*)(self + 0x40C) + i * 8;
        *(s32*)(pair + 0) = func_00137D10(stream);
        *(s32*)(pair + 4) = func_00137D10(stream);
    }

    if (count > 0) {
        s32 face_count = *(s32*)(mesh_copy + 0x0C);
        *(u32*)(self + 0x410) = (u32)func_0018D500((face_count + 1) * 4);
        for (i = 0; i <= face_count; ++i)
            ((s32*)*(u32*)(self + 0x410))[i] = func_00137D10(stream);
    }

    mesh = self + 0x4E8;
    mesh_copy = self + 0x50C;
    tree = self + 0x530;
    trace_reader(77, object, stream);
    func_00137FB0(stream, mesh);
    trace_reader(78, object, stream);
    if ((object_flags & ((u64)1 << 33)) != 0)
        copy_words(mesh_copy, mesh, 9);
    else
        func_00160EA0(mesh, 0, mesh_copy);
    trace_reader(79, object, stream);
    func_00138278(stream, mesh_copy, tree);
    trace_reader(80, object, stream);
    func_001289C0(self, stream);
    trace_reader(81, object, stream);
    func_001B7C40(self, 1);
    call_descriptor_method(self, 0x120);
    if (*(u32*)(self + 0x18) == 0) {
        call_descriptor_method(self, 0xD8);
        func_00138F28((void*)*(u32*)(self + 0x14), self);
    }
}
