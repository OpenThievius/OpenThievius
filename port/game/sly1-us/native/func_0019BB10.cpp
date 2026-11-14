/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Native replacement for the object-gatherer VU0 contact callback. */
#include "sly1/host_abi.h"

struct Sly1ObjectGathererContactSelf {
    u8 pad[0x550];
    s32 state;
};

struct Sly1ObjectGathererContact {
    u8 pad[0x80];
    f32 normal[4];
    u8 pad90[4];
    f32 response;
    f32 friction;
};

extern "C" void func_0019BB10(Sly1ObjectGathererContactSelf* self,
                                 Sly1ObjectGathererContact* contact)
    __asm__("_Z13func_0019BB10v");

extern "C" void func_0019BB10(Sly1ObjectGathererContactSelf* self,
                                 Sly1ObjectGathererContact* contact) {
    if (contact->normal[2] > 0.7f) {
        contact->response = self->state == 1 ? 3.0f : 5.0f;
        contact->friction = 0.1f;
    } else {
        contact->response = 0.0f;
        contact->friction = 0.0f;
    }
}
