/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Native EE GIF-channel DMA and the host-side GIF FIFO boundary. */
#include "openrac/native_gs.h"

#include "openrac/guest.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

#include "common/log.h"

namespace {
constexpr uint32_t kGifChcr = 0x1000A000u;
constexpr uint32_t kDmacCtrl = 0x1000E000u;
constexpr uint32_t kDmacStat = 0x1000E010u;
constexpr size_t kMaxTransferBytes = 64u * 1024u * 1024u;
constexpr size_t kMaxFifoBytes = 128u * 1024u * 1024u;
constexpr uint32_t kMaxTags = 1u << 16;

struct DmaAddress {
    uint32_t address;
    bool scratchpad;
};

uint32_t encode_dma_address(DmaAddress address) {
    return (address.address & 0x7FFFFFFFu) | (address.scratchpad ? 0x80000000u : 0u);
}

std::mutex g_fifo_mutex;
std::vector<uint8_t> g_gif_fifo;
size_t g_gif_fifo_head = 0;
std::atomic_flag g_headless_notice = ATOMIC_FLAG_INIT;
std::atomic_flag g_invalid_notice = ATOMIC_FLAG_INIT;
std::atomic<uint32_t> g_gif_transfer_count{0};

uint32_t load_mmio(uint32_t address) {
    uint32_t value = 0;
    std::memcpy(&value, openrac_guest_base + address, sizeof(value));
    return value;
}

void store_mmio(uint32_t address, uint32_t value) {
    std::memcpy(openrac_guest_base + address, &value, sizeof(value));
}

bool guest_span(DmaAddress address, size_t size, const uint8_t** source) {
    if (openrac_guest_base == nullptr) return false;
    if (address.scratchpad) {
        if (address.address > GUEST_SCRATCHPAD_SIZE ||
            size > GUEST_SCRATCHPAD_SIZE - address.address) return false;
        *source = openrac_guest_base + GUEST_SCRATCHPAD + address.address;
        return true;
    }
    const uint32_t normalized = openrac_guest_ram_address(address.address);
    if (normalized > openrac_guest_ram_size || size > openrac_guest_ram_size - normalized)
        return false;
    *source = openrac_guest_base + normalized;
    return true;
}

bool fifo_append(const uint8_t* source, size_t size) {
    std::lock_guard lock(g_fifo_mutex);
    const size_t pending = g_gif_fifo.size() - g_gif_fifo_head;
    if (size > kMaxFifoBytes - pending) return false;
    if (g_gif_fifo_head != 0 &&
        (g_gif_fifo_head >= 1024u * 1024u || g_gif_fifo.size() + size > kMaxFifoBytes)) {
        g_gif_fifo.erase(g_gif_fifo.begin(), g_gif_fifo.begin() +
                         static_cast<std::ptrdiff_t>(g_gif_fifo_head));
        g_gif_fifo_head = 0;
    }
    g_gif_fifo.insert(g_gif_fifo.end(), source, source + size);
    return true;
}

bool transfer_span(DmaAddress source, uint32_t qwc, size_t* total_bytes) {
    const size_t bytes = static_cast<size_t>(qwc) * 16u;
    if (bytes > kMaxTransferBytes - *total_bytes) return false;
    const uint8_t* data = nullptr;
    if (!guest_span(source, bytes, &data) || !fifo_append(data, bytes)) return false;
    *total_bytes += bytes;
    return true;
}

bool execute_gif_chain(uint32_t chcr, size_t* transferred_bytes) {
    if ((chcr & 1u) == 0 || ((chcr >> 2) & 3u) != 1u || (chcr & (1u << 6)) != 0)
        return false;

    uint32_t tag_address = load_mmio(kGifChcr + 0x30u);
    DmaAddress tadr{tag_address & 0x7FFFFFFFu, (tag_address >> 31) != 0};
    std::array<DmaAddress, 2> return_addresses{};
    size_t depth = 0;
    size_t total_bytes = 0;
    for (uint32_t index = 0; index < kMaxTags; ++index) {
        const uint8_t* tag_ptr = nullptr;
        if (!guest_span(tadr, 16, &tag_ptr)) return false;
        uint64_t low = 0;
        uint64_t high = 0;
        std::memcpy(&low, tag_ptr, sizeof(low));
        std::memcpy(&high, tag_ptr + 8, sizeof(high));
        const uint32_t qwc = static_cast<uint32_t>(low & 0xFFFFu);
        const uint32_t id = static_cast<uint32_t>((low >> 28) & 7u);
        const DmaAddress data_address{static_cast<uint32_t>(high & 0x7FFFFFFFu),
                                      (high >> 63) != 0};
        DmaAddress payload{tadr.address + 16u, tadr.scratchpad};
        DmaAddress next{tadr.address + 16u, tadr.scratchpad};
        bool end_after_payload = false;

        switch (id) {
        case 0: // REFE
            payload = data_address;
            end_after_payload = true;
            break;
        case 1: // CNT
            next.address += qwc * 16u;
            break;
        case 2: // NEXT
            next = data_address;
            break;
        case 3: // REF
        case 4: // REFS
            payload = data_address;
            break;
        case 5: // CALL
            if (depth >= return_addresses.size()) return false;
            return_addresses[depth++] = DmaAddress{tadr.address + 16u + qwc * 16u, tadr.scratchpad};
            next = data_address;
            break;
        case 6: // RET
            if (depth == 0) {
                end_after_payload = true;
            } else {
                next = return_addresses[--depth];
            }
            break;
        case 7: // END
            end_after_payload = true;
            break;
        }

        if (!transfer_span(payload, qwc, &total_bytes)) return false;
        store_mmio(kGifChcr + 0x10u,
                   encode_dma_address(DmaAddress{payload.address + qwc * 16u, payload.scratchpad}));
        store_mmio(kGifChcr + 0x20u, 0);
        if (end_after_payload) {
            store_mmio(kGifChcr + 0x30u, encode_dma_address(next));
            *transferred_bytes = total_bytes;
            return true;
        }
        store_mmio(kGifChcr + 0x30u, encode_dma_address(next));
        tadr = next;
    }
    return false;
}

bool execute_gif_dma(uint32_t chcr, size_t* transferred_bytes) {
    if ((load_mmio(kDmacCtrl) & 1u) == 0) return false;
    if (((chcr >> 2) & 3u) == 1u) return execute_gif_chain(chcr, transferred_bytes);
    if (((chcr >> 2) & 3u) != 0 || (chcr & 1u) == 0) return false;

    const uint32_t madr = load_mmio(kGifChcr + 0x10u);
    const uint32_t qwc = load_mmio(kGifChcr + 0x20u) & 0xFFFFu;
    size_t total = 0;
    const DmaAddress source{madr & 0x7FFFFFFFu, (madr >> 31) != 0};
    if (!transfer_span(source, qwc, &total))
        return false;
    store_mmio(kGifChcr + 0x10u, encode_dma_address(DmaAddress{source.address + qwc * 16u,
                                                               source.scratchpad}));
    store_mmio(kGifChcr + 0x20u, 0);
    *transferred_bytes = total;
    return true;
}

} // namespace

