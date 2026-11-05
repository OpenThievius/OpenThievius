/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only resource reader for the collision-mesh prefix. */
#include "common.h"

s32 func_00137B90(void* reader);
s32 func_00137BE0(void* reader);
s32 func_00137D10(void* reader);
f32 func_00137DF8(void* reader);
void func_00137E70(void* reader, void* destination);
void* func_0018D5C0(u32 size);

static s32 stream_failed(void* reader) {
    return *(s32*)((u8*)reader + 0x1C) < 0;
}

static s32 read_count(void* reader, s32 limit) {
    const s32 count = func_00137BE0(reader);
    return count >= 0 && count <= limit ? count : -1;
}

void func_00137FB0(void* reader, void* mesh) {
    u8* out = (u8*)mesh;
    const f32 radius = func_00137DF8(reader);
    const s32 vertex_count = read_count(reader, 8192);

    *(f32*)(out + 0x00) = radius;
    *(s32*)(out + 0x04) = vertex_count < 0 ? 0 : vertex_count;
    *(u32*)(out + 0x08) = vertex_count > 0
        ? (u32)func_0018D5C0((u32)vertex_count * 0x10u) : 0;
    if (vertex_count < 0) return;

    for (s32 i = 0; i < vertex_count; ++i) {
        func_00137E70(reader, (u8*)*(u32*)(out + 0x08) + (u32)i * 0x10u);
        if (stream_failed(reader)) return;
    }

    const s32 face_count = read_count(reader, 16384);
    *(s32*)(out + 0x0C) = face_count < 0 ? 0 : face_count;
    *(u32*)(out + 0x10) = face_count > 0
        ? (u32)func_0018D5C0((u32)face_count * 0x20u) : 0;
    *(u32*)(out + 0x1C) = face_count > 0
        ? (u32)func_0018D500(face_count * 0x10) : 0;
    *(u32*)(out + 0x20) = face_count > 0
        ? (u32)func_0018D500(face_count * 4) : 0;

    if (face_count < 0) return;
    const s32 link_capacity = read_count(reader, 65535);
    *(s32*)(out + 0x14) = link_capacity < 0 ? 0 : link_capacity;
    *(u32*)(out + 0x18) = link_capacity > 0
        ? (u32)func_0018D5C0((u32)link_capacity * 0x10u) : 0;
    if (link_capacity < 0) return;

    u32 link_cursor = *(u32*)(out + 0x18);
    for (s32 i = 0; i < face_count; ++i) {
        u8* face = (u8*)*(u32*)(out + 0x10) + (u32)i * 0x20u;
        const s16 a = (s16)func_00137D10(reader);
        const s16 b = (s16)func_00137D10(reader);
        const s16 c = (s16)func_00137D10(reader);
        if (stream_failed(reader)) return;
        const u8* vertices = (const u8*)*(u32*)(out + 0x08);
        if (a >= 0 && b >= 0 && c >= 0 &&
            a < vertex_count && b < vertex_count && c < vertex_count) {
            const f32* va = (const f32*)(vertices + (u32)a * 0x10u);
            const f32* vb = (const f32*)(vertices + (u32)b * 0x10u);
            const f32* vc = (const f32*)(vertices + (u32)c * 0x10u);
            const f32 abx = vb[0] - va[0], aby = vb[1] - va[1], abz = vb[2] - va[2];
            const f32 acx = vc[0] - va[0], acy = vc[1] - va[1], acz = vc[2] - va[2];
            f32 nx = aby * acz - abz * acy;
            f32 ny = abz * acx - abx * acz;
            f32 nz = abx * acy - aby * acx;
            const f32 length = __builtin_sqrtf(nx * nx + ny * ny + nz * nz);
            if (length > 0.0f) { nx /= length; ny /= length; nz /= length; }
            ((f32*)face)[0] = nx;
            ((f32*)face)[1] = ny;
            ((f32*)face)[2] = nz;
            ((f32*)face)[3] = 0.0f;
        }
        *(s16*)(face + 0x1E) = a;

        /* Retail consumes a scalar and a 12-byte face vector before the byte
         * link count. The scalar is faceData[i], the vector is faceData[i]+4. */
        *(f32*)((u8*)*(u32*)(out + 0x1C) + (u32)i * 0x10u) = func_00137DF8(reader);
        func_00137E70(reader, (u8*)*(u32*)(out + 0x1C) + (u32)i * 0x10u + 4u);
        if (stream_failed(reader)) return;
        const s32 edge_count = func_00137B90(reader);
        if (stream_failed(reader)) return;
        if (edge_count <= 0) continue;

        u32 first = 0;
        u32 previous = 0;
        for (s32 edge = 0; edge < edge_count; ++edge) {
            const s16 edge_index = (s16)func_00137D10(reader);
            if (stream_failed(reader)) return;
            if ((u32)(link_cursor - *(u32*)(out + 0x18)) >= (u32)link_capacity * 0x10u) {
                /* Keep consuming the retail stream if malformed data exceeds
                 * the declared edge-node capacity, without writing past it. */
                continue;
            }
            u8* node = (u8*)link_cursor;
            link_cursor += 0x10u;
            *(s16*)(node + 0x00) = edge_index;
            *(s16*)(node + 0x02) = (s16)edge;
            *(s16*)(node + 0x04) = (s16)i;
            *(s32*)(node + 0x08) = previous;
            if (previous != 0) *(u32*)(previous + 0x0C) = link_cursor - 0x10u;
            else first = link_cursor - 0x10u;
            previous = link_cursor - 0x10u;
        }
        *(u32*)(face + 0x14) = first;
        *(u32*)(face + 0x18) = previous;
    }
}
