/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only C02 base-constructor backfill. Upstream C exists but its source unit
 * cannot yet be compiled as C by hostgen; remove this when that unit is wired. */
#include "common.h"

void func_00152438(void* list, s32 link_offset);
void func_00182C18(void* object);

void func_00123ED0(void* object) {
    u8* self = (u8*)object;
    const s32 has_parent = *(u32*)(self + 0x18) != 0;

    func_00152438(self + 0x34, 0x1C);
    func_00152438(self + 0x60, 0x58);

    /* C02's two packed two-bit state fields occupy byte 0x2CB. */
    *(u8*)(self + 0x2C8) = 0;
    *(u8*)(self + 0x2C9) = 0xFF;
    *(u8*)(self + 0x2CA) = 0xFF;
    *(u8*)(self + 0x2CB) = has_parent ? 0x01 : 0x0A;

    func_00182C18(object);
    *(f32*)(self + 0x80) = 3.402823466e+38f;
    *(s32*)(self + 0x88) = 0x0FFFFFFF;
    *(f32*)(self + 0x84) = 3.402823466e+38f;
    *(u32*)(self + 0x20C) = 0x00260E70u;
    *(u32*)(self + 0x210) = 0x00260E80u;
    *(u32*)(self + 0x214) = 0x00260E90u;
    *(u32*)(self + 0x218) = 0x00260EA0u;
    *(u32*)(self + 0x21C) = 0x00260EB0u;
    *(u32*)(self + 0x220) = 0x00260EC0u;

    /* The retail matrix at 0x002483D0 is the identity transform. */
    for (u32 matrix = 0; matrix < 3; ++matrix) {
        u8* row = self + 0xD0 + matrix * 0x10;
        *(f32*)(row + 0) = matrix == 0 ? 1.0f : 0.0f;
        *(f32*)(row + 4) = matrix == 1 ? 1.0f : 0.0f;
        *(f32*)(row + 8) = matrix == 2 ? 1.0f : 0.0f;
        *(f32*)(row + 12) = 0.0f;
    }
    for (u32 matrix = 0; matrix < 2; ++matrix) {
        const u32 offset = matrix == 0 ? 0x110u : 0x1A0u;
        for (u32 row_index = 0; row_index < 3; ++row_index) {
            u8* row = self + offset + row_index * 0x10;
            *(f32*)(row + 0) = row_index == 0 ? 1.0f : 0.0f;
            *(f32*)(row + 4) = row_index == 1 ? 1.0f : 0.0f;
            *(f32*)(row + 8) = row_index == 2 ? 1.0f : 0.0f;
            *(f32*)(row + 12) = 0.0f;
        }
    }
    func_00152438(self + 0x1E0, 8);
}
