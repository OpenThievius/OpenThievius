// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only synchronous CD service for the native host32 launcher.
#include "sly1/host_abi.h"

extern "C" int sly1_host_cd_read(u32 sector, u32 count, u32 destination);
extern "C" void sly1_native_trace_cd_read(u32 sector, u32 count,
                                            u32 mode, u32 result)
    __attribute__((weak));

static s32 g_last_cd_status = -1;

extern "C" s32 func_00203B18(u32 sector, u32 count, u32 destination,
                              const u8* options) {
    const u32 mode = options != nullptr
        ? static_cast<u32>(options[0]) | (static_cast<u32>(options[1]) << 8) |
          (static_cast<u32>(options[2]) << 16)
        : 0xFFFFFFFFu;
    if (options == nullptr || options[2] != 0) {
        g_last_cd_status = -1;
        if (sly1_native_trace_cd_read != nullptr)
            sly1_native_trace_cd_read(sector, count, mode, 0u);
        return 0;
    }
    const s32 result = sly1_host_cd_read(sector, count, destination);
    g_last_cd_status = result ? 0 : -1;
    if (sly1_native_trace_cd_read != nullptr)
        sly1_native_trace_cd_read(sector, count, mode, result ? 1u : 0u);
    return result;
}

extern "C" s32 func_00203D90(void) {
    return g_last_cd_status;
}
