// SPDX-License-Identifier: AGPL-3.0-or-later

#include "openrac/disc_image.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "openrac/guest.h"
#include "openrac/host_env.h"

namespace {

constexpr uint32_t kLogicalSectorSize = 2048;

class DiscImage {
    struct CachedResource {
        std::filesystem::path path;
        uint64_t byte_count = 0;
    };

public:
    bool open(const char* path, std::string* reason) {
        if (path == nullptr || *path == '\0') {
            *reason = "disc image path is empty";
            return false;
        }

        std::error_code error;
        const uintmax_t size = std::filesystem::file_size(path, error);
        if (error || size > static_cast<uintmax_t>(std::numeric_limits<int64_t>::max())) {
            *reason = "could not read disc image size";
            return false;
        }

        std::ifstream input(path, std::ios::binary);
        if (!input) {
            *reason = "could not open disc image";
            return false;
        }

        uint32_t stride = 0;
        uint32_t payload = 0;
        uint64_t base = 0;
        if (size % kLogicalSectorSize == 0 && has_primary_volume(input, 0, 0, kLogicalSectorSize)) {
            stride = kLogicalSectorSize;
        } else {
            const std::optional<uint32_t> first_track_payload = raw_primary_payload(input, 0);
            if (size % 2352 == 0 && first_track_payload) {
                stride = 2352;
                payload = *first_track_payload;
            } else {
                constexpr uint64_t kPregapBytes = uint64_t{150} * 2352;
                const std::optional<uint32_t> pregap_payload =
                    size > kPregapBytes && (size - kPregapBytes) % 2352 == 0
                        ? raw_primary_payload(input, kPregapBytes)
                        : std::nullopt;
                if (!pregap_payload) {
                    *reason = "disc image has no ISO 9660 primary volume descriptor at logical sector 16";
                    return false;
                }
                stride = 2352;
                payload = *pregap_payload;
                base = kPregapBytes;
            }
        }

        input_ = std::move(input);
        path_ = path;
        base_ = base;
        stride_ = stride;
        payload_ = payload;
        if ((size - base) / stride > std::numeric_limits<uint32_t>::max()) {
            *reason = "disc image has more logical sectors than the reader supports";
            input_.close();
            return false;
        }
        sectors_ = static_cast<uint32_t>((size - base) / stride);
        load_catalog_cache();
        return true;
    }

    bool find_file(const char* path, uint32_t* first_sector, uint32_t* byte_size) {
        if (!input_.is_open() || path == nullptr || first_sector == nullptr || byte_size == nullptr)
            return false;
        std::string_view remaining(path);
        if (remaining.size() >= 7 && equal_ascii(remaining.substr(0, 7), "cdrom0:"))
            remaining.remove_prefix(7);
        while (!remaining.empty() && (remaining.front() == '/' || remaining.front() == '\\'))
            remaining.remove_prefix(1);
        if (remaining.empty()) return false;

        std::array<uint8_t, kLogicalSectorSize> pvd{};
        if (!read(16, 1, pvd.data())) return false;
        if (pvd[0] != 1 || pvd[1] != 'C' || pvd[2] != 'D' || pvd[3] != '0' ||
            pvd[4] != '0' || pvd[5] != '1' || pvd[6] != 1) return false;
        const uint8_t* root = pvd.data() + 156;
        if (root[0] < 34 || root[32] != 1 || root[33] != 0) return false;
        uint32_t directory_sector = read_le32(root + 2) + root[1];
        uint32_t directory_size = read_le32(root + 10);

        while (!remaining.empty()) {
            const size_t separator = remaining.find_first_of("/\\");
            const std::string_view component = remaining.substr(0, separator);
            if (component.empty() || component == "." || component == "..") return false;
            if (separator == std::string_view::npos) remaining = {};
            else {
                remaining.remove_prefix(separator + 1);
                while (!remaining.empty() && (remaining.front() == '/' || remaining.front() == '\\'))
                    remaining.remove_prefix(1);
            }

            uint32_t extent = 0;
            uint32_t size = 0;
            uint8_t flags = 0;
            if (!find_record(directory_sector, directory_size, component, &extent, &size, &flags))
                return false;
            if (remaining.empty()) {
                if ((flags & 0x02u) != 0) return false;
                *first_sector = extent;
                *byte_size = size;
                return true;
            }
            if ((flags & 0x02u) == 0) return false;
            directory_sector = extent;
            directory_size = size;
        }
        return false;
    }

