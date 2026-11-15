// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only scalar equivalent of the matched plane-tree point walk.
#include "common.h"
#include "collider.h"
#include "placed_object.h"

struct NativeTreePlane {
    Vec4 normal;
    f32 offset;
};

s32 func_0013E658(Vec4* point, TreeNode03* node)
{
    while (node != 0) {
        const NativeTreePlane* plane = (const NativeTreePlane*)node->face;
        const f32 distance = point->x * plane->normal.x +
                             point->y * plane->normal.y +
                             point->z * plane->normal.z - plane->offset;

        if (distance < 1.0e-4f) {
            if (node->back == 0) return (s32)node;
            node = node->back;
        } else {
            node = node->front;
        }
    }
    return 0;
}
