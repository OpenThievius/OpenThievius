/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only host translation of the retail two-axis endpoint sorter. */
#include "common.h"

struct SortEntry_001389D0 { s32 endpoint; void* owner; f32* key; };
void func_001385A0(void* world, void* a, void* b, s32 flags_a, s32 flags_b);

void func_001389D0(void* world) {
    u8* object = (u8*)world;
    s32 body_count, entry_count, axis;
    if (world == 0) return;
    body_count = *(s32*)(object + 0x38);
    if (body_count <= 0 || *(s32*)(object + 0x1AEC) != 0 ||
        *(void**)(object + 0x1AD8) == 0) return;
    entry_count = body_count * 2;
    if (entry_count > 512) return;

    for (axis = 0; axis < 2; ++axis) {
        struct SortEntry_001389D0* entries = (struct SortEntry_001389D0*)
            ((u8*)*(void**)(object + 0x1AD8) + axis * 0x1800);
        s32 mode = axis == 0 ? 1 : 0;
        s32 i;
        for (i = 1; i < entry_count; ++i) {
            struct SortEntry_001389D0 current = entries[i];
            s32 insertion = i;
            f32 current_key;
            if (current.key == 0) continue;
            current_key = *current.key;
            while (insertion > 0) {
                struct SortEntry_001389D0* previous = &entries[insertion - 1];
                if (previous->key == 0 || *previous->key <= current_key) break;
                if (current.endpoint != previous->endpoint)
                    func_001385A0(world, (u8*)current.owner + 0x480,
                                  (u8*)previous->owner + 0x480,
                                  current.endpoint != 0 ? mode : 0,
                                  current.endpoint == 0 ? mode : 0);
                entries[insertion] = *previous;
                --insertion;
            }
            entries[insertion] = current;
        }
    }
    *(s32*)(object + 0x1AEC) = 1;
}
