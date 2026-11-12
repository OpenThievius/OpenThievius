// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native implementation of the bounded, zero-filling string copy.
extern "C" void* func_00201720(void* destination, const char* source,
                               int capacity)
{
    auto* output = static_cast<unsigned char*>(destination);
    int index = 0;
    while (index < capacity && source[index] != '\0') {
        output[index] = static_cast<unsigned char>(source[index]);
        ++index;
    }
    while (index < capacity) {
        output[index] = 0;
        ++index;
    }
    return destination;
}
