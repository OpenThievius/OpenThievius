// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only CPU backfill for the retail VU0 point-velocity operation. Keep it
// separate from the matching decomp; replace it if that source becomes hostable.
#include "collider.h"

namespace {

struct ColliderMotion {
    Vec4 linear;
    Vec4 angular;
};

} // namespace

void func_001BC7F8(Collider* body, Vec4* point, void* motion_ptr, Vec4* out) {
    const auto* motion = static_cast<const ColliderMotion*>(motion_ptr);
    const Vec4 arm = {
        point->x - body->v140.x,
        point->y - body->v140.y,
        point->z - body->v140.z,
        0.0f,
    };
    const Vec4& angular = motion->angular;
    *out = {
        motion->linear.x + angular.y * arm.z - angular.z * arm.y,
        motion->linear.y + angular.z * arm.x - angular.x * arm.z,
        motion->linear.z + angular.x * arm.y - angular.y * arm.x,
        motion->linear.w,
    };
}
