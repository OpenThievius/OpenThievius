/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only VIF1/GIF startup reset, transcribed from US retail 0x002018E8. */
#include "common.h"

int openrac_guest_vif1_fifo_write(u32 word0, u32 word1, u32 word2, u32 word3);
int openrac_guest_ee_mmio_store32(u32 address, u32 value);

void func_002018E8(void) {
    /* Keep the register writes visible in guest MMIO memory. The native VU0
     * path runs synchronously, so retail's VI29 busy wait is already idle. */
    (void)openrac_guest_ee_mmio_store32(0x10003C10u, 1u);
    (void)openrac_guest_ee_mmio_store32(0x10003C20u, 2u);

    /* Retail's D_00279300 and D_00279310, in the order sent to VIF1 FIFO. */
    static const u32 reset_fifo_qword_0[4] = {
        0x01000404u, 0x20000000u, 0x00000000u, 0x05000000u
    };
    static const u32 reset_fifo_qword_1[4] = {
        0x06000000u, 0x03000000u, 0x02000000u, 0x04000000u
    };
    (void)openrac_guest_vif1_fifo_write(reset_fifo_qword_0[0], reset_fifo_qword_0[1],
                                        reset_fifo_qword_0[2], reset_fifo_qword_0[3]);
    (void)openrac_guest_vif1_fifo_write(reset_fifo_qword_1[0], reset_fifo_qword_1[1],
                                        reset_fifo_qword_1[2], reset_fifo_qword_1[3]);
    (void)openrac_guest_ee_mmio_store32(0x10003000u, 1u);
}
