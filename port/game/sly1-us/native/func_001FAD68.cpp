// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only replacement for the initializer-table interior entry in retail
// assembly func_001FAC00. The native C++ linker supplies this process's table.
using Constructor = void (*)();

extern "C" Constructor __init_array_start[];
extern "C" Constructor __init_array_end[];
extern "C" int sly1_native_host32_crt_initialized;

namespace {
void trace_constructor(Constructor constructor) {
    const unsigned int address = reinterpret_cast<unsigned int>(constructor);
    char line[] = "native host32: init 0x00000000\n";
    const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 8; ++i)
        line[22 + i] = hex[(address >> ((7 - i) * 4)) & 15];
    __asm__ volatile("int $0x80" : : "a"(4), "b"(2), "c"(line),
                     "d"(sizeof(line) - 1) : "memory", "cc");
}
} // namespace

extern "C" void func_001FAD68() {
    // A normally linked SDL build has already run this table through the
    // platform C runtime before entering Sly. The freestanding diagnostic
    // launcher leaves the flag clear and still dispatches the table here.
    if (sly1_native_host32_crt_initialized != 0) return;
    for (Constructor* current = __init_array_start;
         current != __init_array_end; ++current) {
        trace_constructor(*current);
        (*current)();
    }
}
