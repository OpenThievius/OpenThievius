// SPDX-License-Identifier: AGPL-3.0-or-later
// Host replacement for the handwritten VU0 vmulx.xyzw helper.
using NativeQuad = float __attribute__((vector_size(16)));

extern "C" NativeQuad func_00118D30(NativeQuad value, NativeQuad scale)
{
    return value * scale[0];
}
