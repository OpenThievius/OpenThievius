// SPDX-License-Identifier: AGPL-3.0-or-later
// Complete the native card-info request with the PS2 memory-card library's
// no-card reply by default. Operation 1/result -1 maps to the retail no-card
// state; the normal game code then decides which prompt to show.
//
// OPENRAC_NATIVE_MEMCARD emulates a formatted card instead: operation 1 /
// result 0 maps (via MapMemoryCardErrorCode(1, 0)) to success, which lets
// PollMemoryCard reach target state 3 instead of being stuck on the no-card
// prompt. This is a host service for the retail memory-card hardware the game
// requires; it writes no save data.
#include "sly1/host_abi.h"
extern "C" char* getenv(const char*);

extern "C" s32 func_00209B80(s32 no_wait, s32* operation, s32* result) {
    // Emulate a formatted card by default: the native port has no real memory
    // card, and the retail no-card prompt would otherwise block progression to
    // gameplay. OPENRAC_NO_MEMCARD restores the no-card behaviour.
    const bool emulate_card = getenv("OPENRAC_NO_MEMCARD") == nullptr;
    if (operation != nullptr) *operation = 1;
    if (result != nullptr) *result = emulate_card ? 0 : -1;
    (void)no_wait;
    return 1;
}
