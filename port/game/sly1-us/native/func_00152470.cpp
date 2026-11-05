// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only host adaptation of Sly's list append operation. EE low-memory
// pointers cannot be mapped directly by Linux when mmap_min_addr is enabled;
// translate the list and item into the launcher's equal-offset low-RAM shadow
// before following or storing their links. The matching body stays staged.
extern "C" unsigned int func_00152460(void* list, unsigned int value);

namespace {
unsigned int mapped_ee_pointer(unsigned int address) {
    return address != 0 && address < 0x10000u ? address + 0x10000u : address;
}
}

extern "C" void func_00152470(void* list, unsigned int item) {
    auto* self = reinterpret_cast<unsigned int*>(mapped_ee_pointer(
        reinterpret_cast<unsigned int>(list)));
    item = mapped_ee_pointer(item);

    auto* slot = reinterpret_cast<unsigned int*>(func_00152460(self, item));
    const unsigned int tail = self[1];
    if (tail == 0) {
        self[0] = item;
    } else {
        auto* old_tail = reinterpret_cast<unsigned int*>(func_00152460(self, tail));
        slot[1] = tail;
        old_tail[0] = item;
    }
    self[1] = item;
}
