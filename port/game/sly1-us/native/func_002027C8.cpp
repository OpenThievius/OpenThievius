// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only no-op for an unimplemented native VU1/GS boundary call.
extern "C" void func_002018E8() {
    // The EE VU1/GS startup sequence has no device operations in the native path.
}
