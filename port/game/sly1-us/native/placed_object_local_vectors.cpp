// SPDX-License-Identifier: AGPL-3.0-or-later
// Host backfills for two original VU0 local-vector store routines.

#include "placed_object.h"

namespace {

Vec02 rotate_local_vector(const PlacedObject* object, const Vec02* local) {
    const Mat02& matrix = object->rot;
    Vec02 world;
    world.x = matrix.r[0].x * local->x + matrix.r[1].x * local->y +
              matrix.r[2].x * local->z;
    world.y = matrix.r[0].y * local->x + matrix.r[1].y * local->y +
              matrix.r[2].y * local->z;
    world.z = matrix.r[0].z * local->x + matrix.r[1].z * local->y +
              matrix.r[2].z * local->z;
    world.w = local->w;
    return world;
}

} // namespace

void func_001290B8(PlacedObject* object, const Vec02* local) {
    object->v150 = rotate_local_vector(object, local);
}

void func_001290E0(PlacedObject* object, const Vec02* local) {
    object->v160 = rotate_local_vector(object, local);
}
