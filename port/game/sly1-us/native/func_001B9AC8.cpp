// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only vector accumulator used by collider torque producers.
#include "common.h"
#include "collider.h"

void func_001B9AC8(Collider* self, Vec4* torque)
{
    self->v180.x += torque->x;
    self->v180.y += torque->y;
    self->v180.z += torque->z;
    self->v180.w += torque->w;
}
