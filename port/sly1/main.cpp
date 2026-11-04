// SPDX-License-Identifier: AGPL-3.0-or-later
// Native launcher for the translated Sly 1 executable.
#include "openrac/elf.h"
#include "openrac/game_host.h"
#include "openrac/memory.h"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: sly1_native <extracted Sly 1 boot executable>\n";
        return 2;
    }

    std::vector<std::uint8_t> executable;
    if (!openrac::runtime::read_file(argv[1], &executable)) {
        std::cerr << "could not read the Sly 1 boot executable: " << argv[1] << '\n';
        return 1;
    }

    openrac::runtime::ElfImage image;
    std::string reason;
    if (!openrac::runtime::read_elf(executable, &image, &reason)) {
        std::cerr << "invalid Sly 1 boot executable: " << reason << '\n';
        return 1;
    }

    openrac::runtime::Memory::create();
    if (!openrac::runtime::load_elf(executable, image, &reason)) {
        std::cerr << "could not load the Sly 1 boot executable: " << reason << '\n';
        return 1;
    }

    openrac::runtime::install_crash_handler();
    openrac_game_register_functions();
    openrac_guest_set_overlay(OPENRAC_OVERLAY_EXE);
    if (!openrac_game_run()) {
        std::cerr << "the Sly 1 entry point is not available in this build\n";
        return 1;
    }
    return 0;
}
