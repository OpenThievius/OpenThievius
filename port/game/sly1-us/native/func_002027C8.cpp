// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native boundary for Sly's EE DMA/VIF initialization routines.
// Retail MMIO channels become ordinary host-side channel records here.
namespace {

struct NativeDmaChannel { unsigned int registers[0x84 / 4]; };
NativeDmaChannel g_channels[10]{};

} // namespace

extern "C" unsigned int func_002027C8(unsigned int mode) {
    auto* channel_table = reinterpret_cast<unsigned int*>(0x00279340u);
    for (unsigned int i = 0; i < 10; ++i) {
        for (unsigned int j = 0; j < 0x84 / 4; ++j) g_channels[i].registers[j] = 0;
        channel_table[i] = reinterpret_cast<unsigned int>(&g_channels[i]);
    }
    (void)mode;
    return 0;
}

extern "C" void func_002018E8() {
    // The EE VU1/GS startup sequence has no device operations in the native path.
}

extern "C" void func_00202A80(unsigned int channel_address, void* source) {
    if (channel_address == 0) return;
    auto* registers = reinterpret_cast<unsigned int*>(channel_address);
    registers[0x30 / 4] = reinterpret_cast<unsigned int>(source);
    registers[0x20 / 4] = (registers[0x20 / 4] & ~0xDu) | 0x105u;
    // Native DMA is synchronous; the renderer handoff will consume the chain
    // here. Clear the host channel's active flag before returning.
    registers[0x20 / 4] &= ~0x100u;
}
