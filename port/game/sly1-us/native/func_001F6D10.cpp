/* SPDX-License-Identifier: AGPL-3.0-or-later */
// Native boundary for EE syscall 0x7c (Deci2Call), used here only for kputs.
using s32 = int;

extern "C" s32 func_001F6D10(s32 request, void* arg) {
    // DECI2 kputs is diagnostic output. The native port has no DECI2 device;
    // report it as accepted so optional debug text does not terminate startup.
    (void)arg;
    return request == 0x10 ? 1 : 0;
}
