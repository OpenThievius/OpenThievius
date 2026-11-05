// SPDX-License-Identifier: AGPL-3.0-or-later
// The retail callback loads ioprp243.img and six IOP modules before binding
// CDVD services. The native host opens the ISO itself and serves sector reads
// synchronously, so replace the PS2-only bootstrap with the runtime's native
// CD and service initialization.
// Keep it port-only so the retail implementation remains available to match.
extern "C" int sly1_native_host32_iop_boot(void);
extern "C" int sly1_native_host32_iop_initialize(int mode);
extern "C" int sly1_native_host32_iop_services_start(int mode);
extern "C" volatile unsigned int sly1_native_iop_boot_stage;

extern "C" void func_001858F8() {
    sly1_native_iop_boot_stage = 1;
    const int booted = sly1_native_host32_iop_boot();
    if (!booted) return;
    sly1_native_iop_boot_stage = 2;

    const int initialized = sly1_native_host32_iop_initialize(0);
    if (!initialized) return;
    sly1_native_iop_boot_stage = 3;
    if (!sly1_native_host32_iop_services_start(0)) return;
    sly1_native_iop_boot_stage = 4;
}
