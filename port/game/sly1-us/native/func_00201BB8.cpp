// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only replacement for the GS privileged-register writes. The native
// DirectRenderer owns the host framebuffer and consumes the GIF packets, so
// console display-mode registers have no host-side register page to update.
extern "C" void func_00201BB8(void*) {}
