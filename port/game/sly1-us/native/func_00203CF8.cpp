// SPDX-License-Identifier: AGPL-3.0-or-later
// The native launcher opens and validates the host ISO before game startup.
// Report the ready status expected from the PS2 CDVD status command; this
// port-only service can be replaced by a fuller native CD/DVD backend later.
#include "sly1/host_abi.h"

extern "C" s32 func_00203CF8(void) {
    return 0x14;
}
