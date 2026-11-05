/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only loader-message backfills derived from NTSC-U retail assembly. */
#include "common.h"

extern u32 D_0027051C;
extern u32 D_0026FA68;
extern u32 D_00275710;
extern u32 D_00275B04;
extern f32 D_00261868;
extern u8 D_00275F80[];

void func_001DDAD0(void);
void func_001F1050(void* object, s32 mode);
void func_001F0220(void* state, s32 mode);
void func_001EFFA0(u8* state, u8* message, s32 count);
void sly1_native_loader_stage(u32 stage, u32 object, u32 reader);

struct NativeLoaderMessage {
    s32 kind;
    void* object;
    void* value;
    u8 pad0[3];
    s32 last;
};

void func_001F0428(void* object, void* value, s32 count) {
    struct NativeLoaderMessage message;
    message.kind = 1;
    message.object = object;
    message.value = value;
    message.pad0[0] = 0;
    message.pad0[1] = 0;
    message.pad0[2] = 0;
    message.last = 0;
    sly1_native_loader_stage(8, (u32)object, (u32)object);
    func_001EFFA0(D_00275F80, (u8*)&message, count);
}

void func_001EFFA0(u8* state, u8* message, s32 count) {
    u32 record;
    u32 reference;
    u32 valid = 0;
    u32 flags;

    if (*(u32*)(state + 4) != 0) return;

    if (D_0027051C != 0) {
        u8* context = (u8*)D_0027051C;
        if (*(s32*)(context + 0x2D0) != 2) func_001DDAD0();
    }

    record = *(u32*)(message + 4);
    if (record != 0) {
        if (record == 0x00275B04u) {
            valid = 1;
        } else {
            reference = D_00275B04;
            if (reference != 0) {
                valid = ((*(u32*)(reference + 8) ^ *(u32*)(reference + 0x18)) ^
                         (*(u32*)(record + 8) ^ *(u32*)(record + 0x18))) == 0;
            }
        }
    }

    flags = *(u32*)(message + 0x10);
    *(u32*)(message + 0x10) = valid ? (flags | 8u) : (flags & ~8u);

    *(u32*)(state + 0x10) = *(u32*)(message + 0x00);
    *(u32*)(state + 0x14) = *(u32*)(message + 0x04);
    *(u32*)(state + 0x18) = *(u32*)(message + 0x08);
    *(u32*)(state + 0x1C) = *(u32*)(message + 0x0C);
    *(u32*)(state + 0x20) = *(u32*)(message + 0x10);
    *(u32*)(state + 0x24) = (u32)count;
    func_001F0220(state, D_00275710 != 0 ? 1 : 2);
}

void func_001F0220(u8* state, s32 mode) {
    s32 previous = *(s32*)(state + 4);
    if (previous == mode) return;

    if (previous == 2 && D_0026FA68 != 0) {
        func_001F1050((void*)D_0026FA68, 0);
    }

    if (mode == 0) {
        *(u32*)(state + 0x0C) = 0;
        *(s32*)(state + 4) = 0;
    } else if (mode == 2) {
        if (*(u32*)(state + 0x10) != 0) {
            /* Retail hands the pending record to the deferred stream loader. */
            extern void func_001E4E80(void* block, s32 a1, s32 a2, s32 a3, s32 a4);
            func_001E4E80((void*)0x00275AF0u,
                          *(s32*)(state + 0x14), *(s32*)(state + 0x18),
                          *(s32*)(state + 0x1C), *(s32*)(state + 0x20));
        }
        *(u32*)(state + 0x10) = 0;
        *(f32*)(state + 0x0C) = 1.0f;
        *(u32*)(state + 0x28) = 0;
        *(s32*)(state + 4) = 2;
    } else {
        *(s32*)(state + 4) = mode;
    }

    *(f32*)(state + 8) = D_00261868;
}
