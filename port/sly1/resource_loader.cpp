// SPDX-License-Identifier: AGPL-3.0-or-later
#include "resource_loader.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace sly1 {
namespace {

constexpr std::uint32_t kSectorSize = 2048;
constexpr std::uint32_t kRawSectorSize = 2352;
constexpr std::uint32_t kCatalogAddress = 0x00247AF0;
constexpr std::size_t kCatalogEntrySize = 0x2C;
constexpr std::size_t kCatalogEntryCount = 46;

std::uint32_t le32(const std::uint8_t* p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}

struct Segment { std::uint32_t address, offset, size; };

class Disc {
public:
    explicit Disc(const std::filesystem::path& path) : file_(path, std::ios::binary) {
        if (!file_) throw std::runtime_error("cannot open disc image: " + path.string());
        file_.seekg(0, std::ios::end);
        const auto end = file_.tellg();
        if (end < 0) throw std::runtime_error("cannot determine disc image size");
        size_ = static_cast<std::uint64_t>(end);
        if (size_ % kSectorSize == 0 && has_pvd(0, 0, kSectorSize)) {
            stride_ = kSectorSize; payload_ = 0; base_ = 0;
            sectors_ = size_ / stride_;
            return;
        }
        for (const std::uint64_t base : {std::uint64_t(0), std::uint64_t(150) * kRawSectorSize}) {
            if (base > size_ || (size_ - base) % kRawSectorSize != 0) continue;
            std::array<std::uint8_t, 17> hdr{};
            file_.seekg(static_cast<std::streamoff>(base + 16ull * kRawSectorSize));
            file_.read(reinterpret_cast<char*>(hdr.data()), hdr.size());
            if (file_.gcount() != static_cast<std::streamsize>(hdr.size())) continue;
            if (hdr[0] != 0 || hdr[11] != 0 ||
                !std::all_of(hdr.begin() + 1, hdr.begin() + 11, [](auto b){ return b == 0xff; })) continue;
            const std::uint32_t payload = hdr[16] == 1 ? 16 : hdr[16] == 2 ? 24 : 0;
            if (payload && has_pvd(base, payload, kRawSectorSize)) {
                stride_ = kRawSectorSize; payload_ = payload; base_ = base;
                sectors_ = (size_ - base_) / stride_;
                return;
            }
        }
        throw std::runtime_error("no ISO 9660 primary volume descriptor at logical sector 16");
    }

    std::uint64_t sectors() const { return sectors_; }
    std::vector<std::uint8_t> read(std::uint32_t sector, std::uint32_t offset, std::uint32_t byte_count) {
        if (offset >= kSectorSize || std::uint64_t(sector) * kSectorSize + offset + byte_count > sectors_ * kSectorSize)
            throw std::runtime_error("catalog range extends beyond disc image");
        std::vector<std::uint8_t> out(byte_count);
        std::size_t done = 0;
        while (done < out.size()) {
            const auto logical = std::uint64_t(sector) * kSectorSize + offset + done;
            const auto lba = logical / kSectorSize;
            const auto within = logical % kSectorSize;
            const auto amount = std::min<std::size_t>(out.size() - done, kSectorSize - within);
            const auto physical = base_ + lba * stride_ + payload_ + within;
            file_.seekg(static_cast<std::streamoff>(physical));
            file_.read(reinterpret_cast<char*>(out.data() + done), static_cast<std::streamsize>(amount));
            if (file_.gcount() != static_cast<std::streamsize>(amount)) throw std::runtime_error("short read from disc image");
            done += amount;
        }
        return out;
    }
private:
    bool has_pvd(std::uint64_t base, std::uint32_t payload, std::uint32_t stride) {
        std::array<char, 7> signature{};
        file_.clear();
        file_.seekg(static_cast<std::streamoff>(base + 16ull * stride + payload));
        file_.read(signature.data(), signature.size());
        return file_.gcount() == static_cast<std::streamsize>(signature.size()) &&
               signature == std::array<char, 7>{1, 'C', 'D', '0', '0', '1', 1};
    }
    std::ifstream file_;
    std::uint64_t size_{}, sectors_{}, base_{};
    std::uint32_t stride_{}, payload_{};
};

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) throw std::runtime_error("cannot open ELF: " + path.string());
    const auto end = in.tellg();
    if (end < 0) throw std::runtime_error("cannot determine ELF size");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    in.seekg(0);
    in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (in.gcount() != static_cast<std::streamsize>(bytes.size())) throw std::runtime_error("short read from ELF");
    return bytes;
}

} // namespace

