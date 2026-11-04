// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native replacement for EE DMA/VIF interrupt configuration.
extern "C" void func_00201A18(int mode, int source, int mask, int enable) {
    // Sly's retail routine writes DMAC/VIF registers and changes EE interrupt
    // state. Native channels complete synchronously and use no EE interrupts.
    (void)mode;
    (void)source;
    (void)mask;
    (void)enable;
}
