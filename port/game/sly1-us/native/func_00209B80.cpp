// SPDX-License-Identifier: AGPL-3.0-or-later
// Complete the native card-info request with the PS2 memory-card library's
// no-card reply. Operation 1/result -1 maps to the retail no-card state; the
// normal game code then decides which prompt to show. This does not format or
// write a card.
#include "sly1/host_abi.h"

extern "C" s32 func_00209B80(s32, s32* operation, s32* result) {
    if (operation != nullptr) *operation = 1;
    if (result != nullptr) *result = -1;
    return 1;
}
