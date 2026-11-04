/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only loader startup backfills translated from NTSC-U retail assembly. */
#include "common.h"

u8* func_001F5C0C(u8* destination, s32 value, s32 byte_count);
void func_001A7E90(void* context);
void func_001F0428(u32 object, u32 value, s32 count);

void func_00140D90(u8* queue) {
    union { f32 f; u32 u; } clear_value;
    u32* words;

    *(u32*)(queue + 4) = 0;
    *(u32*)(queue + 8) = 0;
    (void)func_001F5C0C(queue + 0x0C, 0, 0x200);
    *(u32*)(queue + 0x20C) = 0;

    clear_value.f = *(f32*)0x00249E3Cu;
    words = (u32*)(queue + 0x210);
    words[0] = clear_value.u;
    words[1] = clear_value.u;
    words[2] = clear_value.u;
    words[3] = clear_value.u;
    (void)func_001F5C0C(queue + 0x220, 0, 0x20C);
    *(u32*)(queue + 0x424) = 0xFFFFFFFFu;
    *(u32*)(queue + 0x428) = 0xFFFFFFFFu;
    *(u32*)(queue + 0x42C) = 0;
}

void func_001605E8(u8* context) {
    (void)func_001F5C0C(context, 0, 0x1A00);
    *(u32*)(context + 0x00) = 0x12;
    *(u32*)(context + 0x04) = 0x1A00;
    *(u32*)(context + 0x19D8) = 0;
    *(u32*)(context + 0x19DC) = 3;
    *(u32*)(context + 0x19E0) = 5;
    *(u32*)(context + 0x19F0) = 0;
    *(u32*)(context + 0x19FC) = 0xFFFFFFFFu;
    func_001A7E90(context);
}

void func_00160CC8(u8* destination) {
    (void)func_001F5C0C(destination, 0, 8);
}

void func_00151A68(u8* destination) {
    (void)func_001F5C0C(destination, 0, 0x24);
}

void func_001608E0(void) {
    u8* context = (u8*)(*(u32*)0x002623C0u);
    *(u32*)(context + 0x19D8) = 0;
    *(u32*)(context + 0x19F0) = 0;
    *(u32*)(context + 0x00) = 0x12;
    *(u32*)(context + 0x04) = 0x1A00;
    *(u32*)(context + 0x19DC) = 3;
    *(u32*)(context + 0x19E0) = 5;
    *(u32*)(context + 0x19FC) = 0xFFFFFFFFu;
    func_00140D90((u8*)0x00261420u);
}

void func_00160880(void) {
    (void)func_001605E8((u8*)(*(u32*)0x002623C0u));
    (void)func_001F5C0C((u8*)0x00269984u, 0, 4);
    func_00160CC8((u8*)0x002623D8u);
    func_00151A68((u8*)0x00261EF0u);
    *(u32*)0x00262C64u = 0;
    *(u32*)0x002623CCu = 0xFFFFFFFFu;
    func_001608E0();
}

void func_00160918(void) {
    func_00160880();
    func_001F0428(0x00247AB0u, 0xFFFFFFFFu, 0);
}

void func_00160070(void) {
    func_00160918();
}
