// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native initialization of a Sly frame callback record.
namespace {

void clear_words(void* address, unsigned int count) {
    auto* words = static_cast<unsigned int*>(address);
    for (unsigned int i = 0; i < count; ++i) words[i] = 0;
}

} // namespace

extern "C" void func_00152888(void* record) {
    auto* words = static_cast<unsigned int*>(record);
    clear_words(record, 4);
    words[4] = 0;
    words[5] = 0;
    words[6] = 0x0024A0E0u;
}

extern "C" void func_001611B8(void* record) {
    func_00152888(record);
    auto* words = static_cast<unsigned int*>(record);
    words[5] = 1;
    words[6] = 0x0024A6F8u;
    words[7] = 0;
    words[8] = 0;
    words[9] = 0;
}

extern "C" void func_001EE318(void* record) {
    func_00152888(record);
    auto* words = static_cast<unsigned int*>(record);
    words[4] = 1;
    words[6] = 0x0024A0E0u;
    words[7] = 0;
    words[8] = 0;
}
