// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only implementation of the matched QueryValueTreeMask body. The current
// decomp source falls off the end of this non-void function; on host32 that
// reaches an adjacent cold block and traps. Keep this replacement isolated so
// matching C can replace it once its return behavior is recovered upstream.

#include "placed_object.h"
#include "value_list_object.h"
#include "vu0.h"

typedef u128 Bits128;

struct ValuePlaneNode {
    Vec02 normal;
    f32 dist;
    s32 front;
    s32 back;
};

s32 expand_mask_through_table(ValueListObject* self, s32 bits) {
    bits &= 0x7fffffff;
    if (bits == 0) return 0;
    s32 acc = -1;
    for (s32 i = 0; i < self->valueCount; ++i) {
        if ((bits >> i) & 1) acc &= self->values[i];
    }
    return acc | bits;
}

namespace {

s32 query_tree_mask(ValueListObject* self, s32 node, Bits128 point,
                    f32 radius, s32* out) {
    if (node < 0) {
        *out = expand_mask_through_table(self, node);
        return *out;
    }

    for (;;) {
        auto* plane = reinterpret_cast<ValuePlaneNode*>(
            static_cast<u32>(node));
        const f32 distance = Vu0::ToFloat(
            Vu0::Dot(point, Vu0::Q(plane->normal))) - plane->dist;
        const f32 near_distance = -radius;
        s32 next;

        if (distance < near_distance) {
            next = plane->front;
        } else if (radius < distance) {
            next = plane->back;
        } else {
            next = plane->back;
            if (static_cast<u32>(next) & 0x80000000u) {
                *out |= expand_mask_through_table(self, next);
            } else {
                query_tree_mask(self, next, point, radius, out);
            }
            next = plane->front;
        }

        if (static_cast<u32>(next) & 0x80000000u) {
            *out |= expand_mask_through_table(self, next);
            return *out;
        }
        node = next;
    }
}

} // namespace

s32 func_001EEA40(ValueListObject* self, s32 node, Bits128 point,
                  f32 radius, s32* out) {
    return query_tree_mask(self, node, point, radius, out);
}
