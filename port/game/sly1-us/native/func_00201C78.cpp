// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only replacement for the EE GIF-DMA start register. Keep the matching
// console implementation in sly1-decomp; native builds submit its chain to
// OpenRAC's direct GS renderer instead of touching 0x1000A000.
using u32 = unsigned int;

extern "C" void openrac_native_renderer_submit_gif_stream(u32 source_address);

extern "C" int func_00201C78(void* source) {
    if (source == nullptr) return -1;
    // This boundary receives a raw GIF packet buffer. The retail helper reads
    // its GIF tag to compute QWC before starting GIF DMA; it is not a DMAC tag
    // chain, so send it through the renderer's raw GIF-stream path.
    openrac_native_renderer_submit_gif_stream(reinterpret_cast<u32>(source));
    return 0;
}
