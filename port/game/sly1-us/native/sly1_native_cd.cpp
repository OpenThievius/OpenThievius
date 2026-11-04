// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only synchronous CD service for the native host32 launcher.
#include "sly1/host_abi.h"

extern "C" int sly1_host_cd_read(u32 sector, u32 count, u32 destination);
extern "C" int sly1_host_cd_read_error();

extern "C" s32 func_00203B18(u32 sector, u32 count, u32 destination,
                              const u8* options) {
    if (options == nullptr || options[2] != 0) return 0;
    return sly1_host_cd_read(sector, count, destination);
}

extern "C" s32 func_002032C8(s32 mode) {
    // Host reads complete before func_00203B18 returns, so no wait is pending.
    (void)mode;
    return 0;
}

extern "C" s32 func_00203D90() {
    return sly1_host_cd_read_error();
}
