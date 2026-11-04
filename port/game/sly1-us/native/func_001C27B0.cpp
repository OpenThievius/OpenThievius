// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native setup for Sly's two EE-to-scratchpad transfer descriptors.
extern "C" void func_001C27B0(int enabled, int mode) {
    if (mode != -1 || enabled == 0) return;

    auto* first = reinterpret_cast<unsigned int*>(0x002748F0u);
    auto* second = reinterpret_cast<unsigned int*>(0x00274940u);
    for (unsigned int i = 0; i < 0x50u / sizeof(unsigned int); ++i) {
        first[i] = 0;
        second[i] = 0;
    }
    first[0] = 0x20605440u;
    first[1] = 0x70000000u;
    second[0] = 0x206054C0u;
    second[1] = 0x70002000u;
}
