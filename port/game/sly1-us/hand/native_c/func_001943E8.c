/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only first-pass subsystem dispatch from the retail frame callback. */
#include "common.h"

extern u8* D_002623C0;
extern u8 D_0026C6C8[];
extern u8 D_0026C948[];
extern u8 D_0026CBC8[];
extern u8 D_0026CE48[];
extern u8 D_00274118[];
s32 func_00160B28(u8* root);
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
    s32 kind;
    s32 combined_state;
    s32 slot = 15;

    if (self == 0 || root == 0) return 0;
    kind = *(s32*)((u8*)self + 0x260);
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
    return 0;
}
