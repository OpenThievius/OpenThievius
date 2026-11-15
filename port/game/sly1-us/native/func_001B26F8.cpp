// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only C++ backfill for the retail assembly sphere test. The matching
// VU0 assembly remains in sly1-decomp; replace this when a hostable C match is
// available. This file does not change decompilation coverage.
#include "camera.h"
#include "cursor_animator.h"
#include "globals.h"
#include "range_record.h"

extern "C" s32 func_001B26F8(RangeRecord* record, void* point, f32 radius)
{
    if (record->link == 0) {
        return 0;
    }

    const u32 flags = record->link->maskC;
    const u32 camera_mask = g_camera->areaMask;
    if ((flags & 0x10000000u) == 0 && (camera_mask & flags) != camera_mask) {
        return 0;
    }

    const Vec4* center = (const Vec4*)&record->v50;
    const Vec4* query = (const Vec4*)point;
    const f32 combined_radius = record->halfLength + radius;
    const f32 dx = center->x - query->x;
    const f32 dy = center->y - query->y;
    const f32 dz = center->z - query->z;
    return dx * dx + dy * dy + dz * dz <= combined_radius * combined_radius;
}
