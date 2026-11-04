// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only byte-fill replacement for Sly's PS2 memset helper.
extern "C" void* func_001F5C0C(void* destination, int value, int size) {
    auto* bytes = static_cast<unsigned char*>(destination);
    unsigned int remaining = static_cast<unsigned int>(size);
    while (remaining != 0) {
        *bytes++ = static_cast<unsigned char>(value);
        --remaining;
    }
    return destination;
}