std::vector<CatalogResource> read_catalog(const std::filesystem::path& elf,
                                          const std::filesystem::path& disc_path) {
    const auto data = read_file(elf);
    if (data.size() < 52 || data[0] != 0x7f || data[1] != 'E' || data[2] != 'L' || data[3] != 'F' || data[4] != 1 || data[5] != 1)
        throw std::runtime_error("expected a 32-bit little-endian ELF executable");
    if ((std::uint16_t(data[0x12]) | (std::uint16_t(data[0x13]) << 8)) != 8)
        throw std::runtime_error("expected a MIPS executable ELF");
    const std::uint32_t phoff = le32(data.data() + 0x1c);
    const std::uint16_t phentsize = std::uint16_t(data[0x2a]) | (std::uint16_t(data[0x2b]) << 8);
    const std::uint16_t phnum = std::uint16_t(data[0x2c]) | (std::uint16_t(data[0x2d]) << 8);
    if (phentsize < 32 || std::uint64_t(phoff) + std::uint64_t(phentsize) * phnum > data.size())
        throw std::runtime_error("ELF program header table is out of bounds");
    std::vector<Segment> segments;
    for (std::uint16_t i = 0; i < phnum; ++i) {
        const auto* p = data.data() + phoff + std::size_t(i) * phentsize;
        if (le32(p) != 1) continue;
        const auto off = le32(p + 4), addr = le32(p + 8), size = le32(p + 16);
        if (std::uint64_t(off) + size > data.size()) throw std::runtime_error("ELF load segment is out of bounds");
        segments.push_back({addr, off, size});
    }
    auto file_offset = [&](std::uint32_t address, std::uint32_t size) -> std::size_t {
        for (const auto& s : segments)
            if (s.address <= address && std::uint64_t(address) + size <= std::uint64_t(s.address) + s.size)
                return std::size_t(s.offset) + address - s.address;
        throw std::runtime_error("ELF does not contain requested file-backed address");
    };
    const auto table = file_offset(kCatalogAddress, kCatalogEntrySize * kCatalogEntryCount);
    Disc disc(disc_path);
    std::vector<CatalogResource> result;
    result.reserve(kCatalogEntryCount);
    for (std::size_t i = 0; i < kCatalogEntryCount; ++i) {
        const auto* p = data.data() + table + i * kCatalogEntrySize;
        std::array<std::uint32_t, 11> w{};
        for (std::size_t j = 0; j < w.size(); ++j) w[j] = le32(p + j * 4);
        const auto sector = w[0] ^ w[5], bytes = w[1] ^ w[7], key = w[8];
        const auto label_off = file_offset(w[9], 1);
        const auto label_end = std::find(data.begin() + label_off,
            data.begin() + std::min(data.size(), label_off + 128), 0);
        if (label_end == data.begin() + std::min(data.size(), label_off + 128)) throw std::runtime_error("unterminated catalog label");
        const std::string label(data.begin() + label_off, label_end);
        if (bytes == 0 || bytes > 256u * 1024u * 1024u) throw std::runtime_error("invalid catalog resource byte count");
        const auto sectors = (std::uint64_t(bytes) + kSectorSize - 1) / kSectorSize;
        if (std::uint64_t(sector) + sectors > disc.sectors()) throw std::runtime_error("catalog resource extends beyond disc image");
        if (std::any_of(result.begin(), result.end(), [key](const auto& r){ return r.key == key; })) throw std::runtime_error("duplicate catalog resource key");
        result.push_back({key, label, sector, bytes, w[10]});
    }
    return result;
}

std::vector<std::uint8_t> load_resource(const std::filesystem::path& disc_path,
                                        const CatalogResource& resource,
                                        std::size_t max_decoded_bytes) {
    Disc disc(disc_path);
    auto compressed = disc.read(resource.sector, 0, resource.compressed_bytes);

    std::array<std::uint8_t, 0x2000> dictionary{};
    std::vector<std::uint8_t> output;
    output.reserve(std::min<std::size_t>(max_decoded_bytes, resource.compressed_bytes * 2ull));
    std::size_t input = 0, write = 0;
    while (input < compressed.size()) {
        const auto flags = compressed[input++];
        for (unsigned bit = 0; bit < 8 && input < compressed.size(); ++bit) {
            if (flags & (1u << bit)) {
                const auto value = compressed[input++];
                if (output.size() == max_decoded_bytes) throw std::runtime_error("decoded stream exceeds output limit");
                output.push_back(value); dictionary[write] = value; write = (write + 1) & 0x1fff;
            } else {
                if (input + 1 >= compressed.size()) throw std::runtime_error("truncated Sly LZ dictionary reference");
                const auto token = std::uint16_t(compressed[input]) | (std::uint16_t(compressed[input + 1]) << 8);
                input += 2;
                auto read = token & 0x1fff;
                const auto count = (token >> 13) + 3;
                if (count > max_decoded_bytes - output.size()) throw std::runtime_error("decoded stream exceeds output limit");
                for (unsigned n = 0; n < count; ++n) {
                    const auto value = dictionary[read]; read = (read + 1) & 0x1fff;
                    output.push_back(value); dictionary[write] = value; write = (write + 1) & 0x1fff;
                }
            }
        }
    }
    return output;
}

} // namespace sly1
