/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only first-pass subsystem dispatch from the retail frame callback. */
#include "common.h"

extern u8* D_002623C0;
extern u8 D_0026C6C8[];
extern u8 D_0026C948[];
extern u8 D_0026CBC8[];
extern u8 D_0026CE48[];
extern u8 D_00274118[];
extern f32 D_00261868[];
s32 func_00160B28(u8* root);
void func_0016EC20(void* state, s32 mask);
s32 func_0016EED0(void* state);
s32 func_0016EC98(void* state);
void func_00195958(void* self);
void func_001961C0(void* self);
void func_001BFCD8(s32 id, void* a1, s32 a2, s32 a3, f32 f12, f32 f13,
                   f32 f14, f32 f15, f32 f16, s32 a4, s32 a5);
void sly1_native_call_method(u32 address, u32 object);

static void call_vtable_slot(u8* object, s32 slot)
{
    u32 vtable;
    u32 method;
    if (object == 0) return;
    vtable = *(u32*)object;
    if (vtable == 0) return;
    method = *(u32*)(vtable + (u32)slot * 4u);
    if (method >= 0x00100000u && method < 0x00220000u)
        sly1_native_call_method(method, (u32)object);
}

static s32 is_special_state(s32 state)
{
    switch (state) {
    case 0x307: case 0x204: case 0x106: case 0x304:
    case 0x502: case 0x407: case 0x504:
        return 1;
    default:
        return 0;
    }
}

int func_001943E8(void* self)
{
    u8* root = D_002623C0;
    u8* menu = (u8*)self;
    u8* pad = (u8*)0x00262C70u;
    u8* row;
    s32 kind;
    s32 combined_state;
    s32 slot = 15;
    s32 page;
    s32 count;
    s32 movement;
    u16 flags;
    s32 ready;

    if (self == 0 || root == 0) return 0;
    kind = *(s32*)(menu + 0x260);
    combined_state = (*(s32*)(root + 0x19D8) << 8) |
                     (*(s32*)(root + 0x19DC) & 0xFF);
    if (!is_special_state(combined_state) && kind < 1)
        slot = 14;

    call_vtable_slot(D_0026C6C8, slot);
    call_vtable_slot(D_0026C948, slot);
    call_vtable_slot(D_0026CBC8, slot);
    call_vtable_slot(D_0026CE48, slot);

    if (kind == 0 || kind == 11 || kind == 17) {
        *(s32*)(D_00274118 + 0x27C) = func_00160B28(root);
        *(u32*)(D_00274118 + 0x264) = (u32)(D_00274118 + 0x27C);
        call_vtable_slot(D_00274118, 14);
    }

    /* D_00269D50 has 0x18-byte rows: vertical at +0xC, item count at +0x10. */
    page = *(s32*)(menu + 0x260);
    if (page < 0 || page >= 33) return 0;
    row = (u8*)(0x00269D50u + (u32)page * 0x18u);
    count = *(s32*)(row + 0x10);
    if (count > 0) {
        movement = *(s32*)(row + 0x0C) != 0
            ? func_0016EED0(pad) : func_0016EC98(pad);
        if (movement > 0 || movement < 0) {
            s32 cursor = *(s32*)(menu + 0x264);
            cursor = movement > 0 ? (cursor + 1) % count : (cursor + count - 1) % count;
            *(s32*)(menu + 0x264) = cursor;
            func_001BFCD8(0x79, 0, 0, 0, 3000.0f, 300.0f,
                          1.0f, 0.0f, 0.0f, 0, 0);
            return 0;
        }
    }

    ready = *(s32*)(menu + 0x274) == 0 && *(s32*)(menu + 0x250) == 2 &&
            D_00261868[0] - *(f32*)(menu + 0x254) > 0.1f;
    if (!ready) return 0;
    flags = *(u16*)(pad + 0xAA);
    if ((flags & 0x0810u) != 0) {
        func_0016EC20(pad, 0x0810);
        func_001961C0(self);
    } else if ((flags & 0x0080u) != 0) {
        func_0016EC20(pad, 0x0080);
    } else if ((flags & 0x0040u) != 0) {
        func_0016EC20(pad, 0x0040);
        func_00195958(self);
    }
    return 0;
}
