// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native replacement for Sly's DMA reset wrapper.
extern "C" void func_001F6C20(int unused) {
    // Native DMA work completes synchronously, so no host queue needs reset.
    (void)unused;
}
