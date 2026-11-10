// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only host32 replacement for Sly's affine coordinate helper.
namespace {

void copy_vector(const float* input, float* output) {
    for (unsigned i = 0; i < 4; ++i) output[i] = input[i];
}

int first_word(const float* input) {
    unsigned int bits = 0;
    const auto* source = reinterpret_cast<const unsigned char*>(input);
    auto* destination = reinterpret_cast<unsigned char*>(&bits);
    for (unsigned i = 0; i < sizeof(bits); ++i) destination[i] = source[i];
    return static_cast<int>(bits);
}

float read_float(const void* base, unsigned int offset) {
    return *reinterpret_cast<const float*>(
        reinterpret_cast<const unsigned char*>(base) + offset);
}

float dot3(const void* row, const float* value) {
    float result = read_float(row, 0) * value[0];
    result += read_float(row, 4) * value[1];
    result += read_float(row, 8) * value[2];
    return result;
}

}  // namespace

extern "C" int func_00128018(void* first_transform, void* second_transform,
                              const void* input_address,
                              void* output_address) {
    const auto* input = reinterpret_cast<const float*>(input_address);
    auto* output = reinterpret_cast<float*>(output_address);

    // Retail takes this fast path when both transform arguments alias.
    if (first_transform == second_transform) {
        copy_vector(input, output);
        return first_word(input);
    }

    if (second_transform == nullptr) {
        copy_vector(input, output);
        return first_word(input);
    }

    float relative[4] = {input[0], input[1], input[2], input[3]};
    for (unsigned lane = 0; lane < 3; ++lane) {
        relative[lane] -= read_float(second_transform, 0x140 + lane * 4);
    }

    output[0] = dot3(reinterpret_cast<const unsigned char*>(second_transform) + 0x110,
                     relative);
    output[1] = dot3(reinterpret_cast<const unsigned char*>(second_transform) + 0x120,
                     relative);
    output[2] = dot3(reinterpret_cast<const unsigned char*>(second_transform) + 0x130,
                     relative);
    // The retail vector instruction leaves lane 3 in persistent VU state. The
    // native coordinate path carries through the input homogeneous lane.
    output[3] = input[3];

    return static_cast<int>(reinterpret_cast<unsigned int>(second_transform) + 0x110u);
}
