// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only implementation of the still-assembly RefreshDiscStatus routine.
#include "sly1/host_abi.h"

extern "C" u32 func_00203CF8(void);
extern "C" volatile u32 D_002613F0;

extern "C" s32 func_00140968(void) {
    const u32 status = func_00203CF8();
    if (D_002613F0 != status) D_002613F0 = status;
    return status == 0x14u;
}
