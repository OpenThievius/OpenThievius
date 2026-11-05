// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only DMA wait for Sly's second native host channel record.
extern unsigned int D_00261F18;
extern unsigned int func_002027A0(unsigned int index);
extern unsigned char D_00212110[];
extern void func_001F6C20(int channel);
extern void func_00202A80(unsigned int channel, void* source);
extern int func_00202C38(void* channel, int mode, int timeout);

extern "C" void func_00185AC8(void) {
    unsigned int channel = D_00261F18;
    if (channel < 0x08000000u || channel >= 0x09000000u) {
        channel = func_002027A0(1);
        D_00261F18 = channel;
    }
    if (channel == 0) return;

    *reinterpret_cast<unsigned int*>(channel) |= 0x40u;
    func_001F6C20(0);
    func_00202A80(channel, D_00212110);
    (void)func_00202C38(reinterpret_cast<void*>(channel), 0, 0);
    *reinterpret_cast<unsigned int*>(channel) &= ~0x40u;
}
