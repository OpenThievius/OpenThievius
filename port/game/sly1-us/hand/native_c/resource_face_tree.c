/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only face-tree reader. Matching decomp C exists but hostgen currently
 * classifies it as no-C; remove this backfill when that function is hostable. */
#include "common.h"

s32 func_00137BE0(void* reader);
void* func_0018D5C0(s32 size);
void sly1_native_loader_stage(u32 stage, u32 value, u32 current);

typedef struct Sly1TreeNode {
    u32 face;
    u32 back;
    u32 front;
} Sly1TreeNode;

typedef struct Sly1FaceTree {
    s32 count;
    s32 count2;
    u32 nodes;
} Sly1FaceTree;

typedef struct Sly1CollisionMesh {
    u32 radius;
    s32 vert_count;
    u32 verts;
    s32 face_count;
    u32 faces;
} Sly1CollisionMesh;

void func_00138278(void* reader, void* mesh, void* tree) {
    Sly1FaceTree* output = (Sly1FaceTree*)tree;
    Sly1CollisionMesh* input = (Sly1CollisionMesh*)mesh;
    s32 count = func_00137BE0(reader);
    s32 count2 = func_00137BE0(reader);
    sly1_native_loader_stage(190u, (u32)count, (u32)reader);
    sly1_native_loader_stage(191u, (u32)count2, (u32)input);
    sly1_native_loader_stage(192u, (u32)input->face_count, input->faces);

    output->count = count;
    output->count2 = count2;
    output->nodes = 0;
    if (count < 0 || count > 0xFFFF || count2 < 0 || count2 > 0xFFFF) {
        *(s32*)((u8*)reader + 0x1Cu) = -1;
        return;
    }

    output->nodes = (u32)func_0018D5C0(count * (s32)sizeof(Sly1TreeNode));
    if (count != 0 && output->nodes == 0) {
        *(s32*)((u8*)reader + 0x1Cu) = -1;
        return;
    }

    Sly1TreeNode* nodes = (Sly1TreeNode*)output->nodes;
    for (s32 i = 0; i < count; ++i) {
        u32 face = (u32)func_00137BE0(reader);
        u32 back = (u32)func_00137BE0(reader);
        u32 front = (u32)func_00137BE0(reader);

        if (face >= (u32)input->face_count ||
            (back != 0xFFFFu && back >= (u32)count) ||
            (front != 0xFFFFu && front >= (u32)count)) {
            *(s32*)((u8*)reader + 0x1Cu) = -1;
            return;
        }
        nodes[i].face = input->faces + face * 0x20u;
        nodes[i].back = back == 0xFFFFu ? 0 : output->nodes + back * 0x0Cu;
        nodes[i].front = front == 0xFFFFu ? 0 : output->nodes + front * 0x0Cu;
    }
}
