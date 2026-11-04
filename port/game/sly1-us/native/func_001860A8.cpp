// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native implementation of Sly's draw-entry dispatch helper.
#include "sly1/host_abi.h"

static_assert(sizeof(u32) == 4);

void func_001278A8(void* object, s32 unused, void* input, void* output,
                   void* unused_a, s32 unused_b, s32 unused_c);
void* func_001BB540(void* object, s32 selector);

extern "C" void func_001860A8(void* unused_queue, s32 count, void** entries) {
    for (s32 draw = 0; draw < count; ++draw) {
        auto* entry = static_cast<unsigned char*>(entries[draw]);

        for (u32 part = 0; part < 2; ++part) {
            auto** object_slot = reinterpret_cast<void**>(entry + 4 + part * 0x40);
            void* object = *object_slot;
            if (object == nullptr) continue;

            auto* part_entry = entry + part * 0x40;
            func_001278A8(object, 0, part_entry + 0x10, part_entry + 0x30,
                          nullptr, 0, 0);

            auto** vtable = *reinterpret_cast<void***>(object);
            using DrawCallback = void (*)(void*, void*, u32);
            auto callback = reinterpret_cast<DrawCallback>(vtable[0xFC / sizeof(void*)]);
            if (callback != nullptr) callback(object, entry, part);
        }

        for (u32 part = 0; part < 2; ++part) {
            void* object = *reinterpret_cast<void**>(entry + 4 + part * 0x40);
            void* callback_object = object != nullptr
                                        ? func_001BB540(object, 0x104)
                                        : nullptr;
            if (callback_object == nullptr) continue;

            auto** vtable = *reinterpret_cast<void***>(callback_object);
            using DrawCallback = void (*)(void*, void*, u32);
            auto callback = reinterpret_cast<DrawCallback>(vtable[0x104 / sizeof(void*)]);
            if (callback != nullptr) callback(callback_object, entry, part);
        }

        auto* first = reinterpret_cast<f32*>(entry + 0x30);
        auto* second = reinterpret_cast<f32*>(entry + 0x70);
        auto* delta = reinterpret_cast<f32*>(entry + 0xB0);
        for (u32 lane = 0; lane < 4; ++lane) delta[lane] = first[lane] - second[lane];
    }
}
