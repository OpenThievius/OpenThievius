// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native coordinate normalization for Sly's movement update.
int func_001EACA0(float* roots, float radius, float dy, float dx);

namespace {

float read_float(const void* base, unsigned int offset) {
    return *reinterpret_cast<const float*>(
        reinterpret_cast<const unsigned char*>(base) + offset);
}

float max_float(float lhs, float rhs) {
    return lhs > rhs ? lhs : rhs;
}

}  // namespace

extern "C" void func_001BC260(float tick, void* object,
                               const float* input, float* output) {
    const float object_height = read_float(object, 328);
    const float input_height = input[2];
    const float scale = read_float(object, 856);
    const float height_span = max_float(input_height, object_height) + tick - object_height;
    const float root = __builtin_sqrtf(scale * (height_span * -2.0f));
    output[2] = root;

    float roots[2] = {0.0f, 0.0f};
    const float radius = scale * 0.5f;
    const float dx = object_height - input_height;
    const int root_count = func_001EACA0(roots, radius, root, dx);
    if (root_count == 1) roots[1] = roots[0];

    const float divisor = max_float(roots[0], roots[1]);
    output[0] = (input[0] - read_float(object, 320)) / divisor;
    output[1] = (input[1] - read_float(object, 324)) / divisor;
}
