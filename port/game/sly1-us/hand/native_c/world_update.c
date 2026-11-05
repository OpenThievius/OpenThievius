/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only C6C frame update translated from the NTSC-U retail C path. */
#include "common.h"

extern u8 D_00261858[];
extern u8 D_00261990[];
extern u32 D_0027B314;

void func_001E9EE8(void* world, f32 step);
void func_001E9C78(void* world, f32 dt);
void func_001E9E30(void* world);
void func_00138C00(void* world, s32 mode);
void func_001BDD00(void* world, s32 kind, f32 dt);
s32 func_00129D70(void* actor);
void func_00129DF8(void* actor);
void func_001BB9D0(void* actor);
void func_001DBEC8(void* world, f32 dt);
void func_001DBE48(void* world);
void func_001B4878(f32 dt);
void func_001DDC40(void* world);
void func_0012E710(void* actor);
void func_00143E40(void* camera, s32 mode, f32 dt);
void func_001EA0A8(void* world, f32 step);
u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);
f32 sly1_native_call_f32(u32 address, u32 a0, u32 a1, u32 a2,
                         u32 a3, f32 f12, f32 f14);

struct NativeActorIterator {
    u32 link;
    s32 offset;
    u32 saved_context;
};

static void actor_iterator_begin(struct NativeActorIterator* iterator, u32 list) {
    iterator->link = list;
    iterator->offset = *(s32*)(list + 8);
    iterator->saved_context = D_0027B314;
    D_0027B314 = (u32)iterator;
}

static u32 actor_iterator_next(struct NativeActorIterator* iterator) {
    u32 actor = *(u32*)iterator->link;
    if (actor != 0) iterator->link = actor + (u32)iterator->offset;
    return actor;
}

static void actor_iterator_end(struct NativeActorIterator* iterator) {
    D_0027B314 = iterator->saved_context;
}

void func_001EA118(void* world) {
    u8* object = (u8*)world;
    f32 dt = *(f32*)(D_00261858 + 8);
    f32 step = *(f32*)(D_00261858 + 0x14);
    struct NativeActorIterator iterator;
    u32 actor;

    func_001E9EE8(world, step);
    if (dt != 0.0f) {
        func_001E9C78(world, dt);
        func_001E9E30(world);
        func_00138C00(world, 0);
        func_001BDD00(world, 1, dt);

        actor_iterator_begin(&iterator, (u32)(object + 0x6C));
        while ((actor = actor_iterator_next(&iterator)) != 0) {
            u32 vtable = *(u32*)actor;
            u32 update = vtable != 0 ? *(u32*)(vtable + 0x118) : 0;
            if (update != 0)
                (void)sly1_native_call_f32(update, actor, 0, 0, 0, dt, 0.0f);
        }
        actor_iterator_end(&iterator);

        func_00138C00(world, 1);
        func_001BDD00(world, 6, dt);
        actor_iterator_begin(&iterator, (u32)(object + 0x60));
        while ((actor = actor_iterator_next(&iterator)) != 0) {
            s32 tries = *(s32*)(actor + 0x228);
            if (tries < 2) {
                if (func_00129D70((void*)actor) != 0)
                    *(s32*)(actor + 0x228) = tries + 1;
                else
                    func_00129DF8((void*)actor);
            }
        }
        actor_iterator_end(&iterator);

        actor_iterator_begin(&iterator, (u32)(object + 0x6C));
        while ((actor = actor_iterator_next(&iterator)) != 0) {
            u32 vtable = *(u32*)actor;
            u32 post = vtable != 0 ? *(u32*)(vtable + 0x11C) : 0;
            if (post != 0) (void)sly1_native_call_regs4(post, actor, 0, 0, 0);
            if ((*(u32*)(actor + 0x53C) & 0x80000000u) != 0)
                func_001BB9D0((void*)actor);
        }
        actor_iterator_end(&iterator);

        func_001DBEC8(world, dt);
        func_001DBE48(world);
        func_001B4878(dt);
    }

    func_001DDC40(world);
    actor_iterator_begin(&iterator, (u32)(object + 0x1B7C));
    while ((actor = actor_iterator_next(&iterator)) != 0)
        func_0012E710((void*)actor);
    actor_iterator_end(&iterator);

    func_00143E40(D_00261990, 1, dt);
    func_001EA0A8(world, step);
}
