/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only typed resource descriptor reader, separate from matched C. */
#include "common.h"

s32 func_00137B90(void* reader);
s32 func_00137D10(void* reader);
s32 func_00137D78(void* reader);
s32 func_00137C40(void* reader);
f32 func_00137DF8(void* reader);
void* func_0018D500(s32 size);
void func_0011B008(void* value);
void func_0011B5F0(void* value, s32 payload);
void func_0011B570(void* value, s32 payload);
void func_0011B5B0(void* value, f32 payload);
void func_0011B740(void* value, void* payload);
void func_0011B788(void* value, void* payload);
void func_0011B7D0(void* value, void* payload);
void func_0011B818(void* value, void* payload);
void func_0011B060(void* value, s32 mode);
u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);

typedef struct ResourceDescriptor {
    s32 type;
    u32 flags;
    u32 unused8;
    u32 callback;
    u32 unused10;
    s32 count;
    s32 offset;
    u32 accessor;
} ResourceDescriptor;

typedef struct ResourceValue {
    s32 type;
    u32 payload;
    u32 child;
    u32 reserved;
} ResourceValue;

static u32 read_value(void* reader, s32 type, ResourceValue* value) {
    value->type = -1;
    value->payload = value->child = value->reserved = 0;
    switch (type) {
    case 0:
        func_0011B5F0(value, func_00137B90(reader));
        break;
    case 5:
        func_0011B570(value, func_00137D78(reader));
        break;
    case 1:
        func_0011B5B0(value, func_00137DF8(reader));
        break;
    case 6: case 7: case 14: case 15: case 16: case 17: case 20:
        func_0011B570(value, func_00137D10(reader));
        break;
    case 2: case 9: case 11: {
        f32* vector = (f32*)func_0018D500(12);
        vector[0] = func_00137DF8(reader);
        vector[1] = func_00137DF8(reader);
        vector[2] = func_00137DF8(reader);
        func_0011B7D0(value, vector);
        break;
    }
    case 10: {
        f32* vector = (f32*)func_0018D500(8);
        vector[0] = func_00137DF8(reader);
        vector[1] = func_00137DF8(reader);
        func_0011B818(value, vector);
        break;
    }
    case 3: case 12: case 4: {
        f32* vector = (f32*)func_0018D500(16);
        vector[0] = func_00137DF8(reader);
        vector[1] = func_00137DF8(reader);
        vector[2] = func_00137DF8(reader);
        vector[3] = type == 4 ? 1.0f : func_00137DF8(reader);
        value->type = type;
        value->payload = (u32)vector;
        break;
    }
    case 13:
        value->type = type;
        value->payload = (u32)func_00137C40(reader);
        break;
    default:
        if (((u32)type & 0x7FFFFFFFu) >> 12 == 1) {
            func_0011B570(value, func_00137D10(reader));
        }
        break;
    }
    return value->payload;
}

static u32 descriptor_target(ResourceDescriptor* descriptor, void* object) {
    if ((descriptor->flags & 0x1000u) == 0) return (u32)object;
    if (descriptor->accessor == 0) return 0;
    {
        extern void sly1_native_loader_stage(u32, u32, u32);
        sly1_native_loader_stage(162u, descriptor->accessor, (u32)descriptor);
    }
    return sly1_native_call_regs4(descriptor->accessor, (u32)object, 1, 0, 0);
}

void func_0011E680(void* object, ResourceDescriptor* descriptor, void* reader) {
    const u32 flags = descriptor->flags;
    if (flags & 0x400u) {
        ResourceValue values[32];
        ResourceValue result;
        s32 i;
        s32 count = descriptor->count;
        if (count < 0) count = 0;
        if (count > 32) count = 32;
        for (i = 0; i < count; ++i) {
            func_0011B008(&values[i]);
            read_value(reader, func_00137D10(reader), &values[i]);
        }
        func_0011B008(&result);
        if (descriptor->callback != 0) {
            extern void sly1_native_loader_stage(u32, u32, u32);
            sly1_native_loader_stage(162u, descriptor->callback,
                                     (u32)descriptor);
            sly1_native_call_regs4(descriptor->callback, (u32)&result, (u32)object,
                                   (u32)count, (u32)values);
        }
        func_0011B060(&result, 2);
        while (count > 0) func_0011B060(&values[--count], 0);
        return;
    }

    if (flags & 0x80u) {
        u32 target = descriptor_target(descriptor, object);
        u8* destination;
        if (target == 0) return;
        destination = (u8*)target + descriptor->offset;
        switch (descriptor->type) {
        case 0: *destination = (u8)func_00137B90(reader); return;
        case 5: *(s32*)destination = func_00137D78(reader); return;
        case 1: *(f32*)destination = func_00137DF8(reader); return;
        case 10:
            ((f32*)destination)[0] = func_00137DF8(reader);
            ((f32*)destination)[1] = func_00137DF8(reader);
            return;
        case 2: case 9: case 11:
            ((f32*)destination)[0] = func_00137DF8(reader);
            ((f32*)destination)[1] = func_00137DF8(reader);
            ((f32*)destination)[2] = func_00137DF8(reader);
            return;
        case 3: case 12:
            ((f32*)destination)[0] = func_00137DF8(reader);
            ((f32*)destination)[1] = func_00137DF8(reader);
            ((f32*)destination)[2] = func_00137DF8(reader);
            ((f32*)destination)[3] = func_00137DF8(reader);
            return;
        case 4:
            ((f32*)destination)[0] = func_00137DF8(reader);
            ((f32*)destination)[1] = func_00137DF8(reader);
            ((f32*)destination)[2] = func_00137DF8(reader);
            ((f32*)destination)[3] = 1.0f;
            return;
        case 13: *(s32*)destination = func_00137C40(reader); return;
        default:
            *(s32*)destination = func_00137D10(reader);
            return;
        }
    }

    {
        u32 target = descriptor_target(descriptor, object);
        u32 method;
        ResourceValue value;
        if (target == 0) return;
        if (flags & 0x100u) {
            method = (u32)descriptor->offset;
        } else {
            u32 vtable = *(u32*)object;
            method = vtable ? *(u32*)(vtable + (u32)descriptor->offset) : 0;
        }
        if (method == 0) return;
        read_value(reader, descriptor->type, &value);
        {
            extern void sly1_native_loader_stage(u32, u32, u32);
            sly1_native_loader_stage(162u, method, (u32)descriptor);
        }
        sly1_native_call_regs4(method, target, value.payload, 0, 0);
        func_0011B060(&value, 0);
    }
}
