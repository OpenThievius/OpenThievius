/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only resource value parser backfill, separate from matched C. */
#include "common.h"


void* func_0018D500(u32 size);
void* func_0011BA28(void* reader);
s32 func_00137C40(void* reader);
s32 func_00137D10(void* reader);
void func_0011E680(void* object, void* descriptor, void* reader);
void* func_0011C240(void* pool);
u8* func_001F5C0C(u8* destination, int value, int byte_count);

void* func_0011AF28(void) {
    u32 node = (u32)func_0011C240((void*)0x0025C250u);
    if (node == 0) return 0;
    *(u32*)node = 0xFFFFFFFFu;
    *(u32*)(node + 4) = 0;
    *(u32*)(node + 8) = 0;
    return (void*)node;
}

void* func_0011C498(void) {
    u32 node = (u32)func_0011C240((void*)0x0025C290u);
    if (node == 0) return 0;
    *(u32*)node = 0;
    *(u32*)(node + 4) = 0xFFFFFFFFu;
    *(u32*)(node + 8) = 0;
    *(u32*)(node + 12) = 0;
    return (void*)node;
}

void* func_0011B008(void* value) {
    u32* words = (u32*)value;
    words[0] = 0xFFFFFFFFu;
    words[1] = words[2] = words[3] = 0;
    return value;
}

void func_0011B018(void* destination, void* source) {
    u32* dst = (u32*)destination;
    u32* src = (u32*)source;
    dst[0] = src[0];
    dst[1] = src[1];
}

void func_0011B060(void* value, s32 mode) { (void)value; (void)mode; }

void func_0011B0A8(void* destination, void* source) {
    u32* dst = (u32*)destination;
    u32* src = (u32*)source;
    dst[0] = src[0];
    dst[1] = src[1];
}

void func_0011B538(void* value, s32 payload) { ((u32*)value)[1] = (u32)payload; }
void func_0011B570(void* value, s32 payload) { ((u32*)value)[0] = 0x14; ((u32*)value)[1] = (u32)payload; }
void func_0011B5B0(void* value, f32 payload) { ((u32*)value)[0] = 0x15; *(f32*)((u8*)value + 4) = payload; }
void func_0011B5F0(void* value, s32 payload) { ((u32*)value)[0] = 0x1B; ((u32*)value)[1] = payload != 0; }
void func_0011B640(void* value, s32 payload) { ((u32*)value)[0] = 0x1C; ((u32*)value)[1] = (u32)payload; }
void func_0011B680(void* value, s32 payload) { ((u32*)value)[0] = 0x1D; ((u32*)value)[1] = (u32)payload; }
void func_0011B6C0(void* value, s32 payload) { ((u32*)value)[0] = 0x1E; ((u32*)value)[1] = (u32)payload; }
void func_0011B740(void* value, void* payload) { ((u32*)value)[0] = 0x16; ((u32*)value)[1] = (u32)payload; }
void func_0011B788(void* value, void* payload) { ((u32*)value)[0] = 0x17; ((u32*)value)[1] = (u32)payload; }
void func_0011B7D0(void* value, void* payload) { ((u32*)value)[0] = 0x18; ((u32*)value)[1] = (u32)payload; }
void func_0011B818(void* value, void* payload) { ((u32*)value)[0] = 0x19; ((u32*)value)[1] = (u32)payload; }
void func_0011B860(void* value, void* payload) { ((u32*)value)[0] = 0x1A; ((u32*)value)[1] = (u32)payload; }
void func_0011B8A8(void* value, void* payload) { ((u32*)value)[0] = 0x20; ((u32*)value)[1] = (u32)payload; }

void* func_0011A630(void) {
    u32 owner = (u32)func_0011C240((void*)0x0025C270u);
    if (owner == 0) return 0;
    func_001F5C0C((u8*)owner, 0, 0x1C);
    return (void*)owner;
}

