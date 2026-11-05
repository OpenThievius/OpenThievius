// SPDX-License-Identifier: AGPL-3.0-or-later
// Retail waits for two IOP RPC binds before querying the CD/DVD service. The
// native host has already opened the disc image and performs sector reads
// synchronously, so no IOP service thread or bind loop is needed here.
extern "C" int func_002040E8(void* argument) {
    (void)argument;
    return 1;
}
