// SPDX-License-Identifier: AGPL-3.0-or-later
// Complete the native memory-card query as a request. The paired
// SyncCardRequest backfill reports the IOP result through the game's existing
// error mapping, so no PS2 RPC client is needed here.
//
// By default the card is reported unformatted (card_result 0) alongside the
// no-card SyncCardRequest reply. With OPENRAC_NATIVE_MEMCARD the card is
// reported formatted (non-zero) so PollMemoryCard can reach target state 3.
#include "sly1/host_abi.h"
extern "C" char* getenv(const char*);

extern "C" s32 func_00209CB8(s32, s32, s32* card_type,
                              s32* free_blocks, s32* card_result) {
    const bool emulate_card = getenv("OPENRAC_NATIVE_MEMCARD") != nullptr;
    if (card_type != nullptr) *card_type = emulate_card ? 2 : 0;
    if (free_blocks != nullptr) *free_blocks = emulate_card ? 8192 : 0;
    if (card_result != nullptr) *card_result = emulate_card ? 1 : 0;
    return 0;
}
