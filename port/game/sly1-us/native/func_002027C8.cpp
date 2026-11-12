// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native replacements for the EE DMA/VIF setup boundary. Channel
// records live in host memory and complete synchronously; no PS2 MMIO is used.
namespace {

struct NativeDmaChannel { unsigned int registers[0x84 / 4]; };
NativeDmaChannel g_channels[10]{};

} // namespace

extern "C" void openrac_native_renderer_submit_dma_chain(unsigned int source_address);
extern "C" void openrac_native_renderer_trace_dma_call(
    unsigned int channel_address, unsigned int source_address);
extern unsigned int D_00279340[10];
extern unsigned int D_00261F1C;
extern "C" __attribute__((weak)) void
openrac_native_renderer_trace_dma_call(unsigned int, unsigned int) {}
extern "C" __attribute__((weak)) void sly1_native_host32_trace_dma_call(
    unsigned int, unsigned int, unsigned int, unsigned int) {}
extern "C" __attribute__((weak)) void sly1_native_host32_trace_dma_channel(
    unsigned int, unsigned int, unsigned int) {}

extern "C" unsigned int openrac_native_dma_channel_address(unsigned int index) {
    if (index >= 10) return 0;
    return reinterpret_cast<unsigned int>(&g_channels[index]);
}

extern "C" unsigned int func_002027A0(unsigned int index) {
    const unsigned int address = openrac_native_dma_channel_address(index);
    sly1_native_host32_trace_dma_channel(
        index, address, reinterpret_cast<unsigned int>(__builtin_return_address(0)));
    return address;
}

extern "C" unsigned int decomp_func_002027A0(unsigned int index) {
    const unsigned int address = openrac_native_dma_channel_address(index);
    sly1_native_host32_trace_dma_channel(
        index, address, reinterpret_cast<unsigned int>(__builtin_return_address(0)));
    return address;
}

extern "C" unsigned int func_002027C8(unsigned int mode) {
    for (unsigned int i = 0; i < 10; ++i) {
        for (unsigned int j = 0; j < 0x84 / 4; ++j)
            g_channels[i].registers[j] = 0;
        D_00279340[i] = reinterpret_cast<unsigned int>(&g_channels[i]);
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
    openrac_native_renderer_trace_dma_call(
        channel_address, reinterpret_cast<unsigned int>(source));
    sly1_native_host32_trace_dma_call(
        channel_address, reinterpret_cast<unsigned int>(source),
        reinterpret_cast<unsigned int>(__builtin_return_address(0)), D_00261F1C);
    if (channel_address == 0) return;
    const unsigned int native_gif_channel =
        reinterpret_cast<unsigned int>(&g_channels[2]);
    if (channel_address == 0x1000A000u || channel_address == native_gif_channel) {
        // GIF channel 2 completes synchronously by handing its chain to the
        // native renderer. The direct host32 build stores a host channel
        // record in g_gifChannel instead of the retail MMIO address.
        openrac_native_renderer_submit_dma_chain(reinterpret_cast<unsigned int>(source));
        return;
    }
    for (unsigned int i = 0; i < 10; ++i) {
        if (channel_address == reinterpret_cast<unsigned int>(&g_channels[i]))
            return;
    }
    if (channel_address >= 0x10000000u && channel_address < 0x11000000u) {
        // Other EE DMA channels have no native consumer yet. Leave the
        // corresponding operation complete instead of dereferencing MMIO.
        return;
    }
    // Startup can reach this service before a game channel handle is ready.
    // Never treat an arbitrary small value as a native register pointer.
}

// The native DMA boundary completes a submitted packet before returning, so
// the retail CHCR busy waits have no device state to poll.
extern "C" void func_00202B68(void*) {}

extern "C" int func_00202C38(void*, int mode, int timeout) {
    (void)mode;
    (void)timeout;
    return 0;
}

// Port-only no-op for an unimplemented native VU1/GS boundary call.
extern "C" void func_002018E8() {
    // The EE VU1/GS startup sequence has no device operations in the native path.
}
