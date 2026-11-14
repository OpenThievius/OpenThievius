// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only point query for the on/off object face tree.
#include "common.h"
#include "collider.h"
#include "placed_object.h"

struct NativeFaceTreePlane {
    f32 offset;
    Vec02 normal;
};

s32 func_001E4DC0(TreeNode03* node, Vec02* point)
{
    while (node != 0) {
        const NativeFaceTreePlane* plane = (const NativeFaceTreePlane*)node->face;
        const f32 distance = point->x * plane->normal.x +
                             point->y * plane->normal.y +
                             point->z * plane->normal.z - plane->offset;

        if (distance < 0.0f) {
            if (node->back == 0) return 1;
            node = node->back;
        } else {
            if (node->front == 0) return 0;
            node = node->front;
        }
    }
    return 0;
}
