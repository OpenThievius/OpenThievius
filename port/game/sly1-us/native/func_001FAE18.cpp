// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native startup for Sly's NTSC-U global initializer table.
#include "native_function_lookup.hpp"

namespace {

constexpr unsigned int kConstructorTable = 0x0027B130u;
constexpr unsigned int kMaximumConstructors = 64;
unsigned int g_initializers_run;

}  // namespace

extern "C" void func_001FAE18() {
    if (g_initializers_run != 0) return;
    g_initializers_run = 1;

    // The retail NTSC-U image stores a count followed by its constructor
    // addresses at this table. Run backwards, as the retail initializer does.
    const auto* constructors = reinterpret_cast<const unsigned int*>(kConstructorTable);
    const unsigned int count = constructors[0];
    if (count > kMaximumConstructors) return;
    for (unsigned int index = count; index != 0; --index) {
        const unsigned int address = sly1::native_function_address(constructors[index]);
        if (address == 0) continue;
        reinterpret_cast<void (*)()>(address)();
    }
}
