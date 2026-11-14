// SPDX-License-Identifier: AGPL-3.0-or-later
// Native SDL input has no PS2 pad DMA slot. The matched retail implementation
// in sly1-decomp dereferences that hardware slot while sending rumble bytes;
// keep the matching body staged and report the same unavailable result it
// returns when the pad reply frame is not ready.
extern "C" int func_002049E8(int port, int slot, unsigned char* bytes) {
    (void)port;
    (void)slot;
    (void)bytes;
    return 0;
}
