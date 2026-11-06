/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only Sly controller setup and polling over the SDL pad report bridge. */
#include "common.h"

extern u32 D_00262C68;
extern u32 D_00262C6C;
extern u8 D_00262C70[];
extern u8 D_00261858[];
extern u32 D_002C2A14;
extern u32 D_002C2A18;
extern u32 D_002C2A1C;
int sly1_native_pad_read(u32 port, u32 report, u32 size);

static f32 axis_value(u8 value) {
    f32 axis = ((f32)value - 128.0f) / 127.0f;
    if (axis < -1.0f) return -1.0f;
    if (axis > 1.0f) return 1.0f;
    return axis;
}

static f32 magnitude(f32 x, f32 y) {
    f32 ax = x < 0.0f ? -x : x;
    f32 ay = y < 0.0f ? -y : y;
    return ax > ay ? ax : ay;
}

static void clear_native_input(u8* self) {
    *(u16*)(self + 0xA8) = 0;
    *(u16*)(self + 0xAA) = 0;
    *(u16*)(self + 0xAC) = 0;
    *(f32*)(self + 0x40) = 0.0f;
    *(f32*)(self + 0x44) = 0.0f;
    *(f32*)(self + 0x48) = 0.0f;
    *(f32*)(self + 0x74) = 0.0f;
    *(f32*)(self + 0x78) = 0.0f;
    *(f32*)(self + 0x7C) = 0.0f;
}

void* func_0018D4B0(s32 size) {
    if (size == 0) return 0;
    u32 result = D_002C2A14;
    u32 next = result + ((u32)size + 15u & ~15u);
    D_002C2A14 = next;
    D_002C2A18 = next;
    D_002C2A1C = next;
    return (void*)result;
}

void func_0016E148(void) {
    u32 flags = D_00262C6C;
    if ((flags & 0x84u) != 0) D_00262C68 = 0;
    else if ((flags & 1u) != 0) D_00262C68 = 5;
    else if ((flags & 2u) != 0) D_00262C68 = 4;
    else D_00262C68 = 7;
}

void func_0016E1A0(void* object, s32 port, s32 slot) {
    u8* self = (u8*)object;
    for (u32 i = 0; i < 0xC4u; ++i) self[i] = 0;
    *(s32*)(self + 0x04) = port;
    *(s32*)(self + 0x08) = slot;
    *(s32*)(self + 0x10) = 0;
    *(s32*)(self + 0x14) = 1;
    *(s32*)(self + 0x18) = 0;
    *(f32*)(self + 0x1C) = *(f32*)(D_00261858 + 0x10);
    u32 raw = (u32)func_0018D4B0(0x140);
    *(u32*)(self + 0x0C) = (raw + 0x3Fu) & ~0x3Fu;
    *(u32*)(self + 0xBC) = (u32)func_0018D4B0(0x20);
    *(s32*)(self + 0xC0) = 1;
}

void func_0016E0B8(void) {
    func_0016E148();
    func_0016E1A0(D_00262C70, 0, 0);
}

void func_0016E238(void* object) {
    u8* self = (u8*)object;
    if (*(s32*)(self + 0x14) == 0) return;
    u32 report = *(u32*)(self + 0x0C);
    if (report == 0 || !sly1_native_pad_read(*(u32*)(self + 4), report, 32)) {
        *(s32*)(self + 0x14) = 1;
        *(s32*)(self + 0x18) = 0;
        clear_native_input(self);
        return;
    }
    u8* bytes = (u8*)report;
    if (bytes[0] != 0) {
        *(s32*)(self + 0x14) = 1;
        *(s32*)(self + 0x18) = 0;
        clear_native_input(self);
        return;
    }

    /* The SDL adapter supplies an already-negotiated pressure-mode report.
     * Retail libpad advances these fields through modes 1/2 before reaching
     * mode 3; the native service performs that negotiation on the host. */
    if (*(s32*)(self + 0x14) != 3 || *(s32*)(self + 0x18) != 4) {
        *(s32*)(self + 0x14) = 3;
        *(s32*)(self + 0x18) = 4;
    }

    u16 old_buttons = *(u16*)(self + 0xA8);
    u16 buttons = (u16)~((u16)bytes[2] << 8 | bytes[3]);
    *(u16*)(self + 0xA8) = buttons;
    *(u16*)(self + 0xAA) = buttons & (u16)~old_buttons;
    *(u16*)(self + 0xAC) = old_buttons & (u16)~buttons;
    *(u8*)(self + 0x10) = bytes[1];
    *(f32*)(self + 0x20) = *(f32*)(D_00261858 + 0x10);
    if (buttons != 0) *(f32*)(self + 0x24) = *(f32*)(D_00261858 + 0x10);

    __builtin_memcpy(self + 0xAE, bytes + 8, 12);
    f32 left_x = axis_value(bytes[6]);
    f32 left_y = axis_value(bytes[7]);
    f32 right_x = axis_value(bytes[4]);
    f32 right_y = axis_value(bytes[5]);
    *(f32*)(self + 0x40) = left_x;
    *(f32*)(self + 0x44) = left_y;
    *(f32*)(self + 0x48) = magnitude(left_x, left_y);
    *(f32*)(self + 0x74) = right_x;
    *(f32*)(self + 0x78) = right_y;
    *(f32*)(self + 0x7C) = magnitude(right_x, right_y);
}

/* The platform pad service owns open/query behavior; no IOP padman task is
 * needed in the native runtime. */
int func_002040E8(void* unused) {
    (void)unused;
    return 1;
}
