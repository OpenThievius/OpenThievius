/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only buffered-reader methods mirrored from matching Sly C. */
#include "common.h"

void func_00137A60(void* self, s32 size, void* destination);

s32 func_00137B90(void* self) {
    u8* reader = (u8*)self;
    s32 remaining = *(s32*)(reader + 0x1C);
    if (remaining >= 1) {
        u8* cursor = (u8*)*(u32*)(reader + 0x18);
        s32 value = cursor[0];
        *(u32*)(reader + 0x18) += 1;
        *(s32*)(reader + 0x1C) = remaining - 1;
        return value;
    }
    u8 value = 0;
    func_00137A60(self, 1, &value);
    return value;
}

s32 func_00137BE0(void* self) {
    u8* reader = (u8*)self;
    s32 remaining = *(s32*)(reader + 0x1C);
    if (remaining >= 2) {
        u8* cursor = (u8*)*(u32*)(reader + 0x18);
        s32 value = (s32)cursor[0] | ((s32)cursor[1] << 8);
        *(u32*)(reader + 0x18) += 2;
        *(s32*)(reader + 0x1C) = remaining - 2;
        return value;
    }
    u16 value = 0;
    func_00137A60(self, 2, &value);
    return value;
}

u32 func_00137C40(void* self) {
    u8* reader = (u8*)self;
    s32 remaining = *(s32*)(reader + 0x1C);
    if (remaining >= 4) {
        u8* cursor = (u8*)*(u32*)(reader + 0x18);
        u32 value = (u32)cursor[0] | ((u32)cursor[1] << 8) |
                    ((u32)cursor[2] << 16) | ((u32)cursor[3] << 24);
        *(u32*)(reader + 0x18) += 4;
        *(s32*)(reader + 0x1C) = remaining - 4;
        return value;
    }
    u32 value = 0;
    func_00137A60(self, 4, &value);
    return value;
}

s32 func_00137CC0(void* self) {
    s32 value = func_00137B90(self);
    return value < 0x80 ? value : value - 0x100;
}

s32 func_00137D10(void* self) {
    u8* reader = (u8*)self;
    s32 remaining = *(s32*)(reader + 0x1C);
    if (remaining >= 2) {
        u8* cursor = (u8*)*(u32*)(reader + 0x18);
        s32 value = (s32)cursor[0] | ((s32)cursor[1] << 8);
        if (value >= 0x8000) value -= 0x10000;
        *(u32*)(reader + 0x18) += 2;
        *(s32*)(reader + 0x1C) = remaining - 2;
        return value;
    }
    if (remaining < 0) return -1;
    s16 value = 0;
    func_00137A60(self, 2, &value);
    if (*(s32*)(reader + 0x1C) < 0) return -1;
    return value;
}

s32 func_00137D78(void* self) {
    u32 value = func_00137C40(self);
    return (s32)value;
}

f32 func_00137DF8(void* self) {
    union { u32 bits; f32 value; } result;
    result.bits = func_00137C40(self);
    return result.value;
}

void func_00137E70(void* self, void* destination) {
    func_00137A60(self, 12, destination);
}

void func_00137E90(void* self, void* destination) {
    func_00137A60(self, 0x10, destination);
}

void func_00137EB0(void* self, u8* destination) {
    func_00137A60(self, 0xC, destination);
    func_00137A60(self, 0xC, destination + 0x10);
    func_00137A60(self, 0xC, destination + 0x20);
}