void func_0011A6C0(void* list, s32 value) {
    u32* words = (u32*)list;
    s32 count = (s32)words[0];
    s32 i;
    if (count < 0 || count >= 320) return;
    for (i = 0; i < count; ++i) if ((s32)words[i + 1] == value) return;
    words[count + 1] = (u32)value;
    words[0] = (u32)(count + 1);
}

void* func_0011A358(void* output, s32 owner, s32 kind, void* body) {
    u32 node = (u32)func_0011C498();
    u32* value = (u32*)output;
    if (node != 0) {
        *(u32*)node = (u32)kind;
        *(u32*)(node + 4) = ((u32*)body)[0];
        *(u32*)(node + 8) = ((u32*)body)[1];
        *(u32*)(node + 12) = *(u32*)((u32)owner + 0x14);
        *(u32*)((u32)owner + 0x14) = node;
    }
    value[0] = 0xFFFFFFFFu;
    value[1] = 0x22;
    return output;
}

void func_0011A080(void* output, void* input, s32 context) {
    u8 value[16];
    s32 kind = *(s32*)input;
    (void)context;
    func_0011B008(value);
    switch (kind) {
    case 0: case 20: case 21: case 22: case 23: case 24: case 25: case 26:
    case 27: case 29: case 32: case 33:
        func_0011B0A8(value, input);
        break;
    default:
        break;
    }
    func_0011B018(output, value);
    func_0011B060(value, 2);
}

void func_0011EC40(void* object, void* reader) {
    s32 id;
    static u32 traced_properties;
    extern void sly1_native_loader_stage(u32 stage, u32 object, u32 reader);
    while ((id = func_00137D10(reader)) >= 0) {
        if (traced_properties < 128) {
            sly1_native_loader_stage(135, (u32)id, (u32)reader);
            ++traced_properties;
        }
        func_0011E680(object, (void*)(0x002505D8u + ((u32)id << 5)), reader);
        if (traced_properties <= 128)
            sly1_native_loader_stage(136, (u32)id, (u32)reader);
    }
    sly1_native_loader_stage(137, (u32)id, (u32)reader);
}
void func_00138518(void* reader);
s32 func_0011A630(void);
void func_0011A6C0(void* list, s32 value);
void func_0011B008(void* value);
void func_0011B018(void* dst, void* src);
void func_0011B060(void* value, s32 mode);
void func_0011B0A8(void* dst, void* src);
void func_0011B538(void* value, s32 kind);
void func_0011A358(void* dst, s32 head, s32 count, void* body);
void func_0011A080(void* dst, s32 value, s32 key);
void* func_0011AF28(void);
void func_0011B570(void* node, s32 value);
void func_0011B5B0(void* node, f32 value);
void func_0011B5F0(void* node, s32 value);
void func_0011B640(void* node, s32 value);
void func_0011B680(void* node, s32 value);
void func_0011B6C0(void* node, s32 value);
void func_0011B740(void* node, void* value);
void func_0011B788(void* node, void* value);
void func_0011B7D0(void* node, void* value);
void func_0011B818(void* node, void* value);
void func_0011B860(void* node, void* value);
void func_0011B8A8(void* node, void* value);
void* func_0018D500(u32 size);
s32 func_00137B90(void* reader);
s32 func_00137D78(void* reader);
f32 func_00137DF8(void* reader);
s32 func_00137C40(void* reader);

