// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only scalar implementation of Sly's relative-vector transform helper.

namespace {

f32 read_lane(const void* transform, u32 row, u32 column) {
    const auto* bytes = static_cast<const u8*>(transform);
    return *reinterpret_cast<const f32*>(bytes + 0x110 + row * 0x10 + column * 4);
}

void copy_vector(const f32* input, f32* output) {
    for (u32 lane = 0; lane < 4; ++lane) output[lane] = input[lane];
}

void apply_forward(void* transform, const f32* input, f32* output) {
    for (u32 row = 0; row < 3; ++row) {
        output[row] = read_lane(transform, row, 0) * input[0]
                    + read_lane(transform, row, 1) * input[1]
                    + read_lane(transform, row, 2) * input[2];
    }
    output[3] = input[3];
}

void apply_inverse(void* transform, const f32* input, f32* output) {
    for (u32 column = 0; column < 3; ++column) {
        output[column] = read_lane(transform, 0, column) * input[0]
                      + read_lane(transform, 1, column) * input[1]
                      + read_lane(transform, 2, column) * input[2];
    }
    output[3] = input[3];
}

}  // namespace

extern "C" void func_001280C0(void* first_transform, void* second_transform,
                                const f32* input, f32* output) {
    if (first_transform == second_transform) {
        copy_vector(input, output);
        return;
    }

    f32 intermediate[4];
    const f32* source = input;
    if (first_transform != nullptr) {
        apply_forward(first_transform, input, intermediate);
        source = intermediate;
    }
    if (second_transform != nullptr) {
        apply_inverse(second_transform, source, output);
    } else if (source == input) {
        copy_vector(input, output);
    } else {
        copy_vector(intermediate, output);
    }
}
