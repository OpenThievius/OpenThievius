// SPDX-License-Identifier: AGPL-3.0-or-later
// Retail waits here for a GS interrupt to signal the display semaphore. The
// native host completes GIF DMA synchronously, so there is no pending display
// operation to wait for. Preserve the completion flag and keep this override
// separate from the matching retail body.
extern "C" int D_002BE46C;

extern "C" void func_001683B8() {
    D_002BE46C = 1;
}