extern "C" int openrac_guest_ee_mmio_store32(uint32_t address, uint32_t value) {
    if (openrac_guest_base == nullptr || address < GUEST_EE_REGS ||
        address >= GUEST_EE_REGS + GUEST_EE_REGS_SIZE) return 0;

    if (address == kDmacStat) {
        const uint32_t previous = load_mmio(kDmacStat);
        const uint32_t status = (previous & 0xFFFFu) & ~(value & 0xFFFFu);
        const uint32_t masks = ((previous >> 16) ^ (value >> 16)) & 0x3FFu;
        store_mmio(kDmacStat, status | (masks << 16));
        return 1;
    }

    if (address != kGifChcr || (value & 0x100u) == 0) return 0;
    store_mmio(kGifChcr, value);
    size_t transferred = 0;
    if (!execute_gif_dma(value, &transferred)) {
        // A malformed/unavailable transfer raises the DMAC bus-error flag and
        // terminates the channel; it is never reported as rendered output.
        const uint32_t stat = load_mmio(kDmacStat);
        store_mmio(kDmacStat, (stat & ~0x100u) | (1u << 15));
        store_mmio(kGifChcr, value & ~0x100u);
        if (!g_invalid_notice.test_and_set(std::memory_order_relaxed)) {
            openrac::log::warn(
                "native GIF DMA channel 2 rejected transfer: CHCR={:#010x} D_CTRL={:#010x} "
                "TADR={:#010x} MADR={:#010x} QWC={:#010x}",
                value, load_mmio(kDmacCtrl), load_mmio(kGifChcr + 0x30u),
                load_mmio(kGifChcr + 0x10u), load_mmio(kGifChcr + 0x20u));
        }
        return 1;
    }

    const uint32_t stat = load_mmio(kDmacStat);
    store_mmio(kDmacStat, stat | (1u << 2));
    store_mmio(kGifChcr, value & ~0x100u);
    const uint32_t transfer = g_gif_transfer_count.fetch_add(1, std::memory_order_relaxed);
    if (transfer < 8) {
        openrac::log::info("native GIF DMA transfer={} bytes={} FIFO_pending={}", transfer + 1,
                           transferred, openrac_guest_gif_fifo_pending());
    }
    if (!g_headless_notice.test_and_set(std::memory_order_relaxed)) {
        openrac::log::warn("native GIF FIFO has no GS renderer; queued commands are not displayed");
    }
    return 1;
}

extern "C" size_t openrac_guest_gif_fifo_read(void* destination, size_t capacity) {
    if (destination == nullptr || capacity == 0) return 0;
    std::lock_guard lock(g_fifo_mutex);
    const size_t available = g_gif_fifo.size() - g_gif_fifo_head;
    const size_t amount = std::min(capacity, available);
    std::memcpy(destination, g_gif_fifo.data() + g_gif_fifo_head, amount);
    g_gif_fifo_head += amount;
    if (g_gif_fifo_head == g_gif_fifo.size()) {
        g_gif_fifo.clear();
        g_gif_fifo_head = 0;
    }
    return amount;
}

extern "C" size_t openrac_guest_gif_fifo_pending(void) {
    std::lock_guard lock(g_fifo_mutex);
    return g_gif_fifo.size() - g_gif_fifo_head;
}
