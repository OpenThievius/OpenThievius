/* SPDX-License-Identifier: AGPL-3.0-or-later */
// Port-only graphics wait boundary. Native DMA submissions are synchronous,
// so the retail busy-poll over EE/VU registers has already completed.
extern "C" void func_00201988(int, int) {}
