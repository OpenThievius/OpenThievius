// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only completion for Sly's sound-bootstrap SIF RPCs. Their IOP peer is
// absent in the native host; complete only the two startup binds that
// func_0011CBB8 waits on. The matching RPC implementation remains in the
// staged decomp source and this shim can be removed when a native sound
// service replaces those requests.
extern "C" int func_001F80C0(void* context, int service_id, int mode) {
    (void)mode;
    if (context == nullptr ||
        (service_id != 0x123456 && service_id != 0x123457)) {
        return -1;
    }
    auto* words = static_cast<int*>(context);
    words[4] = 0; // f10: no asynchronous result is pending.
    words[9] = 1; // f24: the synchronous RPC bind completed successfully.
    return 0;
}
