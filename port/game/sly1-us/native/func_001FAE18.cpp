// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native startup for Sly's NTSC-U global initializer table.
#include "native_function_lookup.hpp"

using u32 = unsigned int;
extern "C" int sly1_i386_syscall6(int, int, int, int, int, int, int);

namespace {

constexpr unsigned int kConstructorTable = 0x0027B130u;
constexpr unsigned int kMaximumConstructors = 64;
unsigned int g_initializers_run;

void trace_constructor(unsigned int address) {
    char line[] = "native host32: constructor 0x00000000\n";
    const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 8; ++i) line[29 + i] = hex[(address >> ((7-i)*4)) & 15];
    // Linux/i386 write(2, line, sizeof(line)-1).
    __asm__ volatile("int $0x80" : : "a"(4), "b"(2), "c"(line), "d"(sizeof(line)-1) : "memory", "cc");
}

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
        trace_constructor(constructors[index]);
        reinterpret_cast<void (*)()>(address)();
    }
}
