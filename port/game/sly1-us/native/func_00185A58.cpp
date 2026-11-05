// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only GS/DMA channel wait for the native host's in-memory channel record.
// The matching retail body remains in staged decomp source.
extern unsigned int D_00261F14;
extern unsigned int func_002027A0(unsigned int index);
extern unsigned char D_00211E10[];
extern void func_001F6C20(int channel);
extern void func_00202A80(unsigned int channel, void* source);
extern int func_00202C38(void* channel, int mode, int timeout);

extern "C" void func_00185A58(void) {
    unsigned int channel = D_00261F14;
    if (channel < 0x08000000u || channel >= 0x09000000u) {
        channel = func_002027A0(0);
        D_00261F14 = channel;
    }
    if (channel == 0) return;

    *reinterpret_cast<unsigned int*>(channel) |= 0x40u;
    func_001F6C20(0);
    func_00202A80(channel, D_00211E10);
    (void)func_00202C38(reinterpret_cast<void*>(channel), 0, 0);
    *reinterpret_cast<unsigned int*>(channel) &= ~0x40u;
}
