// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only host32 version of the Sly frame callback initializer.
#include "native_function_lookup.hpp"

namespace {

void trace_callback(unsigned int address, unsigned int index) {
    char line[] = "native host32: frame callback 00 at 0x00000000\n";
    const char hex[] = "0123456789abcdef";
    line[30] = hex[(index >> 4) & 15];
    line[31] = hex[index & 15];
    for (int i = 0; i < 8; ++i) line[38 + i] = hex[(address >> ((7-i)*4)) & 15];
    __asm__ volatile("int $0x80" : : "a"(4), "b"(2), "c"(line), "d"(sizeof(line)-1) : "memory", "cc");
}

void trace_step(const char* text, unsigned int size) {
    __asm__ volatile("int $0x80" : : "a"(4), "b"(2), "c"(text), "d"(size) : "memory", "cc");
}

} // namespace

void* func_00193210(void* self, unsigned int* first, unsigned int* second,
                    unsigned int* third, unsigned int* fourth);
void func_00191E70(int value);
void func_00193270(void* state);
void func_00193288(void* state, int value);
void func_00193300(void* state);
void func_00191EB0();

extern "C" void func_00185B38() {
    alignas(16) unsigned char storage[0x70]{};
    auto* callbacks = reinterpret_cast<const unsigned int*>(0x002647C0u);

    *reinterpret_cast<unsigned int*>(storage + 0x30) = 0x007F0000u;
    *reinterpret_cast<unsigned int*>(storage + 0x40) = 0x003F3F3Fu;
    *reinterpret_cast<unsigned int*>(storage + 0x50) = 0x00003F3Fu;
    *reinterpret_cast<unsigned int*>(storage + 0x60) = 0x0000003Fu;
    trace_step("native host32: frame setup A start\n", 35);
    func_00193210(storage, reinterpret_cast<unsigned int*>(storage + 0x30),
                  reinterpret_cast<unsigned int*>(storage + 0x40),
                  reinterpret_cast<unsigned int*>(storage + 0x50),
                  reinterpret_cast<unsigned int*>(storage + 0x60));
    trace_step("native host32: frame setup A done\n", 34);
    func_00191E70(1);
    trace_step("native host32: frame setup B done\n", 34);
    func_00193270(storage);
    trace_step("native host32: frame setup C done\n", 34);

    int value = 0x1A;
    for (unsigned int i = 0; i < 29; ++i, --value) {
        if (i >= 3) func_00193288(storage, value);
        const unsigned int native = sly1::native_function_address(callbacks[i]);
        if (native != 0) {
            trace_callback(callbacks[i], i);
            reinterpret_cast<void (*)()>(native)();
        }
    }

    func_00193288(storage, 0);
    func_00193300(storage);
    func_00191EB0();
}
