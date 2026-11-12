// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only replacement for the EE GIF-DMA start register. Keep the matching
// console implementation in sly1-decomp; native builds submit its chain to
// OpenRAC's direct GS renderer instead of touching 0x1000A000.
using u32 = unsigned int;

extern "C" {
extern unsigned char D_002624E0[0x10];
extern unsigned char D_002624F0[0x10];
extern unsigned char D_00262500[0xD0];
extern unsigned char D_002625D0[0x10];
extern unsigned char D_002625E0[0x10];
extern unsigned char D_002625F0[0xD0];
}

extern "C" void openrac_native_renderer_submit_gif_stream(u32 source_address);

extern "C" int func_00201C78(void* source) {
    if (source == nullptr) return -1;
    unsigned char packet[0x90];
    unsigned char* first = nullptr;
    unsigned char* second = nullptr;
    unsigned char* third = nullptr;
    const u32 address = reinterpret_cast<u32>(source);
    if (address == reinterpret_cast<u32>(D_002624E0)) {
        first = D_002624E0;
        second = D_002624F0;
        third = D_00262500;
    } else if (address == reinterpret_cast<u32>(D_002625D0)) {
        first = D_002625D0;
        second = D_002625E0;
        third = D_002625F0;
    }
    if (first != nullptr) {
        // Retail keeps this 0x90-byte GIF stream in three adjacent globals.
        // The native linker may place those aliases apart, so gather the
        // exact retail byte range before passing it to the renderer.
        for (u32 i = 0; i < 0x10; ++i) {
            packet[i] = first[i];
            packet[0x10 + i] = second[i];
        }
        for (u32 i = 0; i < 0x70; ++i) packet[0x20 + i] = third[i];
        source = packet;
    }
    // This boundary receives a raw GIF packet buffer. The retail helper reads
    // its GIF tag to compute QWC before starting GIF DMA; it is not a DMAC tag
    // chain, so send it through the renderer's raw GIF-stream path.
    openrac_native_renderer_submit_gif_stream(reinterpret_cast<u32>(source));
    return 0;
}
