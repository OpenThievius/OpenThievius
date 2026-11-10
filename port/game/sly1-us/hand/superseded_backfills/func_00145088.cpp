// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only C backfill for pushing a controller onto the C6D active stack.
#include "sly1/host_abi.h"

struct T2173VTable {
    void (*slot0)(void*, s32);
    void (*slot1)(void*, s32);
    void (*slot2)(void*, s32);
};

struct T2173Entry {
    s32 key;
    void* controller;
    void* target;
    s32 argument;
};

extern "C" f32 D_0026185C;

extern "C" void func_00145088(void* self, s32 key, void* controller,
                                s32 target, s32 argument,
                                f32, f32, f32, f32, f32, f32) {
    u8* bytes = static_cast<u8*>(self);
    s32* count = reinterpret_cast<s32*>(bytes + 0x3D0);
    T2173Entry* entries = reinterpret_cast<T2173Entry*>(bytes + 0x3D4);
    s32 index = 0;

    // The retail stack is sorted in descending key order.
    while (index < *count && entries[index].key >= key) ++index;
    if (index < *count) {
        __builtin_memmove(&entries[index + 1], &entries[index],
                          static_cast<u32>(*count - index) * sizeof(T2173Entry));
    }

    T2173Entry* entry = &entries[index];
    entry->key = key;
    entry->controller = controller;
    entry->target = reinterpret_cast<void*>(target);
    entry->argument = argument;
    *count += 1;

    const T2173VTable* table = *reinterpret_cast<const T2173VTable* const*>(controller);
    if (table->slot2 != nullptr) table->slot2(controller, argument);
    if (index != 0) return;

    if (*count >= 2) {
        T2173Entry* displaced = &entries[1];
        const T2173VTable* displaced_table =
            *reinterpret_cast<const T2173VTable* const*>(displaced->controller);
        if (displaced_table->slot1 != nullptr)
            displaced_table->slot1(displaced->controller, displaced->argument);
    }

    *reinterpret_cast<f32*>(bytes + 0x364) = D_0026185C;
    if (table->slot0 != nullptr) table->slot0(controller, argument);
}
