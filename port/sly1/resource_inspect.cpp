// SPDX-License-Identifier: AGPL-3.0-or-later
#include "resource_loader.hpp"

#include <charconv>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        if (argc < 3 || argc > 5) {
            std::cerr << "usage: sly1_resource_inspect ELF DISC [KEY [OUTPUT]]\n";
            return 2;
        }
        const auto catalog = sly1::read_catalog(argv[1], argv[2]);
        std::uint32_t wanted = 0x0100;
        if (argc >= 4) {
            const std::string key = argv[3];
            const auto* begin = key.data() + (key.starts_with("0x") ? 2 : 0);
            const auto result = std::from_chars(begin, key.data() + key.size(), wanted, 16);
            if (result.ec != std::errc{} || result.ptr != key.data() + key.size()) throw std::runtime_error("resource key must be hexadecimal");
        }
        for (const auto& resource : catalog) {
            if (resource.key != wanted) continue;
            const auto decoded = sly1::load_resource(argv[2], resource);
            std::cout << "catalog entries: " << catalog.size() << "\n"
                      << "key: 0x" << std::hex << std::setw(4) << std::setfill('0') << resource.key << std::dec << "\n"
                      << "label: " << resource.label << "\n"
                      << "sector: " << resource.sector << "\n"
                      << "compressed bytes: " << resource.compressed_bytes << "\n"
                      << "decoded bytes: " << decoded.size() << "\n";
            if (argc == 5) {
                std::ofstream out(argv[4], std::ios::binary);
                if (!out) throw std::runtime_error("cannot open output file");
                out.write(reinterpret_cast<const char*>(decoded.data()), static_cast<std::streamsize>(decoded.size()));
                if (!out) throw std::runtime_error("failed to write output file");
                std::cout << "output: " << argv[4] << "\n";
            }
            return 0;
        }
        throw std::runtime_error("resource key is not present in catalog");
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << "\n";
        return 1;
    }
}
