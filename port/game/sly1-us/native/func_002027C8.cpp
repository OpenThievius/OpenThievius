// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native replacements for the EE DMA/VIF setup boundary. Channel
// records live in host memory and complete synchronously; no PS2 MMIO is used.
struct NativeSprTransfer {
    unsigned int tags;
    int base;
    unsigned int addrs[8];
    int sizes[8];
    int count;
    int state;
};
extern "C" NativeSprTransfer D_002748F0[2];
extern "C" int D_00274990;
extern "C" void sly1_native_host32_trace_spr_copy(
    unsigned int direction, unsigned int chain, unsigned int tags,
    unsigned int transfer_index, unsigned int count, unsigned int source,
    unsigned int target, unsigned int size, unsigned int result);

namespace {

void copy_bytes(unsigned int destination, unsigned int source, unsigned int size) {
    volatile unsigned char* out = reinterpret_cast<volatile unsigned char*>(destination);
    const volatile unsigned char* in =
        reinterpret_cast<const volatile unsigned char*>(source);
    for (unsigned int i = 0; i < size; ++i) out[i] = in[i];
}

struct NativeDmaChannel { unsigned int registers[0x84 / 4]; };
NativeDmaChannel g_channels[10]{};

bool in_main_ram(unsigned int address, unsigned int size) {
    return address >= 0x00100000u && address <= 0x02000000u &&
           size <= 0x02000000u - address;
}

bool in_scratchpad(unsigned int address, unsigned int size) {
    return address >= 0x70000000u && address <= 0x70004000u &&
           size <= 0x70004000u - address;
}

bool copy_to_scratchpad(unsigned int chain_address) {
    const unsigned int physical_chain = chain_address & 0x0FFFFFFFu;
    for (unsigned int transfer_index = 0; transfer_index < 2u; ++transfer_index) {
        NativeSprTransfer& transfer = D_002748F0[transfer_index];
        if ((transfer.tags & 0x0FFFFFFFu) != physical_chain ||
            transfer.count <= 0 || transfer.count > 8)
            continue;

        unsigned int first_source = 0;
        unsigned int first_target = 0;
        unsigned int first_size = 0;
        for (int i = 0; i < transfer.count; ++i) {
            const unsigned long long tag =
                *reinterpret_cast<const volatile unsigned long long*>(
                    transfer.tags + static_cast<unsigned int>(i) * 16u);
            const unsigned int size = static_cast<unsigned int>(transfer.sizes[i]);
            const unsigned int source =
                static_cast<unsigned int>(tag >> 32) & 0x1FFFFFFFu;
            const unsigned int target = transfer.addrs[i];
            if (i == 0) {
                first_source = source;
                first_target = target;
                first_size = size;
            }
            if (size == 0 || !in_main_ram(source, size) ||
                !in_scratchpad(target, size)) {
                sly1_native_host32_trace_spr_copy(
                    9u, chain_address, transfer.tags, transfer_index,
                    static_cast<unsigned int>(transfer.count), first_source,
                    first_target, first_size, 2u);
                return false;
            }
            copy_bytes(target, source, size);
        }
        sly1_native_host32_trace_spr_copy(
            9u, chain_address, transfer.tags, transfer_index,
            static_cast<unsigned int>(transfer.count), first_source,
            first_target, first_size, 0u);
        return true;
    }
    sly1_native_host32_trace_spr_copy(
        9u, chain_address, 0u, 0xFFFFFFFFu, 0u, 0u, 0u, 0u, 1u);
    return false;
}

bool copy_from_scratchpad() {
    const int active = D_00274990;
    if (active < 0 || active > 1) return false;
    NativeSprTransfer& transfer = D_002748F0[active];
    if (transfer.count <= 0 || transfer.count > 8) return false;

    unsigned int first_source = 0;
    unsigned int first_target = 0;
    unsigned int first_size = 0;
    for (int i = 0; i < transfer.count; ++i) {
        const unsigned int source = transfer.addrs[i];
        if (source < 0x70000010u) continue;
        const unsigned int tag_address = source - 16u;
        if (!in_scratchpad(tag_address, 16u)) return false;
        const unsigned long long tag =
            *reinterpret_cast<const volatile unsigned long long*>(tag_address);
        const unsigned int id = (static_cast<unsigned int>(tag) >> 28) & 7u;
        if (id != 1u && id != 7u) continue;
        const unsigned int size =
            (static_cast<unsigned int>(tag) & 0xFFFFu) * 16u;
        const unsigned int target =
            static_cast<unsigned int>(tag >> 32) & 0x1FFFFFFFu;
        if (first_source == 0) {
            first_source = source;
            first_target = target;
            first_size = size;
        }
        if (size == 0 || !in_scratchpad(source, size) ||
            !in_main_ram(target, size)) {
            sly1_native_host32_trace_spr_copy(
                8u, 0u, transfer.tags, static_cast<unsigned int>(active),
                static_cast<unsigned int>(transfer.count), first_source,
                first_target, first_size, 2u);
            return false;
        }
        copy_bytes(target, source, size);
    }
    sly1_native_host32_trace_spr_copy(
        8u, 0u, transfer.tags, static_cast<unsigned int>(active),
        static_cast<unsigned int>(transfer.count), first_source,
        first_target, first_size, 0u);
    return true;
}

} // namespace

extern "C" void openrac_native_renderer_submit_dma_chain(unsigned int source_address);
extern "C" void openrac_native_renderer_trace_dma_call(
    unsigned int channel_address, unsigned int source_address);
extern "C" int openrac_guest_vif1_submit_chain(unsigned int source_address,
                                                 unsigned int channel_chcr);
extern unsigned int D_00279340[10];
extern unsigned int D_00261F1C;
extern "C" __attribute__((weak)) void
openrac_native_renderer_trace_dma_call(unsigned int, unsigned int) {}
extern "C" __attribute__((weak)) void sly1_native_host32_trace_dma_call(
    unsigned int, unsigned int, unsigned int, unsigned int) {}
extern "C" __attribute__((weak)) void sly1_native_host32_trace_spr_copy(
    unsigned int, unsigned int, unsigned int, unsigned int, unsigned int,
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
    const unsigned int native_vif1_channel =
        reinterpret_cast<unsigned int>(&g_channels[1]);
    if (channel_address == 0x10009000u || channel_address == native_vif1_channel) {
        // The direct host build stores channel registers in g_channels, while
        // the VIF runtime consumes the retail MMIO register block. Bridge the
        // submitted chain into that existing VIF1/VU1 path synchronously.
        const unsigned int address = reinterpret_cast<unsigned int>(source);
        const unsigned int chcr = (g_channels[1].registers[0] & 0x40u) | 0x105u;
        (void)openrac_guest_vif1_submit_chain(address, chcr);
        return;
    }
    const unsigned int native_to_spr_channel =
        reinterpret_cast<unsigned int>(&g_channels[9]);
    if (channel_address == native_to_spr_channel) {
        (void)copy_to_scratchpad(reinterpret_cast<unsigned int>(source));
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
extern "C" void func_00202B68(void* channel_address) {
    const unsigned int native_from_spr_channel =
        reinterpret_cast<unsigned int>(&g_channels[8]);
    if (reinterpret_cast<unsigned int>(channel_address) == native_from_spr_channel)
        (void)copy_from_scratchpad();
}

extern "C" int func_00202C38(void*, int mode, int timeout) {
    (void)mode;
    (void)timeout;
    return 0;
}

// Port-only no-op for an unimplemented native VU1/GS boundary call.
extern "C" void func_002018E8() {
    // The EE VU1/GS startup sequence has no device operations in the native path.
}
