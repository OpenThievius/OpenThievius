// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only synchronous CD service for the native host32 launcher.
#include "sly1/host_abi.h"

extern "C" int sly1_host_cd_read(u32 sector, u32 count, u32 destination);

static s32 g_last_cd_status = -1;

extern "C" s32 func_00203B18(u32 sector, u32 count, u32 destination,
                              const u8* options) {
    if (options == nullptr || options[2] != 0) {
        g_last_cd_status = -1;
        return 0;
    }
    const s32 result = sly1_host_cd_read(sector, count, destination);
    g_last_cd_status = result ? 0 : -1;
    return result;
}

extern "C" s32 func_00203D90(void) {
    return g_last_cd_status;
}
