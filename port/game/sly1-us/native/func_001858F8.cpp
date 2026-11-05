// SPDX-License-Identifier: AGPL-3.0-or-later
// The retail callback loads ioprp243.img and six IOP modules before binding
// CDVD services. The native host opens the ISO itself and serves sector reads
// synchronously, so this PS2-only bootstrap has no host work to perform.
// Keep it port-only so the retail implementation remains available to match.
extern "C" void func_001858F8() {}
