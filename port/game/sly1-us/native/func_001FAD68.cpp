// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only replacement for the initializer-table interior entry in retail
// assembly func_001FAC00. The host CRT and Sly's retail initializer list are
// separate tables; the packet-writer callback needs a host-side bridge.
using Constructor = void (*)();

extern "C" Constructor __init_array_start[];
extern "C" Constructor __init_array_end[];
extern "C" int sly1_native_host32_crt_initialized;
extern "C" void func_0015F658();

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
    // The freestanding diagnostic has no C runtime to execute its own
    // initializers. SDL builds have already run these before main.
    if (sly1_native_host32_crt_initialized == 0) {
        for (Constructor* current = __init_array_start;
             current != __init_array_end; ++current) {
            trace_constructor(*current);
            (*current)();
        }
    }

    // The retail constructor list also calls ResetPacketWriters. It is an
    // explicit callback, not a C++ global constructor, so the host CRT never
    // includes it in __init_array. Initialize its globals after the image is
    // mapped and before the first frame submits a GS packet.
    func_0015F658();
}
