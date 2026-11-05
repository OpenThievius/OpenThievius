/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only resource reader for the collision-mesh prefix. */
#include "common.h"

void func_00137A60(void* reader, s32 size, void* destination);
s32 func_00137B90(void* reader);
s32 func_00137BE0(void* reader);
f32 func_00137DF8(void* reader);
void func_00137E70(void* reader, void* destination);
void* func_0018D5C0(u32 size);

static s32 read_count(void* reader, s32 limit) {
    const s32 count = func_00137BE0(reader);
    return count >= 0 && count <= limit ? count : -1;
}

void func_00137FB0(void* reader, void* mesh) {
    u8* out = (u8*)mesh;
    const f32 radius = func_00137DF8(reader);
    const s32 vertex_count = read_count(reader, 8192);
    const s32 face_count = read_count(reader, 16384);
    const s32 total_links = read_count(reader, 65535);
    (void)total_links;

    *(f32*)(out + 0x00) = radius;
    *(s32*)(out + 0x04) = vertex_count < 0 ? 0 : vertex_count;
    *(s32*)(out + 0x0C) = face_count < 0 ? 0 : face_count;
    *(u32*)(out + 0x08) = vertex_count > 0
        ? (u32)func_0018D5C0((u32)vertex_count * 0x10u) : 0;
    *(u32*)(out + 0x10) = face_count > 0
        ? (u32)func_0018D5C0((u32)face_count * 0x20u) : 0;

    if (vertex_count < 0 || face_count < 0) return;

    for (s32 i = 0; i < vertex_count; ++i) {
        func_00137E70(reader, (u8*)*(u32*)(out + 0x08) + (u32)i * 0x10u);
    }

    for (s32 i = 0; i < face_count; ++i) {
        u8 indices[6];
        u8 bounds[16];
        u8 link_count = 0;
        func_00137A60(reader, 6, indices);
        func_00137A60(reader, 16, bounds);
        func_00137A60(reader, 1, &link_count);

        u8* face = (u8*)*(u32*)(out + 0x10) + (u32)i * 0x20u;
        *(u16*)(face + 0x00) = *(u16*)(indices + 0x00);
        *(u16*)(face + 0x02) = *(u16*)(indices + 0x02);
        *(u16*)(face + 0x04) = *(u16*)(indices + 0x04);
        *(f32*)(face + 0x08) = *(f32*)bounds;
        *(f32*)(face + 0x0C) = *(f32*)(bounds + 4);
        *(f32*)(face + 0x10) = *(f32*)(bounds + 8);
        *(f32*)(face + 0x14) = *(f32*)(bounds + 12);

        for (s32 link = 0; link < link_count; ++link) {
            u8 link_record[6];
            func_00137A60(reader, 6, link_record);
        }
    }
}