    bool read(uint32_t first, uint32_t count, uint8_t* output) {
        if (count == 0) return input_.is_open() && first <= sectors_;
        if (!input_.is_open() || first > sectors_ || count > sectors_ - first || output == nullptr) return false;
        std::lock_guard guard(mutex_);
        const uint64_t byte_count = uint64_t{count} * kLogicalSectorSize;
        if (byte_count > static_cast<uint64_t>(std::numeric_limits<std::streamsize>::max())) return false;
        const auto cached = catalog_sectors_.find(first);
        if (cached != catalog_sectors_.end() && cached->second.byte_count == byte_count) {
            std::ifstream resource(cached->second.path, std::ios::binary);
            if (resource) {
                resource.read(reinterpret_cast<char*>(output), static_cast<std::streamsize>(byte_count));
                if (resource.gcount() == static_cast<std::streamsize>(byte_count)) {
                    if (openrac_env_string("OPENRAC_TRACE_CD") != nullptr)
                        std::fprintf(stderr, "[info] native Sly cached resource read sector=%u count=%u file=%s\n",
                                     first, count, cached->second.path.c_str());
                    return true;
                }
            }
        }
        const uint64_t offset = base_ + uint64_t{first} * stride_ + payload_;
        if (stride_ == kLogicalSectorSize) {
            input_.clear();
            input_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
            input_.read(reinterpret_cast<char*>(output), static_cast<std::streamsize>(byte_count));
            return input_.good() || input_.gcount() == static_cast<std::streamsize>(byte_count);
        }

        for (uint32_t i = 0; i < count; ++i) {
            input_.clear();
            const uint64_t sector_offset = base_ + uint64_t{first + i} * stride_ + payload_;
            input_.seekg(static_cast<std::streamoff>(sector_offset), std::ios::beg);
            input_.read(reinterpret_cast<char*>(output + uint64_t{i} * kLogicalSectorSize),
                        kLogicalSectorSize);
            if (input_.gcount() != kLogicalSectorSize) return false;
        }
        return true;
    }

