// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only implementation of the still-unmatched script-event filter.
// Replace this when sly1-decomp provides hostable matching C.
#include "sly1/host_abi.h"
#include <stddef.h>

struct NativeWorld;

struct NativeQueuedCall {
    void (*fn)(u32, u32, u32);
    u32 owner;
    s32 key;
    u32 arguments;
    NativeQueuedCall* next;
};

static_assert(sizeof(NativeQueuedCall) == 0x14);
static_assert(offsetof(NativeQueuedCall, next) == 0x10);

struct NativeWorld {
    u8 pad[0x1ec8];
    NativeQueuedCall* first;
    NativeQueuedCall* last;
};

struct NativeOwner {
    u8 pad[0x14];
    NativeWorld* world;
};

struct NativePlacedOwner {
    u8 pad[0x29c];
    f32 eventTimer;
};

extern "C" void func_001834A0(void* self, s32 key, void* arguments);
extern "C" void func_001DBC18(void* world, void* record);

extern "C" s32 func_00183668(void* self, s32 key, s32, void* arguments) {
    if (key == 1) {
        auto* values = static_cast<void**>(arguments);
        if (values == nullptr || values[0] == nullptr) return 0;

        auto* owner = static_cast<NativePlacedOwner*>(self);
        const f32 remaining = owner->eventTimer - *static_cast<f32*>(values[0]);
        owner->eventTimer = remaining;
        return remaining > 0.0f;
    }

    s32 oppositeKey = -1;
    if (key == 4) oppositeKey = 5;
    else if (key == 5) oppositeKey = 4;
    else if (key == 0 || key == 29) return 0;

    auto* owner = static_cast<NativeOwner*>(self);
    auto* world = owner->world;
    NativeQueuedCall* previous = nullptr;
    for (NativeQueuedCall* call = world->first; call != nullptr; call = call->next) {
        if (call->fn == reinterpret_cast<void (*)(u32, u32, u32)>(func_001834A0) &&
            call->owner == static_cast<u32>(reinterpret_cast<unsigned long>(self))) {
            if (call->key == key) {
                if (key >= 8) return 1;
                if (key >= 6) {
                    if (arguments == nullptr || call->arguments == 0) return 0;
                    auto* incoming = static_cast<s32**>(arguments);
                    if (incoming[0] == nullptr) return 0;
                    auto* queuedValue = reinterpret_cast<const s32*>(call->arguments + 4);
                    return *queuedValue == *incoming[0];
                }
                return 1;
            }

            if (call->key == oppositeKey) {
                if (previous != nullptr) {
                    previous->next = call->next;
                    if (world->last == call) world->last = previous;
                } else if (call->next != nullptr) {
                    world->first = call->next;
                } else {
                    world->first = nullptr;
                    world->last = nullptr;
                }
                call->next = nullptr;
                func_001DBC18(world, call);
                return 1;
            }
        }
        previous = call;
    }
    return 0;
}
