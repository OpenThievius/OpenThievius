// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only backfill based on NTSC-U retail assembly at 0x00160208.
// Keep separate from sly1-decomp so matching C can replace this implementation.
namespace {
constexpr unsigned int kCatalogAddress = 0x00247AF0u;
constexpr unsigned int kCatalogEntrySize = 0x2Cu;
constexpr unsigned int kCatalogEntryCount = 46;
constexpr unsigned int kResourceKeyOffset = 0x20;
constexpr unsigned int kResourceFlagsOffset = 0x28;

unsigned int read_u32(unsigned int address) {
    return *reinterpret_cast<const volatile unsigned int*>(address);
}
} // namespace

extern "C" void func_00160208(int resource_class,
                              int* flag_2_count,
                              int* flag_4_count,
                              int* flag_8_count) {
    *flag_2_count = 0;
    *flag_4_count = 0;
    *flag_8_count = 0;

    for (unsigned int i = 0; i < kCatalogEntryCount; ++i) {
        const auto entry = kCatalogAddress + i * kCatalogEntrySize;
        if ((static_cast<int>(read_u32(entry + kResourceKeyOffset)) >> 8) != resource_class) {
            continue;
        }

        const auto flags = read_u32(entry + kResourceFlagsOffset);
        *flag_2_count += (flags & 0x2u) != 0;
        *flag_4_count += (flags & 0x4u) != 0;
        *flag_8_count += (flags & 0x8u) != 0;
    }
}
