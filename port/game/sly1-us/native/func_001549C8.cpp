// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only initializer for the retail component parameter block. The matched
// decompilation remains authoritative once this function is reconstructed.
#include "common.h"
#include <stddef.h>

extern "C" void* func_001F5C0C(void* destination, s32 value, s32 size);
extern "C" u8 D_00248D00[];
extern "C" u8 D_00248D20[];
extern "C" u8 D_00248D30[];

struct __attribute__((aligned(16))) SlyQuad {
    u32 word[4];
};

struct __attribute__((aligned(16))) ComponentArray {
    SlyQuad quad[9];
};

struct __attribute__((aligned(16))) ComponentLimits {
    f32 f00;
    f32 f04;
    f32 f08;
    u32 pad0c[3];
    f32 f18;
    u32 pad1c;
    f32 f20;
    u32 pad24[3];
    SlyQuad q30;
    u32 pad40[3];
    s32 i4c;
    f32 f50;
    f32 f54;
    u32 pad58[6];
    f32 f70;
    u32 pad74;
    f32 f78;
    u32 pad7c;
};

struct __attribute__((aligned(16))) ComponentTail {
    SlyQuad quad[13];
};

struct __attribute__((aligned(16))) ComponentParameters {
    s32 enabled;
    ComponentArray array;
    ComponentLimits limits;
    ComponentTail tail;
};

static_assert(sizeof(ComponentArray) == 0x90, "retail component array size");
static_assert(sizeof(ComponentLimits) == 0x80, "retail component limits size");
static_assert(sizeof(ComponentTail) == 0xd0, "retail component tail size");
static_assert(offsetof(ComponentParameters, array) == 0x10,
              "retail component array offset");
static_assert(offsetof(ComponentParameters, limits) == 0xa0,
              "retail component limits offset");
static_assert(offsetof(ComponentParameters, tail) == 0x120,
              "retail component tail offset");

static ComponentArray g_component_array_template;
static ComponentLimits g_component_limits_template;
static bool g_component_array_initialized;
static bool g_component_limits_initialized;

extern "C" void func_001549C8(void* memory) {
    auto* destination = static_cast<ComponentParameters*>(memory);

    if (!g_component_array_initialized) {
        func_001F5C0C(&g_component_array_template, 0, sizeof(g_component_array_template));
        g_component_array_template.quad[1] = *reinterpret_cast<SlyQuad*>(D_00248D30);
        g_component_array_template.quad[5] = *reinterpret_cast<SlyQuad*>(D_00248D00);
        g_component_array_template.quad[8] = *reinterpret_cast<SlyQuad*>(D_00248D30);
        g_component_array_initialized = true;
    }

    if (!g_component_limits_initialized) {
        func_001F5C0C(&g_component_limits_template, 0, sizeof(g_component_limits_template));
        g_component_limits_template.f00 = 750.0f;
        g_component_limits_template.f04 = 750.0f;
        g_component_limits_template.f08 = 1.0f;
        g_component_limits_template.f18 = -1500.0f;
        g_component_limits_template.f20 = -1.0f;
        g_component_limits_template.q30 = *reinterpret_cast<SlyQuad*>(D_00248D20);
        g_component_limits_template.i4c = 200;
        g_component_limits_template.f50 = 1.0f;
        g_component_limits_template.f54 = 1.0f;
        g_component_limits_template.f70 = 1.0f;
        g_component_limits_template.f78 = -1.0f;
        g_component_limits_initialized = true;
    }

    func_001F5C0C(destination, 0, 0x200);
    destination->enabled = 1;
    destination->array = g_component_array_template;
    destination->limits = g_component_limits_template;
}