    uint32_t sectors() const { return sectors_; }

private:
    void load_catalog_cache() {
        const char* configured = std::getenv("OPENRAC_SLY1_CATALOG_DIR");
        std::filesystem::path directory;
        if (configured != nullptr && *configured != '\0') {
            directory = configured;
        } else {
            directory = std::filesystem::current_path() / "build/port/extracted/catalog";
        }
        std::ifstream manifest(directory / "catalog.tsv");
        if (!manifest) return;
        std::string line;
        if (!std::getline(manifest, line)) return;
        while (std::getline(manifest, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::array<std::string, 8> columns{};
            size_t start = 0;
            bool valid = true;
            for (size_t i = 0; i < columns.size(); ++i) {
                const size_t end = line.find('\t', start);
                if (i + 1 < columns.size() && end == std::string::npos) { valid = false; break; }
                columns[i] = line.substr(start, end == std::string::npos ? end : end - start);
                start = end == std::string::npos ? line.size() : end + 1;
            }
            if (!valid) continue;
            try {
                const uint32_t sector = static_cast<uint32_t>(std::stoul(columns[2]));
                const uint32_t sector_count = static_cast<uint32_t>(std::stoul(columns[4]));
                const uint64_t expected_size = uint64_t{sector_count} * kLogicalSectorSize;
                if (sector_count == 0 || expected_size > std::numeric_limits<size_t>::max()) continue;
                const std::filesystem::path filename(columns[7]);
                if (filename.has_parent_path() || filename.filename() != filename) continue;
                const std::filesystem::path resource_path = directory / filename;
                std::error_code error;
                if (std::filesystem::file_size(resource_path, error) == expected_size && !error)
                    catalog_sectors_.emplace(sector, CachedResource{resource_path, expected_size});
            } catch (...) {
                continue;
            }
        }
        if (openrac_env_string("OPENRAC_TRACE_CD") != nullptr)
            std::fprintf(stderr, "[info] loaded %zu extracted Sly resource ranges from %s\n",
                         catalog_sectors_.size(), directory.c_str());
    }

    static uint32_t read_le32(const uint8_t* bytes) {
        return uint32_t{bytes[0]} | (uint32_t{bytes[1]} << 8) |
               (uint32_t{bytes[2]} << 16) | (uint32_t{bytes[3]} << 24);
    }

    static std::optional<uint32_t> raw_primary_payload(std::ifstream& input, uint64_t base) {
        std::array<uint8_t, 24> raw{};
        const uint64_t offset = base + uint64_t{16} * 2352;
        input.clear();
        input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        input.read(reinterpret_cast<char*>(raw.data()), raw.size());
        if (input.gcount() != static_cast<std::streamsize>(raw.size()) || raw[0] != 0 || raw[11] != 0)
            return std::nullopt;
        for (size_t i = 1; i < 11; ++i) if (raw[i] != 0xFF) return std::nullopt;
        const uint32_t payload = raw[15] == 1 ? 16 : raw[15] == 2 ? 24 : 0;
        return payload != 0 && has_primary_volume(input, base, payload, 2352)
                   ? std::optional<uint32_t>(payload)
                   : std::nullopt;
    }

    static bool equal_ascii(std::string_view left, std::string_view right) {
        if (left.size() != right.size()) return false;
        for (size_t i = 0; i < left.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(left[i])) !=
                std::tolower(static_cast<unsigned char>(right[i]))) return false;
        }
        return true;
    }

    static std::string_view versionless(std::string_view name) {
        const size_t version = name.rfind(';');
        if (version != std::string_view::npos) name = name.substr(0, version);
        return name;
    }

    bool find_record(uint32_t directory_sector, uint32_t directory_size,
                     std::string_view wanted, uint32_t* extent, uint32_t* size, uint8_t* flags) {
        std::array<uint8_t, kLogicalSectorSize> sector{};
        uint32_t position = 0;
        while (position < directory_size) {
            const uint32_t sector_index = position / kLogicalSectorSize;
            const uint32_t in_sector = position % kLogicalSectorSize;
            if (!read(directory_sector + sector_index, 1, sector.data())) return false;
            if (sector[in_sector] == 0) {
                position = (sector_index + 1) * kLogicalSectorSize;
                continue;
            }
            const uint8_t record_size = sector[in_sector];
            if (record_size < 34 || record_size > kLogicalSectorSize - in_sector ||
                record_size > directory_size - position) return false;
            const uint8_t* record = sector.data() + in_sector;
            const uint8_t name_size = record[32];
            if (33u + name_size > record_size) return false;
            const std::string_view name(reinterpret_cast<const char*>(record + 33), name_size);
            if (!(name_size == 1 && (name[0] == '\0' || name[0] == '\1')) &&
                equal_ascii(versionless(name), versionless(wanted))) {
                *extent = read_le32(record + 2) + record[1];
                *size = read_le32(record + 10);
                *flags = record[25];
                return true;
            }
            position += record_size;
        }
        return false;
    }

    static bool has_primary_volume(std::ifstream& input, uint64_t base, uint32_t payload,
                                   uint32_t stride) {
        std::array<uint8_t, 7> descriptor{};
        const uint64_t offset = base + uint64_t{16} * stride + payload;
        input.clear();
        input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        input.read(reinterpret_cast<char*>(descriptor.data()), descriptor.size());
        return input.gcount() == static_cast<std::streamsize>(descriptor.size()) &&
               descriptor[0] == 1 && descriptor[1] == 'C' && descriptor[2] == 'D' &&
               descriptor[3] == '0' && descriptor[4] == '0' && descriptor[5] == '1' &&
               descriptor[6] == 1;
    }

