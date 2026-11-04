/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef OPENRAC_NATIVE_GS_H
#define OPENRAC_NATIVE_GS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct openrac_gs_vertex {
    float x;
    float y;
    uint32_t rgba;
} openrac_gs_vertex;

/* The native EE device layer handles selected MMIO stores. Returns one when
 * it handled the store; other addresses use the ordinary guest-memory path. */
int openrac_guest_ee_mmio_store32(uint32_t address, uint32_t value);
int openrac_guest_gs_privileged_store64(uint32_t address, uint64_t value);
int openrac_guest_vif1_mmio_store32(uint32_t address, uint32_t value);
void openrac_guest_gs_reset(void);
/* Feed one 16-byte EE write to the VIF1 FIFO through the native VIF command path. */
int openrac_guest_vif1_fifo_write(uint32_t word0, uint32_t word1,
                                  uint32_t word2, uint32_t word3);

/* Native GS frontends consume the ordered GIF qword stream from this FIFO. */
size_t openrac_guest_gif_fifo_read(void* destination, size_t capacity);
size_t openrac_guest_gif_fifo_pending(void);

/* GIF stream state decoded from the FIFO in submission order. */
int openrac_guest_gs_read_register(uint32_t address, uint64_t* value);
uint64_t openrac_guest_gs_register_write_count(uint32_t address);
int openrac_guest_gs_read_privileged_register(uint32_t offset, uint64_t* value);
uint64_t openrac_guest_gif_eop_count(void);
uint64_t openrac_guest_gif_image_tag_count(void);
uint64_t openrac_guest_gif_image_qword_count(void);
void openrac_guest_gs_publish_frame(void);
size_t openrac_guest_gs_read_vertices(openrac_gs_vertex* destination, size_t capacity);
size_t openrac_guest_gs_published_vertex_count(void);
void openrac_guest_gif_stream_write(const void* source, size_t size);

/* Synchronously collect a guest DMAtag chain and queue its raw GIF packet data
 * for OpenRAC's native 2D renderer. */
void openrac_native_renderer_submit_dma_chain(uint32_t source_address);
uint64_t openrac_native_renderer_dma_submit_count(void);
uint64_t openrac_native_renderer_dma_accept_count(void);
uint64_t openrac_native_renderer_dma_reject_count(void);
uint64_t openrac_native_renderer_dma_bytes(void);

#ifdef __cplusplus
}
#endif

#endif
