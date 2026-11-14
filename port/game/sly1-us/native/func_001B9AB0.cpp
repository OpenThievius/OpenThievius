// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only vector accumulator used by collider force producers.
#include "common.h"
#include "collider.h"

void func_001B9AB0(Collider* self, Vec02* force)
{
    self->v170.x += force->x;
    self->v170.y += force->y;
    self->v170.z += force->z;
    self->v170.w += force->w;
}
