// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace sly1 {

struct CatalogResource {
    std::uint32_t key{};
    std::string label;
    std::uint32_t sector{};
    std::uint32_t compressed_bytes{};
    std::uint32_t flags{};
};

// Reads the NTSC-U retail catalog from a user-supplied ELF and returns its
// validated raw-disc resource records.
std::vector<CatalogResource> read_catalog(const std::filesystem::path& elf,
                                          const std::filesystem::path& disc);

// Reads and decodes one catalog resource using Sly's retail 8 KiB LZ stream.
std::vector<std::uint8_t> load_resource(const std::filesystem::path& disc,
                                        const CatalogResource& resource,
                                        std::size_t max_decoded_bytes = 256u * 1024u * 1024u);

} // namespace sly1
