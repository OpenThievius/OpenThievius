// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only completion for IOP RPCs while the PS2-side command and sound
// services are absent. The matching implementation remains in sly1-decomp
// and is not changed by this host backfill.
static int complete_native_iop_call(void* rpc, void* receive, int receive_size) {
    // The command-channel client is polled by IsIopReplyReady. Supply the
    // reply-buffer sentinels that its retail completion check expects.
    if (rpc == reinterpret_cast<void*>(0x0827B4C8u)) {
        if (receive != nullptr && receive_size >= 3 * static_cast<int>(sizeof(int))) {
            auto* words = static_cast<int*>(receive);
            words[0] = -1;
            words[1] = 0;
            words[2] = -1;
        }
        return 0;
    }

    // The bank-load client has no IOP peer either; its one-word result does
    // not use the command reply-buffer sentinel protocol.
    if (rpc == reinterpret_cast<void*>(0x0827B4F0u)) {
        if (receive != nullptr && receive_size >= static_cast<int>(sizeof(int)))
            *static_cast<int*>(receive) = 0;
        return 0;
    }
    return -1;
}

// Decompiled callers use this source-level name. The matching body is kept
// staged as decomp_CallRemoteService, so the host port supplies the service.
struct RemoteCallClient;
using RemoteCallEndFn = void (*)(void*);
int CallRemoteService(RemoteCallClient* rpc, int command, int flags,
                      void* send, int send_size, void* receive, int receive_size,
                      RemoteCallEndFn callback, void* user_data) {
    (void)command;
    (void)flags;
    (void)send;
    (void)send_size;
    (void)callback;
    (void)user_data;
    return complete_native_iop_call(rpc, receive, receive_size);
}

// Legacy translated call sites still enter the retail address directly.
extern "C" int func_001F8290(void* rpc, int command, int flags,
                              void* send, int send_size,
                              void* receive, int receive_size,
                              int callback, int user_data) {
    (void)command;
    (void)flags;
    (void)send;
    (void)send_size;
    (void)callback;
    (void)user_data;
    return complete_native_iop_call(rpc, receive, receive_size);
}
