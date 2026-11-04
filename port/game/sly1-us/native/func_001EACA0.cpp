// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native quadratic root helper used by Sly's coordinate update.
extern "C" int func_001EACA0(float* roots, float radius, float dy, float dx) {
    const float discriminant = dy * dy - (radius * (4.0f * dx));
    if (discriminant < 0.0f) return 0;

    const float twice_radius = radius + radius;
    const float root_delta = __builtin_sqrtf(discriminant) / twice_radius;
    constexpr float kRepeatedRootThreshold = 9.999999747378752e-05f;
    const float center = -(dy / twice_radius);

    if (__builtin_fabsf(root_delta) < kRepeatedRootThreshold) {
        roots[0] = center;
        return 1;
    }

    roots[1] = center - root_delta;
    roots[0] = center + root_delta;
    return 2;
}
