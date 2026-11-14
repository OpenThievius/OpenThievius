// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only CPU replacement for the original-assembly listener distance helper.
extern "C" float func_001BF398(const void* listener, const void* position) {
    const float* a = static_cast<const float*>(listener);
    const float* b = static_cast<const float*>(position);
    const float x = b[0] - a[0];
    const float y = b[1] - a[1];
    const float z = b[2] - a[2];
    return __builtin_sqrtf(x * x + y * y + z * z);
}
