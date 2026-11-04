/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "openrac/sif.h"

#include "openrac/guest.h"
#include "openrac/host_env.h"
#include "common/log.h"

#include <cstring>
#include <cstdio>
#include <mutex>
#include <unordered_map>

namespace {
constexpr uint32_t kSifCommandInit = 0x80000002u;
constexpr uint32_t kSifRpcBind = 0x80000009u;
constexpr uint32_t kSifRpcCall = 0x8000000Au;
constexpr uint32_t kInitCompletion = 0x006294C0u;
std::mutex g_rpc_mutex;
std::unordered_map<uint32_t, uint32_t> g_native_rpc_services;

bool guest_u32(uint32_t guest_address, uint32_t* value) {
    const uint32_t address = openrac_guest_ram_address(guest_address);
    if (address > openrac_guest_ram_size ||
        openrac_guest_ram_size - address < sizeof(*value)) return false;
    std::memcpy(value, openrac_guest_base + address, sizeof(*value));
    return true;
}

bool guest_write_u32(uint32_t guest_address, uint32_t value) {
    const uint32_t address = openrac_guest_ram_address(guest_address);
    if (address > openrac_guest_ram_size ||
        openrac_guest_ram_size - address < sizeof(value)) return false;
    std::memcpy(openrac_guest_base + address, &value, sizeof(value));
    return true;
}

int complete_rpc_bind(uint32_t packet) {
    uint32_t client = 0;
    uint32_t service = 0;
    if (!guest_u32(packet + 28, &client) || !guest_u32(packet + 32, &service)) return 0;
    const uint32_t client_address = openrac_guest_ram_address(client);
    if (client_address > openrac_guest_ram_size ||
        openrac_guest_ram_size - client_address < 40) return 0;

    uint32_t semaphore = 0xFFFFFFFFu;
    if (!guest_u32(client + 8, &semaphore)) return 0;
    if (!guest_write_u32(client + 16, 0) || !guest_write_u32(client + 20, 0) ||
        !guest_write_u32(client + 24, 0) || !guest_write_u32(client + 36, 1) ||
        !guest_write_u32(client + 0, 0)) return 0;
    {
        std::lock_guard lock(g_rpc_mutex);
        g_native_rpc_services[client_address] = service;
    }
    if (static_cast<int32_t>(semaphore) >= 0)
        openrac_guest_kernel_syscall(0x42u, semaphore, 0, 0, 0, 0, 0, 0, 0);
    if (openrac_env_string("OPENRAC_TRACE_SIF") != nullptr)
        std::fprintf(stderr, "[info] native SIF RPC bind service=%#010x client=%#010x\n",
                     service, client);
    return 1;
}

void trace_rpc_call(uint32_t packet, uint32_t source, uint32_t destination, int data_size) {
    uint32_t client = 0;
    uint32_t rpc_number = 0;
    uint32_t send_size = 0;
    uint32_t receive_buffer = 0;
    uint32_t receive_size = 0;
    uint32_t request_word = 0;
    if (!guest_u32(packet + 28, &client) || !guest_u32(packet + 32, &rpc_number) ||
        !guest_u32(packet + 36, &send_size) || !guest_u32(packet + 40, &receive_buffer) ||
        !guest_u32(packet + 44, &receive_size)) return;
    if (data_size >= 4) guest_u32(source, &request_word);
    static unsigned packet_dump_count = 0;
    if (openrac_env_string("OPENRAC_TRACE_SIF") != nullptr && packet_dump_count < 8) {
        ++packet_dump_count;
        std::fprintf(stderr, "[info] SIF RPC packet=%#010x words:", packet);
        for (uint32_t offset = 16; offset < 64; offset += 4) {
            uint32_t word = 0;
            if (!guest_u32(packet + offset, &word)) break;
            std::fprintf(stderr, " +%02x=%#010x", offset, word);
        }
        std::fprintf(stderr, "\n");
    }
    uint32_t service = 0;
    {
        std::lock_guard lock(g_rpc_mutex);
        const auto it = g_native_rpc_services.find(openrac_guest_ram_address(client));
        if (it != g_native_rpc_services.end()) service = it->second;
    }
    openrac::log::warn(
        "native SIF RPC call has no response: service={:#010x} rpc={} send={} recvbuf={:#010x} "
        "recvsize={} source={:#010x} destination={:#010x} data_size={} word={:#010x} client={:#010x}",
        service, rpc_number, send_size, receive_buffer, receive_size, source, destination,
        data_size, request_word, client);
}
}

extern "C" int openrac_lib_sly1_sif_send_cmd(uint32_t command, uint32_t packet,
                                               int packet_size, uint32_t source,
                                               uint32_t destination, int data_size) {
    const uint32_t packet_address = openrac_guest_ram_address(packet);
    if (packet_size < 16 || packet_address > openrac_guest_ram_size ||
        static_cast<uint32_t>(packet_size) > openrac_guest_ram_size - packet_address) {
        openrac_guest_missing("SIF command packet is outside guest RAM");
        return 0;
    }
    if (command == kSifRpcBind) return complete_rpc_bind(packet);
    if (command == kSifRpcCall) {
        trace_rpc_call(packet, source, destination, data_size);
        return 0;
    }
    if (command == kSifCommandInit) {
        // The retail EE sends command 0x80000002 after SIFCMD initialization
        // and waits for the IOP completion callback at D_006294C0[0].
        const uint32_t completion_address = openrac_guest_ram_address(kInitCompletion);
        if (completion_address > openrac_guest_ram_size ||
            openrac_guest_ram_size - completion_address < sizeof(uint32_t)) return 0;
        const uint32_t complete = 1;
        std::memcpy(openrac_guest_base + completion_address, &complete, sizeof(complete));
        if (openrac_env_string("OPENRAC_TRACE_SIF") != nullptr)
            std::fprintf(stderr, "[info] native SIF command %#010x completed\n", command);
        return 1;
    }

    openrac::log::warn("native SIF command {:#010x} has no response handler", command);
    return 0;
}