void* func_0011BA28(void* reader) {
    s32 encoded_tag = func_00137B90(reader);
    s32 kind;
    s32 child_flag;
    u32 node;

    if (encoded_tag == 0xFF) return 0;
    kind = encoded_tag & 0x7F;
    child_flag = encoded_tag & 0x80;
    node = (u32)func_0011AF28();
    *(s32*)node = kind;

    if (*(volatile s32*)0x00275714u != 0) {
        func_00137C40(reader);
        func_00137C40(reader);
        func_00137C40(reader);
        func_00137C40(reader);
        func_00137C40(reader);
    }

    switch (kind) {
    case 20:
        func_0011B570((void*)node, func_00137D78(reader));
        break;
    case 21:
        func_0011B5B0((void*)node, func_00137DF8(reader));
        break;
    case 27:
        func_0011B5F0((void*)node, func_00137C40(reader));
        break;
    case 32:
        func_0011B8A8((void*)node, (void*)(u32)func_00137C40(reader));
        break;
    case 28:
        func_0011B640((void*)node, func_00137C40(reader));
        break;
    case 29:
        func_0011B680((void*)node, func_00137C40(reader));
        break;
    case 22: case 24: case 26: {
        f32* values = (f32*)func_0018D500(12);
        values[0] = func_00137DF8(reader);
        values[1] = func_00137DF8(reader);
        values[2] = func_00137DF8(reader);
        *(s32*)node = -1;
        if (kind == 22) func_0011B740((void*)node, values);
        else if (kind == 24) func_0011B7D0((void*)node, values);
        else func_0011B860((void*)node, values);
        break;
    }
    case 23: {
        f32* values = (f32*)func_0018D500(60);
        s32 i;
        for (i = 0; i < 15; ++i) values[i] = func_00137DF8(reader);
        *(s32*)node = -1;
        func_0011B788((void*)node, values);
        break;
    }
    case 25: {
        f32* values = (f32*)func_0018D500(8);
        values[0] = func_00137DF8(reader);
        values[1] = func_00137DF8(reader);
        *(s32*)node = -1;
        func_0011B818((void*)node, values);
        break;
    }
    case 30:
        func_0011B6C0((void*)node, (s32)func_0011BA28(reader));
        break;
    default:
        break;
    }

    *(u32*)(node + 8) = child_flag ? (u32)func_0011BA28(reader) : 0;
    return (void*)node;
}

struct ResourcePair {
    s32 key;
    s32 value;
};

void func_001C13E8(void* output, s32 index, void* context) {
    u8 value[16];
    u8 pair[16];
    u8* self = (u8*)context;
    struct ResourcePair* entries = (struct ResourcePair*)*(u32*)(self + 0x1EF4);
    struct ResourcePair* entry = &entries[index];

    func_0011B008(value);
    if (entry->key != 0 && entry->value == 0) {
        entry->value = func_0011A630();
        func_0011A080(pair, entry->key, entry->value);
        func_0011B0A8(value, pair);
        func_0011B060(pair, 2);
    }
    func_0011B538(value, 0x22);
    func_0011B018(output, value);
    func_0011B060(value, 2);
}

void func_001C1190(void* context, void* reader) {
    u8 empty[16];
    u8 value[16];
    u8 child[16];
    u8* self = (u8*)context;
    s32 count = func_00137C40(reader);
    s32 first_count;
    s32 second_count;
    s32 index;
    s32 head;

    *(s32*)(self + 0x1EF0) = count;
    *(u32*)(self + 0x1EF4) = (u32)func_0018D500((u32)count << 3);
    first_count = func_00137C40(reader);
    second_count = func_00137C40(reader);

    for (index = 0; index < count; ++index) {
        struct ResourcePair* entry =
            &((struct ResourcePair*)*(u32*)(self + 0x1EF4))[index];
        entry->key = (s32)func_0011BA28(reader);
        entry->value = 0;
        if (*(volatile s32*)0x00275714u != 0) {
            func_00138518(reader);
            func_00138518(reader);
            func_00138518(reader);
        }
    }
    if (*(volatile s32*)0x00275714u != 0) {
        for (index = 0; index < first_count; ++index) func_00138518(reader);
        for (index = 0; index < second_count; ++index) func_00138518(reader);
    }

    head = func_0011A630();
    *(u32*)(self + 0x2C) = (u32)head;
    func_0011A6C0((void*)0x00259F38u, head);
    *(s32*)(self + 0x1EF8) = first_count;

    func_0011B008(empty);
    func_0011B538(empty, 0);
    func_0011A358(value, head, first_count, empty);
    func_0011B060(value, 2);
    for (index = 0; index < count; ++index) {
        func_0011B008(value);
        func_001C13E8(child, index, context);
        func_0011B0A8(value, child);
        func_0011B060(child, 2);
        func_0011B060(value, 2);
    }
    func_0011B060(empty, 2);
}
