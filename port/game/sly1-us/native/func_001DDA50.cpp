// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native backfill based on NTSC-U retail behavior.
extern "C" void func_0016E118(int bits);

extern "C" void func_001DDA50(void* object) {
    auto* words = static_cast<unsigned int*>(object);
    const unsigned int references = --words[0x231c / sizeof(unsigned int)];
    if (references == 0) func_0016E118(2);
}
