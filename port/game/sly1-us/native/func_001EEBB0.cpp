/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only scalar translation of the retail value-tree point lookup. */
#include "sly1/host_abi.h"

struct Vec4_001EEBB0 { f32 v[4]; };

extern "C" int sly1_native_trace_value_tree_path_begin(
    u32 state, u32 root, f32 x, f32 y, f32 z, f32 w);
extern "C" void sly1_native_trace_value_tree_path_node(
    u32 depth, u32 node, f32 nx, f32 ny, f32 nz, f32 nw,
    f32 dot, f32 offset, u32 below, u32 above);
extern "C" void sly1_native_trace_value_tree_path_leaf(
    u32 below_branch, u32 raw_leaf, u32 result);

extern "C" void func_001EEBB0(void* state, const Vec4_001EEBB0* direction,
                                u32* result)
    __asm__("_Z13func_001EEBB0PvS_S_");

static u32 sly1_native_expand_value_leaf(const u8* state, u32 bits) {
    bits &= 0x7FFFFFFFu;
    if (bits == 0u) return 0u;
    const u32 count = *reinterpret_cast<const u32*>(state + 0x3Cu);
    const u32 values_address = *reinterpret_cast<const u32*>(state + 0x40u);
    if (count > 32u || values_address == 0u) return 0xFFFFFFFFu;
    const u32* values = reinterpret_cast<const u32*>(values_address);
    u32 expanded = 0xFFFFFFFFu;
    for (u32 i = 0; i < count; ++i) {
        if ((bits & (1u << i)) != 0u)
            expanded &= values[i];
    }
    return expanded | bits;
}

extern "C" void func_001EEBB0(void* state, const Vec4_001EEBB0* direction,
                                u32* result) {
    u32 current = state != nullptr
        ? *reinterpret_cast<const u32*>(static_cast<const u8*>(state) + 0x38u)
        : 0;
    const bool trace_this_call = state != nullptr && direction != nullptr &&
        sly1_native_trace_value_tree_path_begin(
            reinterpret_cast<u32>(state), current,
            direction->v[0], direction->v[1], direction->v[2], direction->v[3]) != 0;
    if (current == 0) {
        *result = 0xFFFFFFFFu;
        return;
    }
    if ((current & 0x80000000u) != 0) {
        *result = sly1_native_expand_value_leaf(
            static_cast<const u8*>(state), current);
        return;
    }

    const u8* state_bytes = static_cast<const u8*>(state);
    u32 node_limit = *reinterpret_cast<const u32*>(state_bytes + 0x34u);
    if (node_limit == 0u || node_limit > 4096u) node_limit = 4096u;
    for (u32 visited = 0; visited <= node_limit; ++visited) {
        const u8* record = reinterpret_cast<const u8*>(current);
        const f32 dot = direction->v[0] * *reinterpret_cast<const f32*>(record + 0x00u) +
                        direction->v[1] * *reinterpret_cast<const f32*>(record + 0x04u) +
                        direction->v[2] * *reinterpret_cast<const f32*>(record + 0x08u);
        const f32 threshold = *reinterpret_cast<const f32*>(record + 0x10u);
        const u32 below = *reinterpret_cast<const u32*>(record + 0x14u);
        const u32 above = *reinterpret_cast<const u32*>(record + 0x18u);
        if (trace_this_call)
            sly1_native_trace_value_tree_path_node(
                visited, current,
                *reinterpret_cast<const f32*>(record + 0x00u),
                *reinterpret_cast<const f32*>(record + 0x04u),
                *reinterpret_cast<const f32*>(record + 0x08u),
                *reinterpret_cast<const f32*>(record + 0x0Cu),
                dot, threshold, below, above);
        const bool take_below = dot < threshold;
        current = take_below ? below : above;
        if (current == 0 || (current & 0x80000000u) != 0) {
            *result = current == 0u
                ? 0xFFFFFFFFu
                : sly1_native_expand_value_leaf(state_bytes, current);
            if (trace_this_call)
                sly1_native_trace_value_tree_path_leaf(
                    take_below ? 1u : 0u, current, *result);
            return;
        }
    }

    /* Bound the traversal if malformed data supplies a cyclic tree. */
    *result = 0xFFFFFFFFu;
}
