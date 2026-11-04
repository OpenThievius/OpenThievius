// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native backfill for Sly's NTSC-U render configuration constructor.
namespace {

void write_record(unsigned int address, unsigned char marker, float vertical) {
    auto* bytes = reinterpret_cast<unsigned char*>(address);
    for (unsigned int i = 0; i < 0x1c; ++i) bytes[i] = 0;
    bytes[4] = marker;
    auto* values = reinterpret_cast<float*>(address + 8);
    values[0] = 2.0f;
    values[1] = vertical;
    values[2] = 0.3f;
    values[3] = 0.3f;
    *reinterpret_cast<unsigned int*>(address + 0x18) = 0xFF7D4B00u;
}

}  // namespace

extern "C" void func_001AEA70(int enabled, int mode) {
    if (mode != 0xFFFF || enabled == 0) return;

    write_record(0x002743D0u, 0x2D, 1.0f);
    write_record(0x002743F0u, 0x2D, -4.0f);
    write_record(0x00274410u, 0x3D, -4.0f);
    write_record(0x00274450u, 0x2D, -4.0f);
    write_record(0x00274488u, 0x2D, -4.0f);
}
