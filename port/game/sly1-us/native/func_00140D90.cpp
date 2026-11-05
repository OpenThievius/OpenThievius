// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only initialization for the startup subsystem state. The matching
// function is not present in the current decomp source. Retail clears the two
// 0x200-byte work areas, seeds four float lanes with FLT_MAX, and initializes
// the trailing indices to -1, -1, and 0. Keep this host replacement separate
// so matching C can replace it without affecting decompilation progress.
extern "C" void func_00140D90(void* state) {
    auto* bytes = static_cast<unsigned char*>(state);

    for (unsigned int offset = 4; offset < 0x210; ++offset)
        bytes[offset] = 0;

    const unsigned int max_float_bits = 0x7F7FFFFFu;
    auto* lanes = reinterpret_cast<unsigned int*>(bytes + 0x210);
    lanes[0] = max_float_bits;
    lanes[1] = max_float_bits;
    lanes[2] = max_float_bits;
    lanes[3] = max_float_bits;

    for (unsigned int offset = 0x220; offset < 0x42C; ++offset)
        bytes[offset] = 0;

    *reinterpret_cast<int*>(bytes + 0x424) = -1;
    *reinterpret_cast<int*>(bytes + 0x428) = -1;
    *reinterpret_cast<int*>(bytes + 0x42C) = 0;
}
