// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native initialization of Sly's NTSC-U display-control bytes.
extern "C" void func_00180E38(int enabled, int mode) {
    if (mode != -1 || enabled == 0) return;
    auto* display = reinterpret_cast<unsigned char*>(0x002640A0u);
    auto* timing = reinterpret_cast<unsigned char*>(0x00264098u);
    display[0] = 0x00;
    display[1] = 0x87;
    display[2] = 0xFF;
    display[3] = 0x7F;
    timing[0] = 0xBF;
    timing[1] = 0xBF;
    timing[2] = 0xBF;
    timing[3] = 0x7F;
}
