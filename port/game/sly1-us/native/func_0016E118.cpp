// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native backfill based on NTSC-U retail behavior.
extern "C" {
extern unsigned int D_00262C6C;
void func_0016E148();
}

extern "C" void func_0016E118(int bits) {
    D_00262C6C &= ~static_cast<unsigned int>(bits);
    func_0016E148();
}
