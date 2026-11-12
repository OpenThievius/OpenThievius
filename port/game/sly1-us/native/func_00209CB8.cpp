// SPDX-License-Identifier: AGPL-3.0-or-later
// Start the native memory-card query as a completed request. The paired
// SyncCardRequest backfill reports the IOP's no-card result through the
// game's existing error mapping, so no PS2 RPC client is needed here.
#include "sly1/host_abi.h"

extern "C" s32 func_00209CB8(s32, s32, s32* card_type,
                              s32* free_blocks, s32* card_result) {
    if (card_type != nullptr) *card_type = 0;
    if (free_blocks != nullptr) *free_blocks = 0;
    if (card_result != nullptr) *card_result = 0;
    return 0;
}
