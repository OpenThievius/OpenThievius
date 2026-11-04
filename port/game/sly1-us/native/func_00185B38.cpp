// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only host32 version of the Sly frame callback initializer.
#include "native_function_lookup.hpp"

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
    func_00193210(storage, reinterpret_cast<unsigned int*>(storage + 0x30),
                  reinterpret_cast<unsigned int*>(storage + 0x40),
                  reinterpret_cast<unsigned int*>(storage + 0x50),
                  reinterpret_cast<unsigned int*>(storage + 0x60));
    func_00191E70(1);
    func_00193270(storage);

    int value = 0x1A;
    for (unsigned int i = 0; i < 29; ++i, --value) {
        if (i >= 3) func_00193288(storage, value);
        const unsigned int native = sly1::native_function_address(callbacks[i]);
        if (native != 0) reinterpret_cast<void (*)()>(native)();
    }

    func_00193288(storage, 0);
    func_00193300(storage);
    func_00191EB0();
}
