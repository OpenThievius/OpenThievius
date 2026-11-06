/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only deferred request grouping, derived from retail C/assembly. */
#include "common.h"

typedef struct DeferredRequestNative {
    u32 callback;
    f32 sort_value;
    s32 group;
    u8 pad[0x74];
} __attribute__((aligned(16))) DeferredRequestNative;

typedef struct GroupMarkerNative {
    u32 callback;
    u8 pad[0x7C];
} __attribute__((aligned(16))) GroupMarkerNative;

typedef char check_DeferredRequestNative[
    sizeof(DeferredRequestNative) == 0x80 ? 1 : -1];
typedef char check_GroupMarkerNative[
    sizeof(GroupMarkerNative) == 0x80 ? 1 : -1];

static void sort_request_range(void** entries, s32 count) {
    for (s32 i = 1; i < count; ++i) {
        void* value = entries[i];
        const DeferredRequestNative* request =
            (const DeferredRequestNative*)value;
        s32 j = i;
        while (j > 0) {
            const DeferredRequestNative* previous =
                (const DeferredRequestNative*)entries[j - 1];
            if (previous->sort_value > request->sort_value ||
                (previous->sort_value == request->sort_value &&
                 (u32)entries[j - 1] < (u32)value))
                break;
            entries[j] = entries[j - 1];
            --j;
        }
        entries[j] = value;
    }
}

s32 func_0019D4B0(s32 count, DeferredRequestNative* requests, void** out) {
    s32 start[18] = {0};
    s32 pos[18] = {0};
    s32 end[18] = {0};
    GroupMarkerNative* markers = (GroupMarkerNative*)0x0026AC30u;
    if (count < 0 || count > 0x400) return 0;

    for (s32 i = 0; i < count; ++i) {
        const s32 group = requests[i].group;
        if (group < 0 || group >= 18) return 0;
        ++end[group];
    }

    if (end[0] > 0 && markers[0].callback != 0) {
        out[0] = &markers[0];
        pos[0] = 1;
        ++end[0];
    }
    start[0] = pos[0];
    for (s32 group = 1; group < 18; ++group) {
        pos[group] = end[group - 1];
        if (end[group] > 0 && markers[group].callback != 0) {
            out[pos[group]++] = &markers[group];
        }
        end[group] += pos[group];
        start[group] = pos[group];
    }

    for (s32 i = 0; i < count; ++i) {
        const s32 group = requests[i].group;
        out[start[group]++] = &requests[i];
    }

    const s32 sort_groups[] = {4, 8, 13, 1};
    for (u32 i = 0; i < sizeof(sort_groups) / sizeof(sort_groups[0]); ++i) {
        const s32 group = sort_groups[i];
        sort_request_range(&out[pos[group]], end[group] - pos[group]);
    }
    return end[17];
}

