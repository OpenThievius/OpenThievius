// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native replacement for Sly's fixed default pointer initialization.
extern "C" int func_0018F940(int enabled, int mode) {
    if (mode == -1 && enabled != 0) {
        *reinterpret_cast<unsigned int*>(0x00269AECu) = 0x0024B5B8u;
    }
    return 0;
}