    std::ifstream input_;
    std::string path_;
    uint64_t base_ = 0;
    uint32_t stride_ = 0;
    uint32_t payload_ = 0;
    uint32_t sectors_ = 0;
    std::mutex mutex_;
    std::unordered_map<uint32_t, CachedResource> catalog_sectors_;
};

DiscImage g_disc;
std::atomic<bool> g_file_request_ready{false};

}  // namespace

extern "C" int openrac_guest_disc_open(const char* path) {
    std::string reason;
    if (g_disc.open(path, &reason)) return 1;
    return 0;
}

extern "C" int openrac_guest_disc_read(uint32_t first_sector, uint32_t count,
                                        uint32_t guest_address) {
    const uint32_t address = openrac_guest_ram_address(guest_address);
    if (address > openrac_guest_ram_size ||
        uint64_t{count} * kLogicalSectorSize > openrac_guest_ram_size - address) {
        openrac_guest_missing("disc read destination is outside guest RAM");
        return 0;
    }

    std::vector<uint8_t> bytes(uint64_t{count} * kLogicalSectorSize);
    if (!g_disc.read(first_sector, count, bytes.data())) {
        openrac_guest_missing("disc read is outside the image or failed");
        return 0;
    }
    std::copy(bytes.begin(), bytes.end(), openrac_guest_base + address);
    return 1;
}

extern "C" uint32_t openrac_guest_disc_sector_count(void) {
    return g_disc.sectors();
}

extern "C" int openrac_lib_sly1_iop_boot(void) {
    if (g_disc.sectors() == 0) return 0;
    // The native services replace IOP boot and its SIFCMD startup handshake.
    // Publish BOOTEND and CMDINIT, and a nonzero opaque IOP receive address.
    openrac_guest_sif_set_register(4, 0x00060000u);
    openrac_guest_sif_set_register(2u, 1u);
    constexpr uint32_t kCdState = 0x002613F0u;
    const uint32_t address = openrac_guest_ram_address(kCdState);
    if (openrac_guest_base == nullptr || address > openrac_guest_ram_size ||
        openrac_guest_ram_size - address < sizeof(uint32_t)) return 0;
    const uint32_t ready = 0x14u;
    std::memcpy(openrac_guest_base + address, &ready, sizeof(ready));
    g_file_request_ready.store(false, std::memory_order_relaxed);
    return 1;
}

extern "C" int openrac_lib_sly1_iop_initialize(int mode) {
    (void)mode;
    // The native port replaces the IOP file service; there is no IOP to boot.
    return g_disc.sectors() != 0 ? 1 : 0;
}

extern "C" int openrac_lib_sly1_iop_services_start(int mode) {
    (void)mode;
    // The guest implementation waits for IOP-side SIF RPC bind replies. Native
    // file and DMA services replace those endpoints, so mark this service group
    // initialized without starting a guest RPC poller.
    constexpr uint32_t kIopServicesReady = 0x0027AE80u;
    const uint32_t address = openrac_guest_ram_address(kIopServicesReady);
    if (openrac_guest_base == nullptr || address > openrac_guest_ram_size ||
        openrac_guest_ram_size - address < sizeof(uint32_t)) {
        openrac_guest_missing("native IOP service state is outside guest RAM");
        return 0;
    }
    const uint32_t ready = 1;
    std::memcpy(openrac_guest_base + address, &ready, sizeof(ready));
    return 1;
}

extern "C" int openrac_lib_sly1_cd_status(void) {
    // Sly's func_00140968 accepts status 0x14 as the ready state.
    return g_disc.sectors() != 0 ? 0x14 : 0;
}

extern "C" int openrac_lib_sly1_cd_disk_ready(int mode) {
    (void)mode;
    // The startup caller waits for 2; 6 is the retail poll's unavailable result.
    return g_disc.sectors() != 0 ? 2 : 6;
}

namespace {
std::atomic<int> g_cd_read_error{0};
}

