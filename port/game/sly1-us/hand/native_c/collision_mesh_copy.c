/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only deep copy for collision meshes when retail VU code is unavailable. */
#include "common.h"

void* func_0018D568(s32 size, void* source);

void func_00160E80(void* mesh) {
    u8* bytes = (u8*)mesh;
    for (u32 offset = 4; offset <= 0x18; offset += 4) {
        *(u32*)(bytes + offset) = 0;
    }
}

void func_00160EA0(void* source_mesh, s32 mode, void* destination_mesh) {
    const u8* source = (const u8*)source_mesh;
    u8* destination = (u8*)destination_mesh;
    s32 vertex_count = *(const s32*)(source + 0x04);
    s32 face_count = *(const s32*)(source + 0x0C);
    u32 source_vertices = *(const u32*)(source + 0x08);
    u32 source_faces = *(const u32*)(source + 0x10);
    u32 source_face_data = *(const u32*)(source + 0x1C);
    u32 source_face_indices = *(const u32*)(source + 0x20);

    *(s32*)(destination + 0x04) = vertex_count;
    *(u32*)(destination + 0x08) = vertex_count > 0 && vertex_count <= 8192
        ? (u32)func_0018D568(vertex_count * 0x10, (void*)source_vertices) : 0;
    *(s32*)(destination + 0x0C) = face_count;
    *(u32*)(destination + 0x10) = face_count > 0 && face_count <= 16384
        ? (u32)func_0018D568(face_count * 0x20, (void*)source_faces) : 0;
    *(u32*)(destination + 0x1C) = face_count > 0 && face_count <= 16384
        ? (u32)func_0018D568(face_count * 0x10, (void*)source_face_data) : 0;
    *(u32*)(destination + 0x20) = face_count > 0 && face_count <= 16384
        ? (u32)func_0018D568(face_count * 4, (void*)source_face_indices) : 0;
    *(u32*)(destination + 0x14) = *(const u32*)(source + 0x14);
    *(u32*)(destination + 0x18) = *(const u32*)(source + 0x18);

    /* Every current retail caller passes zero; transformed clones need the
     * separate matrix helper path implemented when that call site appears. */
    (void)mode;
}
