// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only completion for SND RPCs while the PS2 IOP sound module is absent.
// The matching/high-match implementation remains in sly1-decomp's
// nonmatching/func_001F8290.cpp and is not changed by this host backfill.
extern "C" int func_001F8290(void* rpc, int command, int flags,
                              void* send, int send_size,
                              void* receive, int receive_size,
                              int callback, int user_data) {
    (void)command;
    (void)flags;
    (void)send;
    (void)send_size;
    (void)receive_size;
    (void)callback;
    (void)user_data;

    // These are the two SND RPC clients initialized by func_0011CBB8.
    // Return a successful, empty result so native startup can continue silently.
    // The launcher maps EE addresses at 0x08000000 + retail address.
    if (rpc == reinterpret_cast<void*>(0x0827B4C8u) ||
        rpc == reinterpret_cast<void*>(0x0827B4F0u)) {
        if (receive != nullptr) *static_cast<int*>(receive) = 0;
        return 0;
    }
    return -1;
}
