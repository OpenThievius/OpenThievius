// SPDX-License-Identifier: AGPL-3.0-or-later

#include "openrac/disc_image.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

#include "openrac/guest.h"

namespace {

constexpr uint32_t kLogicalSectorSize = 2048;

class DiscImage {
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
        } else if (size % 2352 == 0 && has_primary_volume(input, 0, 24, 2352)) {
            stride = 2352;
            payload = 24;
        } else if (size > uint64_t{150} * 2352 && (size - uint64_t{150} * 2352) % 2352 == 0 &&
                   has_primary_volume(input, uint64_t{150} * 2352, 24, 2352)) {
            stride = 2352;
            payload = 24;
            base = uint64_t{150} * 2352;
        } else {
            *reason = "disc image has no ISO 9660 primary volume descriptor at logical sector 16";
            return false;
        }

        input_ = std::move(input);
        path_ = path;
        base_ = base;
        stride_ = stride;
        payload_ = payload;
        sectors_ = static_cast<uint32_t>((size - base) / stride);
        return true;
    }

    bool read(uint32_t first, uint32_t count, uint8_t* output) {
        if (count == 0) return input_.is_open() && first <= sectors_;
        if (!input_.is_open() || first > sectors_ || count > sectors_ - first || output == nullptr) return false;
        std::lock_guard guard(mutex_);
        const uint64_t byte_count = uint64_t{count} * kLogicalSectorSize;
        if (byte_count > static_cast<uint64_t>(std::numeric_limits<std::streamsize>::max())) return false;
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
};

DiscImage g_disc;

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
