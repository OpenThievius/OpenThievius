/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only implementation of the reachable retail allocation wrapper.
 * The US routine's active path returns func_0018D758(a0) directly. */
#include "common.h"

extern void* func_0018D758(u32 size);

s32 func_001FD8A8(s32 size) {
    return (s32)(u32)func_0018D758((u32)size);
}
