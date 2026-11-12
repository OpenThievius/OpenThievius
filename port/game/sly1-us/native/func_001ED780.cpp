// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only host-float implementation of the retail perpendicular-basis helper.
// Retail forms these vectors with VU0 cross products and normalization.
namespace {
struct Vec3 {
    float x, y, z;
};

Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

float length_squared(const Vec3& value)
{
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

Vec3 normalized(const Vec3& value)
{
    const float inverse_length = 1.0f / __builtin_sqrtf(length_squared(value));
    return {value.x * inverse_length,
            value.y * inverse_length,
            value.z * inverse_length};
}
}

extern "C" void func_001ED780(const float* direction, float* output,
                              const float* fallback_a,
                              const float* fallback_b)
{
    const Vec3 axis{direction[0], direction[1], direction[2]};
    const Vec3 primary{fallback_a[0], fallback_a[1], fallback_a[2]};
    const Vec3 alternate{fallback_b[0], fallback_b[1], fallback_b[2]};

    Vec3 first = cross(primary, axis);
    if (length_squared(first) < 0.0001f)
        first = cross(alternate, axis);
    const Vec3 second = cross(axis, first);
    const Vec3 unit_first = normalized(first);
    const Vec3 unit_second = normalized(second);

    output[0] = unit_first.x;
    output[1] = unit_first.y;
    output[2] = unit_first.z;
    output[3] = 0.0f;
    output[4] = unit_second.x;
    output[5] = unit_second.y;
    output[6] = unit_second.z;
    output[7] = 0.0f;
}
