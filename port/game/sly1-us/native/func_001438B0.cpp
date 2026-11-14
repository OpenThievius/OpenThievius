// SPDX-License-Identifier: AGPL-3.0-or-later
// The matching SetCurrentCameraFogColor declaration omits both property
// callback arguments. Retail preserves a1 as the color pointer while replacing
// a0 with g_camera. Keep that host ABI repair in the port until the decomp
// signature is corrected.
extern void* g_camera;
extern void func_00143A08(void* camera, void* color);

extern "C" void func_001438B0(void* target, void* color) {
    (void)target;
    if (g_camera != nullptr && color != nullptr)
        func_00143A08(g_camera, color);
}
