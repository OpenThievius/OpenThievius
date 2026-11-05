// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native disc-ready service. The launcher opens and validates the
// selected ISO before entering Sly, so the IOP device-ready poll completes
// synchronously. Keep the missing retail function out of the decomp tree.
extern "C" int func_00203828(int device) {
    (void)device;
    return 2;
}
