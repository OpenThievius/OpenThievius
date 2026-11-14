/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Native replacement for the object-gatherer VU0 contact-velocity callback. */
#include "common.h"

typedef struct Sly1ObjectGathererResolveSelf {
    u8 pad[0x550];
    s32 state;
    u8 pad554[0x61C - 0x554];
    f32 speed;
    u8 pad620[0x630 - 0x620];
    f32 direction[4];
} Sly1ObjectGathererResolveSelf;

typedef struct Sly1ObjectGathererResolveContact {
    u8 pad[0x80];
    f32 normal[4];
    u8 pad90[0xB0 - 0x90];
    f32 velocity[4];
} Sly1ObjectGathererResolveContact;

void func_0019BB90(Sly1ObjectGathererResolveSelf* self,
                   Sly1ObjectGathererResolveContact* contact, s32 end)
{
    f32* velocity = (f32*)((u8*)contact + (end << 6) + 0x30);
    const f32 normal_speed = self->direction[0] * contact->normal[0] +
                             self->direction[1] * contact->normal[1] +
                             self->direction[2] * contact->normal[2];

    if (contact->normal[2] < 0.7f && self->speed != 0.0f) {
        velocity[0] += (self->direction[0] - contact->normal[0] * normal_speed) * self->speed;
        velocity[1] += (self->direction[1] - contact->normal[1] * normal_speed) * self->speed;
        velocity[2] += (self->direction[2] - contact->normal[2] * normal_speed) * self->speed;
    }
}
