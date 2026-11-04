// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native initialization of Sly's fixed frame/render configuration.
extern "C" int func_001930B8(int enabled, int mode) {
    if (mode != -1 || enabled == 0) return 0;

    auto* early = reinterpret_cast<unsigned char*>(0x00604700u);
    auto* early_source = reinterpret_cast<const unsigned char*>(0x00248D30u);
    for (unsigned int i = 0; i < 0x80; ++i) early[i] = 0;
    for (unsigned int i = 0; i < 0x10; ++i) early[0x60 + i] = early_source[i];

    auto* config = reinterpret_cast<unsigned char*>(0x00269C10u);
    auto* vector = reinterpret_cast<const unsigned int*>(0x00248D20u);
    for (unsigned int i = 0; i < 0x80; ++i) config[i] = 0;
    const float constants[] = {1000.0f, 1200.0f, 1.0f, -1740.0f, -0.5f,
                               0.5f, 2.0f, 1.0f, -1.0f};
    const unsigned int offsets[] = {0x00, 0x04, 0x08, 0x18, 0x20,
                                    0x50, 0x54, 0x70, 0x74};
    for (unsigned int i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        *reinterpret_cast<float*>(config + offsets[i]) = constants[i];
    }
    for (unsigned int i = 0; i < 4; ++i) {
        reinterpret_cast<unsigned int*>(config + 0x30)[i] = vector[i];
    }
    *reinterpret_cast<unsigned int*>(config + 0x4c) = 1;
    return 0;
}
