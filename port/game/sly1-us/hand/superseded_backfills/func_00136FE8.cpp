// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native setup for Sly's NTSC-U palette and UI color presets.
namespace {

void set_bytes(unsigned int address, unsigned char a, unsigned char b,
               unsigned char c, unsigned char d) {
    auto* p = reinterpret_cast<unsigned char*>(address);
    p[0] = a; p[1] = b; p[2] = c; p[3] = d;
}

void clear_bytes(unsigned int address, unsigned int count) {
    auto* p = reinterpret_cast<unsigned char*>(address);
    for (unsigned int i = 0; i < count; ++i) p[i] = 0;
}

} // namespace

extern "C" void func_00136FE8(int enabled, int mode) {
    if (mode != -1 || enabled == 0) return;

    set_bytes(0x002610D0u, 0x00, 0x00, 0x00, 0x80);
    set_bytes(0x002610D8u, 0x00, 0x52, 0x7E, 0x80);
    set_bytes(0x002610E0u, 0x19, 0x1E, 0x6E, 0x80);
    set_bytes(0x002610E8u, 0x7E, 0x28, 0x0A, 0x80);
    set_bytes(0x002610F0u, 0x50, 0x28, 0x28, 0x80);
    set_bytes(0x002610F8u, 0xF0, 0xF0, 0x00, 0x80);
    set_bytes(0x00261100u, 0x28, 0x28, 0x00, 0x80);
    set_bytes(0x00261108u, 0xFF, 0xFF, 0xFF, 0x80);
    set_bytes(0x00261110u, 0xE6, 0xE6, 0xE6, 0x80);
    set_bytes(0x00261118u, 0x00, 0x72, 0x3E, 0x80);

    auto* first = reinterpret_cast<unsigned char*>(0x00261148u);
    auto* second = reinterpret_cast<unsigned char*>(0x00261168u);
    clear_bytes(0x00261148u, 0x1C);
    clear_bytes(0x00261168u, 0x1C);
    *reinterpret_cast<unsigned int*>(first + 0x18) = 0xFF7D4B00u;
    *reinterpret_cast<float*>(first + 0x08) = 2.0f;
    first[0x04] = 0x2D;
    *reinterpret_cast<float*>(first + 0x10) = 0.3f;
    *reinterpret_cast<float*>(first + 0x14) = 0.3f;

    *reinterpret_cast<unsigned int*>(second + 0x18) = 0xFF7D4B00u;
    second[0x04] = 0x2D;
    *reinterpret_cast<float*>(second + 0x08) = 2.0f;
    *reinterpret_cast<float*>(second + 0x0C) = 16.0f;
    *reinterpret_cast<float*>(second + 0x10) = 0.3f;
    *reinterpret_cast<float*>(second + 0x14) = 0.3f;
}