extern "C" int openrac_lib_sly1_cd_read_sectors(uint32_t sector, uint32_t count,
                                                uint32_t destination, uint32_t mode) {
    const uint32_t mode_address = openrac_guest_ram_address(mode);
    if (mode_address > openrac_guest_ram_size || openrac_guest_ram_size - mode_address < 3 ||
        openrac_guest_base[mode_address + 2] != 0) {
        g_cd_read_error.store(-1, std::memory_order_relaxed);
        openrac_guest_missing("native disc read mode is unsupported");
        return 0;
    }
    const int success = openrac_guest_disc_read(sector, count, destination);
    g_cd_read_error.store(success ? 0 : -1, std::memory_order_relaxed);
    if (openrac_env_string("OPENRAC_TRACE_CD") != nullptr) {
        const uint32_t thread_id = openrac_guest_current_thread_id();
        const gaddr caller = openrac_guest_last_called_address_for_thread(thread_id);
        std::fprintf(stderr,
                     "[info] native disc read thread=%u caller=%#010x sector=%u count=%u "
                     "destination=%#010x result=%d\n",
                     thread_id, caller, sector, count, destination, success);
    }
    return success;
}

extern "C" int openrac_lib_sly1_cd_read_wait(int mode) {
    (void)mode;
    return 0;
}

extern "C" int openrac_lib_sly1_cd_read_error(void) {
    return g_cd_read_error.load(std::memory_order_relaxed);
}

extern "C" int openrac_lib_sly1_cd_file_open(uint32_t guest_path) {
    const uint32_t address = openrac_guest_ram_address(guest_path);
    if (openrac_guest_base == nullptr || address >= openrac_guest_ram_size) {
        g_file_request_ready.store(false, std::memory_order_relaxed);
        return 0;
    }
    const char* start = reinterpret_cast<const char*>(openrac_guest_base + address);
    const size_t available = openrac_guest_ram_size - address;
    const auto* end = static_cast<const char*>(std::memchr(start, '\0', available));
    if (end == nullptr) {
        g_file_request_ready.store(false, std::memory_order_relaxed);
        return 0;
    }
    const std::string path(start, static_cast<size_t>(end - start));
    uint32_t first_sector = 0;
    uint32_t byte_size = 0;
    const bool found = g_disc.find_file(path.c_str(), &first_sector, &byte_size);
    g_file_request_ready.store(found, std::memory_order_relaxed);
    return found ? 1 : 0;
}

extern "C" int openrac_lib_sly1_cd_file_ready(void) {
    return g_file_request_ready.load(std::memory_order_relaxed) ? 1 : 0;
}

extern "C" int openrac_lib_sly1_cd_file_close(void) {
    g_file_request_ready.store(false, std::memory_order_relaxed);
    return 0;
}

extern "C" int openrac_guest_disc_file_info(const char* path, uint32_t* first_sector,
                                              uint32_t* byte_size) {
    return g_disc.find_file(path, first_sector, byte_size) ? 1 : 0;
}

extern "C" int openrac_guest_disc_read_file(const char* path, uint32_t file_offset,
                                              uint32_t byte_count, uint32_t guest_address) {
    uint32_t first_sector = 0;
    uint32_t file_size = 0;
    if (!g_disc.find_file(path, &first_sector, &file_size) || file_offset > file_size ||
        byte_count > file_size - file_offset) return 0;
    const uint32_t address = openrac_guest_ram_address(guest_address);
    if (address > openrac_guest_ram_size || byte_count > openrac_guest_ram_size - address) {
        openrac_guest_missing("disc file read destination is outside guest RAM");
        return 0;
    }
    if (byte_count == 0) return 1;

    const uint32_t skipped = file_offset % kLogicalSectorSize;
    const uint32_t sectors = (skipped + byte_count + kLogicalSectorSize - 1) /
                             kLogicalSectorSize;
    std::vector<uint8_t> data(uint64_t{sectors} * kLogicalSectorSize);
    if (!g_disc.read(first_sector + file_offset / kLogicalSectorSize, sectors, data.data()))
        return 0;
    std::copy_n(data.data() + skipped, byte_count, openrac_guest_base + address);
    return 1;
}
