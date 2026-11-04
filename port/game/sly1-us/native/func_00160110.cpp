// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only backfill based on NTSC-U retail assembly at 0x00160110.
// Keep separate from sly1-decomp so matching C can replace this implementation.
namespace {
constexpr unsigned int kCatalogAddress = 0x00247AF0u;
constexpr unsigned int kCatalogEntrySize = 0x2Cu;
constexpr unsigned int kCatalogEntryCount = 46;
constexpr unsigned int kResourceKeyOffset = 0x20;

unsigned int read_u32(unsigned int address) {
    return *reinterpret_cast<const volatile unsigned int*>(address);
}
} // namespace

extern "C" void* func_00160110(void* resource_key_value) {
    const auto resource_key = reinterpret_cast<unsigned int>(resource_key_value);
    for (unsigned int i = 0; i < kCatalogEntryCount; ++i) {
        const auto entry = kCatalogAddress + i * kCatalogEntrySize;
        if (read_u32(entry + kResourceKeyOffset) == resource_key) {
            return reinterpret_cast<void*>(entry);
        }
    }
    return nullptr;
}
