/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only host translation of the retail three-axis endpoint sorter. */
#include "sly1/host_abi.h"

struct SortEntry_001389D0 {
    s32 endpoint;
    void* owner;
    f32* key;
};

struct World_001389D0 {
    u8 pad0[0x38];
    s32 body_count;
    u8 pad3C[0x1AD8u - 0x3Cu];
    SortEntry_001389D0* banks;
    u8 pad1ADC[0x1AECu - 0x1ADCu];
    s32 sorted;
};

void func_001385A0(void* world, void* a, void* b, s32 flags_a, s32 flags_b);
extern "C" void sly1_native_trace_sorter(u32 stage, u32 world, u32 axis, u32 value);

extern "C" void func_001389D0(World_001389D0* world)
    __asm__("_Z13func_001389D0P3C6C");

extern "C" void func_001389D0(World_001389D0* world) {
    if (world == nullptr || world->body_count <= 0 || world->sorted != 0 ||
        world->banks == nullptr) return;

    const s32 entry_count = world->body_count * 2;
    if (entry_count > 512) return;
    sly1_native_trace_sorter(0, (u32)world, 0, (u32)entry_count);

    for (s32 axis = 0; axis < 2; ++axis) {
        SortEntry_001389D0* entries = reinterpret_cast<SortEntry_001389D0*>(
            reinterpret_cast<u8*>(world->banks) + static_cast<u32>(axis) * 0x1800u);
        const s32 mode = axis == 0 ? 1 : 0;
        sly1_native_trace_sorter(1, (u32)world, (u32)axis, (u32)entry_count);
        u32 updates = 0;

        for (s32 i = 1; i < entry_count; ++i) {
            const SortEntry_001389D0 current = entries[i];
            if (current.key == nullptr) continue;
            const f32 current_key = *current.key;
            s32 insertion = i;

            while (insertion > 0) {
                SortEntry_001389D0& previous = entries[insertion - 1];
                if (previous.key == nullptr || *previous.key <= current_key) break;

                if (current.endpoint != previous.endpoint) {
                    ++updates;
                    if (updates == 1 || (updates & 0x3Fu) == 0)
                        sly1_native_trace_sorter(2, (u32)world, (u32)axis, updates);
                    func_001385A0(
                        world, static_cast<u8*>(current.owner) + 0x480u,
                        static_cast<u8*>(previous.owner) + 0x480u,
                        current.endpoint != 0 ? mode : 0,
                        current.endpoint == 0 ? mode : 0);
                    if (updates == 1 || (updates & 0x3Fu) == 0)
                        sly1_native_trace_sorter(3, (u32)world, (u32)axis, updates);
                }
                entries[insertion] = previous;
                --insertion;
            }
            entries[insertion] = current;
        }
        sly1_native_trace_sorter(4, (u32)world, (u32)axis, updates);
    }

    world->sorted = 1;
    sly1_native_trace_sorter(5, (u32)world, 0, (u32)entry_count);
}
