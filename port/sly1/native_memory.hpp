// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

namespace sly1 {

struct NativeFunctionAddress {
    unsigned int retail_address;
    unsigned int native_address;
};

extern const NativeFunctionAddress sly1_native_function_addresses[];
extern const unsigned int sly1_native_function_address_count;

// Maps retail ELF load-segment bytes at their 32-bit addresses for direct
// host-compiled Sly data references. MIPS code bytes are mapped non-executable.
class RetailDataMap {
public:
    struct PatchStats {
        unsigned int patched{};
        unsigned int unresolved{};
    };

    RetailDataMap() = default;
    RetailDataMap(const RetailDataMap&) = delete;
    RetailDataMap& operator=(const RetailDataMap&) = delete;
    ~RetailDataMap();

    bool map(const char* elf_path, std::string* error);
    PatchStats patch_function_pointers(const NativeFunctionAddress* functions,
                                       unsigned int function_count);

private:
    struct Mapping {
        void* address;
        unsigned long size;
    };
    struct DataRange {
        unsigned int address;
        unsigned int size;
    };
    std::vector<Mapping> mappings_;
    std::vector<DataRange> data_ranges_;
};

} // namespace sly1
