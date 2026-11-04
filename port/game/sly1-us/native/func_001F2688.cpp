// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native backfill for Sly's NTSC-U render configuration constructor.
extern "C" void func_001F2688(int enabled, int mode) {
    if (mode != 0xFFFF || enabled == 0) return;

    constexpr unsigned int address = 0x00276620u;
    auto* bytes = reinterpret_cast<unsigned char*>(address);
    for (unsigned int i = 0; i < 0x1c; ++i) bytes[i] = 0;
    bytes[4] = 0x3D;
    auto* values = reinterpret_cast<float*>(address + 8);
    values[0] = 2.0f;
    values[1] = 1.0f;
    values[2] = 0.3f;
    values[3] = 0.3f;
    *reinterpret_cast<unsigned int*>(address + 0x18) = 0xFF7D4B00u;

    const float source = *reinterpret_cast<float*>(0x00276648u);
    *reinterpret_cast<float*>(0x00276650u) = source * 0.75f;
}
