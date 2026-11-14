// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only CPU replacement for CollectLightsForSphere's VU0 microprograms.
// The matching source remains in sly1-decomp; host staging renames that body
// and routes this retail address here.
#include "cone_volume.h"
#include "world.h"

struct LightGroup {
    void** lights;
    s32 count;
    s32 flags;
    u16 kind;
    s16 firstSlot;
};

struct SortKeyedObject : PlacedObject {
    s32 kind; // 0x2D0
    s32 f2D4; // light mode at 0x2D4, returned by the collector
    u8 pad2D8[0x304 - 0x2D8];
    s32 sortKey0; // 0x304
    s32 sortKey1; // 0x308
};

struct LightObject : SortKeyedObject {
    u8 pad310[0x334 - 0x310];
    f32 rangeMax;
    u8 pad338[0x370 - 0x338];
    Vec02 planes[6];
};

extern s32 g_lightGroupCount;
extern LightGroup g_lightGroups[16];

namespace {

bool sphere_reaches_light(const LightObject* light, const Vec02* centre,
                          f32 radius, s32 kind)
{
    if (kind == 1) {
        // Mode 1 uploads -position and rangeMax as one quadword. Its VU0
        // test is a sphere against that point-light range.
        const f32 x = centre->x - light->v140.x;
        const f32 y = centre->y - light->v140.y;
        const f32 z = centre->z - light->v140.z;
        const f32 reach = radius + light->rangeMax;
        return x * x + y * y + z * z <= reach * reach;
    }

    if (kind == 2) {
        // Modes 2 and 3 upload their six side/near/far planes. The matching
        // PlaneSigns helper defines the convention: a sphere is inside a
        // plane when dot(normal, centre) + offset - radius is negative.
        for (s32 i = 0; i < 6; ++i) {
            const Vec02& plane = light->planes[i];
            const f32 signed_distance = plane.x * centre->x +
                                        plane.y * centre->y +
                                        plane.z * centre->z + plane.w;
            if (signed_distance - radius >= 0.0f)
                return false;
        }
        return true;
    }

    return false;
}

u32 light_mask(const LightGroup* group, const Vec02* centre, f32 radius)
{
    const s32 count = group->count < 16 ? group->count : 16;
    if (count <= 0)
        return 0;
    if (group->kind == 0)
        return count == 16 ? 0xFFFFu : ((1u << count) - 1u);

    u32 mask = 0;
    for (s32 i = 0; i < count; ++i) {
        const LightObject* light = (const LightObject*)group->lights[i];
        if (sphere_reaches_light(light, centre, radius, group->kind))
            mask |= 1u << (count - 1 - i);
    }
    return mask;
}

void append_reached_lights(const LightGroup* group, u32 mask, s32 maxLights,
                           s32* strongest, s32* flaggedCount, s32* count,
                           LightObject** out)
{
    if (group == 0)
        return;

    u32 bit = 1u << group->count;
    for (s32 i = 0; i < group->count && i < 16; ++i) {
        bit >>= 1;
        if ((mask & bit) != 0 && *count < maxLights) {
            LightObject* light = (LightObject*)group->lights[i];
            out[(*count)++] = light;
            if (*strongest < light->f2D4)
                *strongest = light->f2D4;
            *flaggedCount += group->flags & 1;
        }
    }
}

} // namespace

s32 func_00185150(World* world, const Vec02* centre, f32 radius,
                  u32 skipMask, s32 maxLights, s32* flagged, s32* outCount,
                  LightObject** out)
{
    s32 count = 0;
    s32 flaggedCount = 0;
    s32 strongest = world->f1D6C != 0.0f;
    const u32 keepOut = ~skipMask;
    const LightGroup* previous = 0;
    u32 previousMask = 0;

    // The original pipelines each group's VU0 test with the next group setup;
    // producing one CPU mask ahead preserves the same order and skip behavior.
    for (s32 i = 0; i <= g_lightGroupCount; ++i) {
        if (i == g_lightGroupCount) {
            append_reached_lights(previous, previousMask, maxLights,
                                  &strongest, &flaggedCount, &count, out);
            break;
        }

        const LightGroup* group = &g_lightGroups[i];
        if ((group->flags & keepOut) != 0)
            continue;

        append_reached_lights(previous, previousMask, maxLights,
                              &strongest, &flaggedCount, &count, out);
        previous = group;
        previousMask = light_mask(group, centre, radius);
    }

    *outCount = count;
    if (flagged != 0)
        *flagged = flaggedCount;
    return strongest;
}
