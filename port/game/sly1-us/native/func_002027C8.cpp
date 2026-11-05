// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native replacements for the EE DMA/VIF setup boundary. Channel
// records live in host memory and complete synchronously; no PS2 MMIO is used.
namespace {

struct NativeDmaChannel { unsigned int registers[0x84 / 4]; };
NativeDmaChannel g_channels[10]{};

} // namespace

extern "C" unsigned int func_002027C8(unsigned int mode) {
    auto* channel_table = reinterpret_cast<unsigned int*>(0x00279340u);
    for (unsigned int i = 0; i < 10; ++i) {
        for (unsigned int j = 0; j < 0x84 / 4; ++j)
            g_channels[i].registers[j] = 0;
        channel_table[i] = reinterpret_cast<unsigned int>(&g_channels[i]);
    }
    (void)mode;
    return 0;
}

extern "C" void func_00201A18(int mode, int source, int mask, int enable) {
    // Native channel submissions complete synchronously and raise no EE IRQ.
    (void)mode;
    (void)source;
    (void)mask;
    (void)enable;
}

extern "C" void func_00202A80(unsigned int channel_address, void* source) {
    if (channel_address == 0) return;
    auto* registers = reinterpret_cast<unsigned int*>(channel_address);
    registers[0x30 / 4] = reinterpret_cast<unsigned int>(source);
    registers[0x20 / 4] = (registers[0x20 / 4] & ~0xDu) | 0x105u;
    registers[0x20 / 4] &= ~0x100u;
}

// Port-only no-op for an unimplemented native VU1/GS boundary call.
extern "C" void func_002018E8() {
    // The EE VU1/GS startup sequence has no device operations in the native path.
}
