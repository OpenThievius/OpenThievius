// SPDX-License-Identifier: AGPL-3.0-or-later
#include "native_memory.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <algorithm>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

namespace sly1 {
namespace {

std::uint16_t u16(const std::uint8_t* p) {
    return std::uint16_t(p[0]) | (std::uint16_t(p[1]) << 8);
}

std::uint32_t u32(const std::uint8_t* p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}

std::uintptr_t align_down(std::uintptr_t value, std::uintptr_t alignment) {
    return value & ~(alignment - 1);
}

std::uintptr_t align_up(std::uintptr_t value, std::uintptr_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

} // namespace

RetailDataMap::~RetailDataMap() {
    for (auto it = mappings_.rbegin(); it != mappings_.rend(); ++it)
        munmap(it->address, it->size);
}

bool RetailDataMap::map(const char* elf_path, std::string* error) {
    if (sizeof(void*) != 4) {
        if (error) *error = "retail data mapping requires a 32-bit host process";
        return false;
    }
    if (!mappings_.empty()) {
        if (error) *error = "retail data image is already mapped";
        return false;
    }
    if (!elf_path || !*elf_path) {
        if (error) *error = "retail ELF path is empty";
        return false;
    }

    std::ifstream file(elf_path, std::ios::binary | std::ios::ate);
    if (!file) {
        if (error) *error = "could not open retail ELF";
        return false;
    }
    const auto end = file.tellg();
    if (end < 52 || static_cast<std::uint64_t>(end) > std::numeric_limits<std::size_t>::max()) {
        if (error) *error = "retail ELF has an invalid size";
        return false;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (file.gcount() != static_cast<std::streamsize>(bytes.size())) {
        if (error) *error = "short read from retail ELF";
        return false;
    }
    if (bytes[0] != 0x7f || bytes[1] != 'E' || bytes[2] != 'L' || bytes[3] != 'F' ||
        bytes[4] != 1 || bytes[5] != 1) {
        if (error) *error = "expected a 32-bit little-endian ELF";
        return false;
    }

    const std::uint32_t phoff = u32(bytes.data() + 0x1c);
    const std::uint16_t phentsize = u16(bytes.data() + 0x2a);
    const std::uint16_t phnum = u16(bytes.data() + 0x2c);
    if (phentsize < 32 || std::uint64_t(phoff) + std::uint64_t(phentsize) * phnum > bytes.size()) {
        if (error) *error = "ELF program header table is out of bounds";
        return false;
    }

    const std::uint32_t shoff = u32(bytes.data() + 0x20);
    const std::uint16_t shentsize = u16(bytes.data() + 0x2e);
    const std::uint16_t shnum = u16(bytes.data() + 0x30);
    if (shnum != 0 && (shentsize < 40 || std::uint64_t(shoff) + std::uint64_t(shentsize) * shnum > bytes.size())) {
        if (error) *error = "ELF section header table is out of bounds";
        return false;
    }
    std::vector<DataRange> data_ranges;
    for (std::uint16_t i = 0; i < shnum; ++i) {
        const auto* section = bytes.data() + shoff + std::size_t(i) * shentsize;
        const std::uint32_t type = u32(section + 4);
        const std::uint32_t flags = u32(section + 8);
        const std::uint32_t address = u32(section + 12);
        const std::uint32_t offset = u32(section + 16);
        const std::uint32_t size = u32(section + 20);
        constexpr std::uint32_t kAlloc = 0x2;
        constexpr std::uint32_t kExec = 0x4;
        constexpr std::uint32_t kProgbits = 1;
        if ((flags & kAlloc) && !(flags & kExec) && type == kProgbits && size != 0) {
            if (std::uint64_t(offset) + size > bytes.size()) {
                if (error) *error = "ELF data section extends beyond the file";
                return false;
            }
            data_ranges.push_back({address, size});
        }
    }

    const long page_value = sysconf(_SC_PAGESIZE);
    if (page_value <= 0 || (page_value & (page_value - 1)) != 0) {
        if (error) *error = "host page size is invalid";
        return false;
    }
    const auto page = static_cast<std::uintptr_t>(page_value);
    std::vector<Mapping> pending;
    auto fail = [&]() {
        for (auto it = pending.rbegin(); it != pending.rend(); ++it)
            munmap(it->address, it->size);
        return false;
    };
    for (std::uint16_t i = 0; i < phnum; ++i) {
        const auto* p = bytes.data() + phoff + std::size_t(i) * phentsize;
        if (u32(p) != 1) continue; // PT_LOAD
        const std::uint32_t offset = u32(p + 4);
        const std::uint32_t address = u32(p + 8);
        const std::uint32_t file_size = u32(p + 16);
        const std::uint32_t memory_size = u32(p + 20);
        if (file_size > memory_size || std::uint64_t(offset) + file_size > bytes.size()) {
            if (error) *error = "ELF load segment has invalid bounds";
            return fail();
        }
        if (memory_size == 0) continue;
        if ((address & (page - 1)) != (offset & (page - 1))) {
            if (error) *error = "ELF segment address and file offset are not page congruent";
            return fail();
        }
        const auto map_address = align_down(address, page);
        const auto delta = static_cast<std::uintptr_t>(address) - map_address;
        const auto map_size = align_up(delta + memory_size, page);
        void* mapped = mmap(reinterpret_cast<void*>(map_address), map_size,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (mapped == MAP_FAILED) {
            if (error) *error = "could not reserve retail data address range";
            return fail();
        }
        pending.push_back({mapped, static_cast<unsigned long>(map_size)});
        if (file_size != 0)
            std::memcpy(reinterpret_cast<void*>(address), bytes.data() + offset, file_size);
    }
    if (pending.empty()) {
        if (error) *error = "ELF contains no loadable segment";
        return fail();
    }
    mappings_ = std::move(pending);
    data_ranges_ = std::move(data_ranges);
    return true;
}

RetailDataMap::PatchStats RetailDataMap::patch_function_pointers(
    const NativeFunctionAddress* functions, unsigned int function_count) {
    PatchStats stats{};
    if (sizeof(void*) != 4 || mappings_.empty() || !functions || function_count == 0) return stats;
    for (const auto& range : data_ranges_) {
        auto* cursor = reinterpret_cast<std::uint32_t*>(range.address);
        const auto words = range.size / sizeof(std::uint32_t);
        for (std::uint32_t i = 0; i < words; ++i) {
            const auto end = functions + function_count;
            const auto it = std::lower_bound(functions, end, cursor[i],
                [](const NativeFunctionAddress& function, std::uint32_t address) {
                    return function.retail_address < address;
                });
            if (it == end || it->retail_address != cursor[i]) continue;
            if (it->native_address == 0) {
                ++stats.unresolved;
            } else {
                cursor[i] = it->native_address;
                ++stats.patched;
            }
        }
    }
    return stats;
}

} // namespace sly1
