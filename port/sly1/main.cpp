// SPDX-License-Identifier: AGPL-3.0-or-later
// Native launcher for the translated Sly 1 executable.
#include "openrac/elf.h"
#include "openrac/disc_image.h"
#include "openrac/game_host.h"
#include "openrac/memory.h"
#include "openrac/native_gs.h"
#include "openrac/native_window.h"
#include "../platform/clock.h"

#include <SDL3/SDL.h>

extern "C" {
#include "game_protos.h"
}

#include <atomic>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <vector>

extern "C" {
const char* openrac_game_disc_image = nullptr;
void sly1_native_start_worker(void);
void sly1_native_start_loader(void);
}

namespace {
bool trace_guest_range(std::uint32_t address, std::size_t size,
                       std::uint32_t* offset_out);
}

extern "C" void game_sly1_native_trace_font(std::uint32_t stage, std::uint32_t a0,
                                              std::uint32_t a1, std::uint32_t a2,
                                              std::uint32_t a3, std::uint32_t a4) {
    if (std::getenv("OPENRAC_TRACE_FONT") == nullptr) return;
    static std::array<std::atomic<unsigned>, 240> traces{};
    if (stage >= traces.size()) return;
    const unsigned index = traces[stage].fetch_add(1, std::memory_order_relaxed);
    constexpr unsigned limits[] = {2, 2, 8, 16, 4, 4, 4, 2, 8, 16, 16, 8, 8, 8, 8, 8, 24, 32, 8, 4, 4, 4, 64, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 16};
    unsigned limit = stage < sizeof(limits) / sizeof(limits[0])
        ? limits[stage] : 8u;
    if ((stage >= 90u && stage <= 110u) ||
        (stage >= 111u && stage <= 125u) ||
        (stage >= 160u && stage <= 175u) ||
        stage == 176u || stage == 178u || stage == 182u || stage == 201u ||
        stage == 73 || stage == 74 || stage == 75 ||
        (stage >= 81 && stage <= 89) || stage == 58 || stage == 59 || stage == 60 ||
        stage == 61 || stage == 62 || stage == 63 || stage == 64 || stage == 57 ||
        (stage >= 136u && stage <= 139u)) {
        limit = 256u;
    } else if (stage == 65u) {
        limit = 4096u;
    } else if (stage == 134u || stage == 135u) {
        limit = 128u;
    } else if (stage == 76 || stage == 78 || stage == 79 || stage == 80 ||
               stage == 70 || stage == 71 || stage == 72) {
        limit = 24u;
    } else if (stage == 35 || (stage >= 50 && stage <= 53)) {
        limit = 64u;
    } else if (stage == 55 || stage == 56) {
        limit = 4096u;
    }
    if (index >= limit) return;
    std::cerr << "[font trace] stage=" << stage << " args=" << std::hex << a0 << ','
              << a1 << ',' << a2 << ',' << a3 << ',' << a4 << std::dec << '\n';
    if (stage == 65u) {
        std::uint32_t writer_offset = 0;
        if (trace_guest_range(a0, 0x20u, &writer_offset)) {
            std::uint32_t end = 0, begin = 0, cursor = 0, mode = 0;
            std::memcpy(&end, openrac_guest_base + writer_offset, 4u);
            std::memcpy(&begin, openrac_guest_base + writer_offset + 4u, 4u);
            std::memcpy(&cursor, openrac_guest_base + writer_offset + 0x0Cu, 4u);
            std::memcpy(&mode, openrac_guest_base + writer_offset + 0x10u, 4u);
            std::cerr << "[vif-reserve] writer=" << std::hex << a0
                      << " begin=" << begin << " cursor=" << cursor
                      << " end=" << end << " mode=" << mode << std::dec << '\n';
        }
    }
    if (stage == 190u) {
        std::uint32_t frame_offset = 0;
        if (trace_guest_range(a0, 0x80u, &frame_offset)) {
            std::uint32_t owner = 0, skin = 0, part = 0, cache = 0;
            std::memcpy(&owner, openrac_guest_base + frame_offset + 0x70u, 4u);
            std::memcpy(&skin, openrac_guest_base + frame_offset + 0x74u, 4u);
            std::memcpy(&part, openrac_guest_base + frame_offset + 0x78u, 4u);
            std::memcpy(&cache, openrac_guest_base + frame_offset + 0x7Cu, 4u);
            std::uint32_t part_offset = 0;
            if (part != 0 && trace_guest_range(part, 0x70u, &part_offset)) {
                std::uint32_t f18 = 0, f28 = 0, flags = 0, mesh_count = 0;
                std::uint32_t meshes = 0, strip_count = 0, strips = 0, light = 0;
                std::memcpy(&f18, openrac_guest_base + part_offset + 0x18u, 4u);
                std::memcpy(&f28, openrac_guest_base + part_offset + 0x28u, 4u);
                std::memcpy(&flags, openrac_guest_base + part_offset + 0x2Cu, 4u);
                std::memcpy(&mesh_count, openrac_guest_base + part_offset + 0x58u, 4u);
                std::memcpy(&meshes, openrac_guest_base + part_offset + 0x5Cu, 4u);
                std::memcpy(&strip_count, openrac_guest_base + part_offset + 0x60u, 4u);
                std::memcpy(&strips, openrac_guest_base + part_offset + 0x64u, 4u);
                std::memcpy(&light, openrac_guest_base + part_offset + 0x6Cu, 4u);
                std::cerr << "[model-part] frame=0x" << std::hex << a0
                          << " owner=0x" << owner << " skin=0x" << skin
                          << " part=0x" << part << " cache=0x" << cache
                          << " f18=0x" << f18 << " f28=0x" << f28
                          << " flags=0x" << flags << " meshes=" << std::dec
                          << mesh_count << "@0x" << std::hex << meshes
                          << " strips=" << std::dec << strip_count << "@0x"
                          << std::hex << strips << " light=0x" << light
                          << std::dec << '\n';
            }
        }
    }
    if (stage == 178u) {
        std::uint32_t object_offset = 0;
        if (trace_guest_range(a0, 0x244u, &object_offset)) {
            std::uint32_t model = 0, part_count = 0, parts = 0, states = 0;
            std::memcpy(&model, openrac_guest_base + object_offset + 0x22Cu, 4u);
            std::memcpy(&part_count, openrac_guest_base + object_offset + 0x238u, 4u);
            std::memcpy(&parts, openrac_guest_base + object_offset + 0x23Cu, 4u);
            std::memcpy(&states, openrac_guest_base + object_offset + 0x240u, 4u);
            std::cerr << "[placed-object-draw] object=0x" << std::hex << a0
                      << " camera=0x" << a1 << " model=0x" << model
                      << " part_count=" << std::dec << part_count
                      << " parts=0x" << std::hex << parts << " states=0x"
                      << states << std::dec << '\n';
        }
    }
    if (stage == 181u) {
        std::uint32_t context_offset = 0;
        if (trace_guest_range(a0, 0x64u, &context_offset)) {
            std::uint32_t object = 0;
            std::memcpy(&object, openrac_guest_base + context_offset + 0x60u, 4u);
            std::uint32_t object_offset = 0, descriptor = 0, descriptor_offset = 0;
            if (object != 0 && trace_guest_range(object, 0x240u, &object_offset)) {
                std::uint32_t type = 0xFFFFFFFFu, size = 0, id = 0;
                std::memcpy(&descriptor, openrac_guest_base + object_offset, 4u);
                if (trace_guest_range(descriptor, 0x10u, &descriptor_offset)) {
                    std::memcpy(&type, openrac_guest_base + descriptor_offset + 4u, 4u);
                    std::memcpy(&size, openrac_guest_base + descriptor_offset + 0x0Cu, 4u);
                }
                std::memcpy(&id, openrac_guest_base + object_offset + 8u, 4u);
                std::uint32_t count = 0, parts = 0;
                std::memcpy(&count, openrac_guest_base + object_offset + 0x238u, 4u);
                std::memcpy(&parts, openrac_guest_base + object_offset + 0x23Cu, 4u);
                std::cerr << "[render-object] context=0x" << std::hex << a0
                          << " object=0x" << object << " class=0x" << type
                          << " size=0x" << size << " id=0x" << id
                          << " part_count=" << std::dec << count
                          << " parts=0x" << std::hex << parts << std::dec << '\n';
            } else {
                std::cerr << "[render-object] context=0x" << std::hex << a0
                          << " object=0x" << object << " (no mapped object)\n";
            }
        }
    }
    if (stage == 182u) {
        std::uint32_t object_offset = 0;
        if (trace_guest_range(a0, 0x248u, &object_offset)) {
            std::uint32_t descriptor = 0, descriptor_offset = 0;
            std::uint32_t type = 0xFFFFFFFFu, size = 0;
            std::memcpy(&descriptor, openrac_guest_base + object_offset, 4u);
            if (trace_guest_range(descriptor, 0xACu, &descriptor_offset)) {
                std::memcpy(&type, openrac_guest_base + descriptor_offset + 4u, 4u);
                std::memcpy(&size, openrac_guest_base + descriptor_offset + 0x0Cu, 4u);
            }
            if (size >= 0x248u && size <= 0x10000u &&
                trace_guest_range(a0, size, &object_offset)) {
                std::uint32_t id = 0, parent = 0, model_header[7]{};
                std::uint32_t draw_method = 0, queue_method = 0;
                std::uint32_t frame_record = 0, range_record = 0;
                std::uint32_t list78_key = 0;
                std::uint64_t object_flags = 0;
                float position[3]{};
                std::memcpy(&id, openrac_guest_base + object_offset + 8u, 4u);
                std::memcpy(&parent, openrac_guest_base + object_offset + 0x18u, 4u);
                std::memcpy(model_header, openrac_guest_base + object_offset + 0x22Cu,
                            sizeof(model_header));
                std::memcpy(&draw_method, openrac_guest_base + descriptor_offset + 0xA4u, 4u);
                std::memcpy(&queue_method, openrac_guest_base + descriptor_offset + 0xA8u, 4u);
                std::memcpy(&frame_record, openrac_guest_base + object_offset + 0x224u, 4u);
                std::memcpy(&range_record, openrac_guest_base + object_offset + 0x284u, 4u);
                std::memcpy(&list78_key, openrac_guest_base + object_offset + 0x294u, 4u);
                std::memcpy(&object_flags, openrac_guest_base + object_offset + 0x2C8u, 8u);
                std::memcpy(position, openrac_guest_base + object_offset + 0x100u,
                            sizeof(position));
                std::cerr << "[model-owner] object=0x" << std::hex << a0
                          << " class=0x" << type << " id=0x" << id
                          << " parent=0x" << parent << " draw=0x" << draw_method
                          << " queue=0x" << queue_method << " frame=0x" << frame_record
                          << " range=0x" << range_record << " list78_key=0x" << list78_key
                          << " flags=0x" << object_flags << std::dec
                          << " pos=" << position[0] << ',' << position[1] << ','
                          << position[2] << " model_header=" << std::hex;
                for (unsigned i = 0; i < std::size(model_header); ++i)
                    std::cerr << (i == 0 ? "" : ",") << "0x" << model_header[i];
                std::cerr << std::dec << '\n';
            }
        }
    }
    if (stage == 202u) {
        std::uint32_t object_offset = 0;
        if (trace_guest_range(a0, 0x244u, &object_offset)) {
            std::uint32_t descriptor = 0, descriptor_offset = 0;
            std::uint32_t type = 0xFFFFFFFFu, size = 0, id = 0;
            std::memcpy(&descriptor, openrac_guest_base + object_offset, 4u);
            if (trace_guest_range(descriptor, 0x10u, &descriptor_offset)) {
                std::memcpy(&type, openrac_guest_base + descriptor_offset + 4u, 4u);
                std::memcpy(&size, openrac_guest_base + descriptor_offset + 0x0Cu, 4u);
            }
            std::memcpy(&id, openrac_guest_base + object_offset + 8u, 4u);
            std::uint32_t part_count = 0, parts = 0, states = 0;
            std::memcpy(&part_count, openrac_guest_base + object_offset + 0x238u, 4u);
            std::memcpy(&parts, openrac_guest_base + object_offset + 0x23Cu, 4u);
            std::memcpy(&states, openrac_guest_base + object_offset + 0x240u, 4u);
            std::cerr << "[draw-collider] object=0x" << std::hex << a0
                      << " class=0x" << type << " size=0x" << size << " id=0x" << id
                      << " part_count=" << std::dec << part_count
                      << " parts=0x" << std::hex << parts << " states=0x" << states
                      << " b=0x" << a1 << " m=0x" << a2 << std::dec << '\n';
        }
    }
    if (stage == 203u) {
        std::uint32_t frame_offset = 0;
        std::uint32_t globals_offset = 0;
        if (trace_guest_range(a0, 0x30u, &frame_offset) &&
            trace_guest_range(0x00262C68u, 8u, &globals_offset)) {
            std::int32_t state = 0, frame_count = 0, item = 0;
            std::uint32_t state_selector = 0, global_flags = 0, state_data = 0;
            std::uint16_t input_flags = 0, held = 0;
            float state_time = 0.0f, elapsed = 0.0f;
            std::memcpy(&state, openrac_guest_base + frame_offset + 4u, 4u);
            std::memcpy(&state_time, openrac_guest_base + frame_offset + 8u, 4u);
            std::memcpy(&frame_count, openrac_guest_base + frame_offset + 0x2Cu, 4u);
            std::memcpy(&state_selector, openrac_guest_base + globals_offset, 4u);
            std::memcpy(&global_flags, openrac_guest_base + globals_offset + 4u, 4u);
            state_data = (state_selector & 4u) != 0 ? 0x00262C70u : 0x00262D38u;
            std::uint32_t data_offset = 0;
            if (trace_guest_range(state_data, 0xACu, &data_offset)) {
                std::memcpy(&input_flags, openrac_guest_base + data_offset + 0xAAu, 2u);
                std::memcpy(&held, openrac_guest_base + data_offset + 0xA8u, 2u);
                std::int32_t code = 0;
                std::memcpy(&code, openrac_guest_base + data_offset + 0x14u, 4u);
                if (frame_count >= 0 && frame_count < 8) {
                    std::memcpy(&item, openrac_guest_base + frame_offset + 8u +
                                static_cast<std::uint32_t>(frame_count) * 4u, 4u);
                }
                std::uint32_t clock_offset = 0;
                if (trace_guest_range(0x00261868u, 4u, &clock_offset)) {
                    float now = 0.0f;
                    std::memcpy(&now, openrac_guest_base + clock_offset, 4u);
                    elapsed = now - state_time;
                }
                std::cerr << "[element-state] frame=0x" << std::hex << a0
                          << " state=" << std::dec << state << " elapsed=" << elapsed
                          << " code=" << code << " data=0x" << std::hex << state_data
                          << " selector=0x" << state_selector << " global_flags=0x"
                          << global_flags << " input_flags=0x"
                          << input_flags << " held=0x" << held << " pressed=0x"
                          << input_flags << " item=" << std::dec << frame_count << ":0x"
                          << std::hex << static_cast<std::uint32_t>(item) << std::dec << '\n';
            }
        }
    }
    if (stage == 64u && (a0 & 0xFFFFFF00u) == 0xBAC30000u &&
        (a0 & 0xFFu) == 9u) {
        std::uint32_t cursor_offset = 0;
        if (trace_guest_range(a1, 32u, &cursor_offset)) {
            std::cerr << "[collider-model-input] cursor=" << std::hex << a1
                      << " remaining=" << std::dec << a2 << " bytes=";
            for (unsigned i = 0; i < 32u; ++i)
                std::cerr << std::hex << std::setw(2) << std::setfill('0')
                          << static_cast<unsigned>(openrac_guest_base[cursor_offset + i]);
            std::cerr << std::dec << std::setfill(' ') << '\n';
        }
    }
    if (stage == 42u) {
        const std::uint32_t begin = a2 >= 16u ? a2 - 16u : a2;
        std::uint32_t offset = 0;
        if (trace_guest_range(begin, 48u, &offset)) {
            std::cerr << "[object-record-window] cursor=" << std::hex << a2
                      << " bytes=";
            for (unsigned i = 0; i < 48u; ++i)
                std::cerr << std::setw(2) << std::setfill('0')
                          << static_cast<unsigned>(openrac_guest_base[offset + i]);
            std::cerr << std::dec << std::setfill(' ') << '\n';
        }
    }
    if (stage == 55u) {
        std::uint32_t list_offset = 0;
        if (trace_guest_range(a0, 12u, &list_offset)) {
            std::uint32_t head = 0, tail = 0;
            std::int32_t link_offset = 0;
            std::memcpy(&head, openrac_guest_base + list_offset, 4u);
            std::memcpy(&tail, openrac_guest_base + list_offset + 4u, 4u);
            std::memcpy(&link_offset, openrac_guest_base + list_offset + 8u, 4u);
            std::uint32_t next = 0, previous = 0, link_address = 0;
            if (link_offset >= 0 && link_offset <= 0x10000 &&
                trace_guest_range(a1 + static_cast<std::uint32_t>(link_offset),
                                  8u, &link_address)) {
                std::memcpy(&next, openrac_guest_base + link_address, 4u);
                std::memcpy(&previous, openrac_guest_base + link_address + 4u, 4u);
            }
            std::cerr << "[list-remove] list=" << std::hex << a0 << " node=" << a1
                      << " head=" << head << " tail=" << tail << " offset="
                      << link_offset << " next=" << next << " previous=" << previous
                      << std::dec << '\n';
        }
    }
    if (stage == 56u && a0 == 0x002622E0u) {
        std::uint32_t link_address = 0, next = 0, previous = 0;
        if (a2 <= 0x10000u &&
            trace_guest_range(a1 + a2, 8u, &link_address)) {
            std::memcpy(&next, openrac_guest_base + link_address, 4u);
            std::memcpy(&previous, openrac_guest_base + link_address + 4u, 4u);
        }
        std::cerr << "[frame-list-insert] node=" << std::hex << a1
                  << " head=" << a3 << " tail=" << a4 << " offset=" << a2
                  << " next=" << next << " previous=" << previous << std::dec
                  << '\n';
    }
    if (stage == 75u && a4 == 0x00A7F77Cu && a0 >= 0xEFu) {
        std::uint32_t offset = 0;
        if (trace_guest_range(a3, 64u, &offset)) {
            std::cerr << "[animation-stream] index=" << std::hex << a0
                      << " cursor=" << a3 << " bytes=";
            for (unsigned i = 0; i < 64u; ++i)
                std::cerr << std::setw(2) << std::setfill('0')
                          << static_cast<unsigned>(openrac_guest_base[offset + i]);
            std::cerr << std::dec << std::setfill(' ') << '\n';
        }
    }
    if ((stage == 60 || stage == 61) && a2 != 0) {
        std::uint32_t cursor_offset = 0;
        if (trace_guest_range(a2 + 0x18u, 4u, &cursor_offset)) {
            std::uint32_t cursor = 0;
            std::memcpy(&cursor, openrac_guest_base + cursor_offset, 4u);
            std::cerr << "[resource-descriptor] stage=" << stage << " object="
                      << std::hex << a0 << " descriptor=" << a1 << " reader=" << a2
                      << " type=" << a3 << " flags=" << a4 << " cursor=" << cursor
                      << std::dec << '\n';
        }
    }
    if (stage == 57 && a0 == 0x4Du &&
        std::getenv("OPENRAC_TRACE_OBJECTS") != nullptr) {
        static std::atomic<unsigned> class4d_snapshots{};
        if (class4d_snapshots.fetch_add(1, std::memory_order_relaxed) < 2u) {
            std::uint32_t object_offset = 0;
            std::uint32_t descriptor_offset = 0;
            if (trace_guest_range(a1, 4u, &object_offset)) {
                std::uint32_t descriptor = 0;
                std::memcpy(&descriptor, openrac_guest_base + object_offset, 4u);
                if (trace_guest_range(descriptor, 0x10u, &descriptor_offset)) {
                    std::uint32_t object_size = 0;
                    std::memcpy(&object_size,
                                openrac_guest_base + descriptor_offset + 0x0Cu, 4u);
                    if (object_size <= 0x1000u &&
                        trace_guest_range(a1, object_size, &object_offset)) {
                        std::cerr << "[factory-object] class=4d object=" << std::hex
                                  << a1 << " descriptor=" << descriptor << " size="
                                  << object_size << '\n';
                        for (std::uint32_t offset = 0; offset + 4u <= object_size;
                             offset += 4u) {
                            std::uint32_t value = 0;
                            std::memcpy(&value,
                                        openrac_guest_base + object_offset + offset, 4u);
                            if (value != 0)
                                std::cerr << "  +" << std::setw(3) << std::setfill('0')
                                          << std::hex << offset << '=' << std::setw(8)
                                          << value << '\n';
                        }
                        std::cerr << std::dec << std::setfill(' ');
                    }
                }
            }
        }
    }
    if (stage == 57 && a0 == 1u &&
        std::getenv("OPENRAC_TRACE_OBJECTS") != nullptr) {
        static std::atomic<unsigned> class1_snapshots{};
        if (class1_snapshots.fetch_add(1, std::memory_order_relaxed) < 4u) {
            std::uint32_t object_offset = 0;
            std::uint32_t descriptor_offset = 0;
            if (trace_guest_range(a1, 4u, &object_offset)) {
                std::uint32_t descriptor = 0;
                std::memcpy(&descriptor, openrac_guest_base + object_offset, 4u);
                if (trace_guest_range(descriptor, 0x54u, &descriptor_offset)) {
                    std::uint32_t size = 0, constructor = 0, reader = 0;
                    std::uint32_t method7 = 0, method19 = 0, method20 = 0;
                    std::memcpy(&size, openrac_guest_base + descriptor_offset + 0x0Cu, 4u);
                    std::memcpy(&constructor, openrac_guest_base + descriptor_offset + 0x10u, 4u);
                    std::memcpy(&reader, openrac_guest_base + descriptor_offset + 0x38u, 4u);
                    std::memcpy(&method7, openrac_guest_base + descriptor_offset + 0x1Cu, 4u);
                    std::memcpy(&method19, openrac_guest_base + descriptor_offset + 0x4Cu, 4u);
                    std::memcpy(&method20, openrac_guest_base + descriptor_offset + 0x50u, 4u);
                    std::cerr << "[factory-object] class=1 object=" << std::hex << a1
                              << " descriptor=" << descriptor << " size=" << size
                              << " constructor=" << constructor << " reader=" << reader
                              << " method7=" << method7 << " method19=" << method19
                              << " method20=" << method20
                              << " owner=" << a2 << " mode=" << a4 << std::dec << '\n';
                }
            }
        }
    }
    if (stage == 23 && a2 <= 0x10000u &&
        std::getenv("OPENRAC_DUMP_LEVEL_WINDOW") != nullptr) {
        std::uint32_t offset = 0;
        if (trace_guest_range(a1, 0x100u, &offset)) {
            std::ofstream dump("/tmp/sly1-level-reader-window.bin",
                               std::ios::binary);
            if (dump) dump.write(
                reinterpret_cast<const char*>(openrac_guest_base + offset), 0x100);
        }
        if (trace_guest_range(GUEST_SCRATCHPAD, 0x2000u, &offset)) {
            std::ofstream dump("/tmp/sly1-level-reader-ring.bin",
                               std::ios::binary);
            if (dump) dump.write(
                reinterpret_cast<const char*>(openrac_guest_base + offset), 0x2000);
        }
    }
    if (stage == 7 && std::getenv("OPENRAC_DUMP_FONT_RESOURCE") != nullptr &&
        a0 <= 0xffffffffu - a1) {
        std::ofstream dump("/tmp/sly1-font-resource.bin", std::ios::binary);
        if (dump) dump.write(reinterpret_cast<const char*>(openrac_guest_base + a0), a1);
    }
    if (stage == 8 && std::getenv("OPENRAC_DUMP_FONT_PACKETS") != nullptr &&
        a1 <= a2 && a2 - a1 <= 0x10000u) {
        static std::atomic<unsigned> packet_index{};
        const unsigned packet = packet_index.fetch_add(1, std::memory_order_relaxed);
        const std::string path = "/tmp/sly1-font-packet-" +
                                 std::to_string(packet) + ".bin";
        std::ofstream dump(path, std::ios::binary);
        if (dump) dump.write(reinterpret_cast<const char*>(openrac_guest_base + a1),
                             a2 - a1);
    }
}
extern "C" void game_sly1_native_trace_resource_range(std::uint32_t sector,
                                                         std::uint32_t byte_count) {
    if (std::getenv("OPENRAC_STARTUP_TRACE") == nullptr) return;
    std::cerr << "[resource-range] sector=" << sector << " bytes=" << byte_count
              << '\n';
}

extern "C" std::uint32_t game_sly1_native_boot_level_key() {
    static const std::uint32_t key = [] {
        const char* value = std::getenv("OPENRAC_BOOT_LEVEL");
        if (value == nullptr || *value == '\0') return 0u;
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(value, &end, 0);
        if (end == value || *end != '\0' || parsed > 0xFFFFFFFFul) return 0u;
        return static_cast<std::uint32_t>(parsed);
    }();
    return key;
}

namespace {

bool trace_guest_range(std::uint32_t address, std::size_t size,
                       std::uint32_t* offset_out) {
    std::uint64_t offset = address;
    std::uint64_t limit = openrac_guest_ram_size;
    if (address >= GUEST_SCRATCHPAD &&
        static_cast<std::uint64_t>(address) + size <=
            static_cast<std::uint64_t>(GUEST_SCRATCHPAD) +
                GUEST_SCRATCHPAD_SIZE) {
        limit = static_cast<std::uint64_t>(GUEST_SCRATCHPAD) +
                GUEST_SCRATCHPAD_SIZE;
    } else if (address >= GUEST_PORT &&
        static_cast<std::uint64_t>(address) + size <=
            static_cast<std::uint64_t>(GUEST_PORT) + GUEST_PORT_SIZE) {
        limit = static_cast<std::uint64_t>(GUEST_PORT) + GUEST_PORT_SIZE;
    } else {
        offset = openrac_guest_ram_address(address);
    }
    if (offset > limit || size > limit - offset) return false;
    *offset_out = static_cast<std::uint32_t>(offset);
    return true;
}

std::uint32_t trace_world_list_count(std::uint32_t world, std::uint32_t list_offset) {
    std::uint32_t list_address = 0;
    if (!trace_guest_range(world + list_offset, 12u, &list_address)) return 0;
    std::uint32_t node = 0;
    std::int32_t link_offset = 0;
    std::memcpy(&node, openrac_guest_base + list_address, sizeof(node));
    std::memcpy(&link_offset, openrac_guest_base + list_address + 8u,
                sizeof(link_offset));
    if (link_offset <= 0 || link_offset > 0x10000) return 0;

    std::uint32_t count = 0;
    while (node != 0 && count < 0x1000u) {
        std::uint32_t link_address = 0;
        if (!trace_guest_range(node + static_cast<std::uint32_t>(link_offset),
                               sizeof(std::uint32_t), &link_address)) break;
        std::memcpy(&node, openrac_guest_base + link_address, sizeof(node));
        ++count;
    }
    return count;
}

std::array<std::uint32_t, 32> trace_world_list_classes(std::uint32_t world,
                                                       std::uint32_t list_offset) {
    std::array<std::uint32_t, 32> classes{};
    std::uint32_t list_address = 0;
    if (!trace_guest_range(world + list_offset, 12u, &list_address)) return classes;
    std::uint32_t node = 0;
    std::int32_t link_offset = 0;
    std::memcpy(&node, openrac_guest_base + list_address, sizeof(node));
    std::memcpy(&link_offset, openrac_guest_base + list_address + 8u,
                sizeof(link_offset));
    if (link_offset <= 0 || link_offset > 0x10000) return classes;

    for (std::uint32_t i = 0; i < classes.size() && node != 0; ++i) {
        std::uint32_t object_address = 0;
        if (!trace_guest_range(node, 0x34u, &object_address)) break;
        std::uint32_t descriptor = 0;
        std::memcpy(&descriptor, openrac_guest_base + object_address, sizeof(descriptor));
        std::uint32_t descriptor_address = 0;
        if (!trace_guest_range(descriptor, 8u, &descriptor_address)) break;
        std::memcpy(&classes[i], openrac_guest_base + descriptor_address + 4u,
                    sizeof(classes[i]));
        std::uint32_t link_address = 0;
        if (!trace_guest_range(node + static_cast<std::uint32_t>(link_offset),
                               sizeof(node), &link_address)) break;
        std::memcpy(&node, openrac_guest_base + link_address, sizeof(node));
    }
    return classes;
}

std::array<std::uint32_t, 9> trace_first_world_object_fields(
    std::uint32_t world, std::uint32_t list_offset) {
    std::array<std::uint32_t, 9> fields{};
    std::uint32_t list_address = 0;
    if (!trace_guest_range(world + list_offset, 12u, &list_address)) return fields;
    std::uint32_t node = 0;
    std::int32_t link_offset = 0;
    std::memcpy(&node, openrac_guest_base + list_address, sizeof(node));
    std::memcpy(&link_offset, openrac_guest_base + list_address + 8u,
                sizeof(link_offset));
    if (node == 0 || link_offset <= 0 || link_offset > 0x10000) return fields;
    std::uint32_t object_address = 0;
    if (!trace_guest_range(node, 0x374u, &object_address)) return fields;
    fields[0] = node;
    constexpr std::uint32_t offsets[] = {0x2C8u, 0x2CCu, 0x2D0u, 0x304u,
                                         0x308u, 0x28Cu, 0x298u, 0x370u};
    for (std::size_t i = 0; i < std::size(offsets); ++i)
        std::memcpy(&fields[i + 1u],
                    openrac_guest_base + object_address + offsets[i],
                    sizeof(fields[i]));
    return fields;
}

std::array<std::array<std::uint32_t, 7>, 12> trace_world_objects(std::uint32_t world) {
    std::array<std::array<std::uint32_t, 7>, 12> objects{};
    std::uint32_t list_address = 0;
    if (!trace_guest_range(world + 0x48u, 12u, &list_address)) return objects;

    std::uint32_t node = 0;
    std::int32_t link_offset = 0;
    std::memcpy(&node, openrac_guest_base + list_address, sizeof(node));
    std::memcpy(&link_offset, openrac_guest_base + list_address + 8u,
                sizeof(link_offset));
    if (link_offset <= 0 || link_offset > 0x10000) return objects;

    for (auto& fields : objects) {
        std::uint32_t object_address = 0;
        if (node == 0 || !trace_guest_range(node, 0x1Cu, &object_address)) break;
        fields[0] = node;
        std::memcpy(&fields[1], openrac_guest_base + object_address + 8u, 4u);
        std::memcpy(&fields[2], openrac_guest_base + object_address + 0x14u, 4u);
        std::memcpy(&fields[3], openrac_guest_base + object_address + 0x18u, 4u);

        std::uint32_t descriptor = 0;
        std::memcpy(&descriptor, openrac_guest_base + object_address, 4u);
        std::uint32_t descriptor_address = 0;
        if (trace_guest_range(descriptor, 0x24u, &descriptor_address)) {
            std::memcpy(&fields[4], openrac_guest_base + descriptor_address + 4u, 4u);
            std::memcpy(&fields[5], openrac_guest_base + descriptor_address + 0x20u, 4u);
        }

        std::uint32_t children_address = 0;
        if (trace_guest_range(node + 0x34u, 12u, &children_address)) {
            std::uint32_t child = 0;
            std::int32_t child_offset = 0;
            std::memcpy(&child, openrac_guest_base + children_address, 4u);
            std::memcpy(&child_offset, openrac_guest_base + children_address + 8u, 4u);
            while (child != 0 && child_offset > 0 && child_offset <= 0x10000 &&
                   fields[6] < 0x1000u) {
                std::uint32_t link_address = 0;
                if (!trace_guest_range(child + static_cast<std::uint32_t>(child_offset),
                                       4u, &link_address)) break;
                std::memcpy(&child, openrac_guest_base + link_address, 4u);
                ++fields[6];
            }
        }

        std::uint32_t link_address = 0;
        if (!trace_guest_range(node + static_cast<std::uint32_t>(link_offset), 4u,
                               &link_address)) break;
        std::memcpy(&node, openrac_guest_base + link_address, sizeof(node));
    }
    return objects;
}

void trace_world_class_objects(std::uint32_t world, std::uint32_t class_id) {
    if (std::getenv("OPENRAC_TRACE_OBJECTS") == nullptr || world == 0) return;
    static std::array<std::uint32_t, 0x95> last_traced_world{};
    const bool trace_each_snapshot = class_id == 0x57u || class_id == 0x88u;
    if (class_id >= last_traced_world.size() ||
        (!trace_each_snapshot && last_traced_world[class_id] == world)) return;
    last_traced_world[class_id] = world;

    const std::uint32_t slot = world + 0x1884u + class_id * 4u;
    std::uint32_t slot_address = 0;
    if (!trace_guest_range(slot, 4u, &slot_address)) return;
    std::uint32_t object = 0;
    std::memcpy(&object, openrac_guest_base + slot_address, 4u);
    if (object == 0) return;
    std::cerr << "\n[owner-class-object] world=0x" << std::hex << world
              << " class=0x" << class_id << " head=0x" << object << std::dec
              << '\n';

    for (unsigned index = 0; object != 0 && index < 128u; ++index) {
        std::uint32_t object_address = 0;
        if (!trace_guest_range(object, 4u, &object_address)) {
            std::cerr << "[owner-class-object] invalid object=0x" << std::hex
                      << object << std::dec << '\n';
            break;
        }
        std::uint32_t descriptor = 0, next = 0;
        std::memcpy(&descriptor, openrac_guest_base + object_address, 4u);
        std::uint32_t descriptor_address = 0;
        std::uint32_t type = 0xFFFFFFFFu;
        std::uint32_t object_size = 0;
        if (!trace_guest_range(descriptor, 0x10u, &descriptor_address)) break;
        std::memcpy(&type, openrac_guest_base + descriptor_address + 4u, 4u);
        std::memcpy(&object_size, openrac_guest_base + descriptor_address + 0x0Cu, 4u);
        if (!trace_guest_range(object, 0x34u, &object_address)) break;
        std::uint32_t object_id = 0, parent = 0, next_by_id = 0, descriptor_flags = 0;
        std::memcpy(&object_id, openrac_guest_base + object_address + 8u, 4u);
        std::memcpy(&parent, openrac_guest_base + object_address + 0x18u, 4u);
        std::memcpy(&next_by_id, openrac_guest_base + object_address + 0x24u, 4u);
        std::memcpy(&descriptor_flags, openrac_guest_base + descriptor_address + 8u, 4u);
        if (object_size < 0x39Cu || object_size > 0x10000u ||
            !trace_guest_range(object, object_size, &object_address)) {
            if (type == 0x88u && object_size >= 0x54u) {
                std::int32_t states = 0, edges = 0, armed = 0;
                std::uint32_t state_table = 0, edge_table = 0, walker = 0;
                std::memcpy(&states, openrac_guest_base + object_address + 0x34u, 4u);
                std::memcpy(&state_table, openrac_guest_base + object_address + 0x38u, 4u);
                std::memcpy(&edges, openrac_guest_base + object_address + 0x3Cu, 4u);
                std::memcpy(&edge_table, openrac_guest_base + object_address + 0x40u, 4u);
                std::memcpy(&armed, openrac_guest_base + object_address + 0x44u, 4u);
                std::memcpy(&walker, openrac_guest_base + object_address + 0x48u, 4u);
                std::cerr << "[state-graph-trace] graph=0x" << std::hex << object
                          << " states=" << std::dec << states << "@0x" << std::hex
                          << state_table << " edges=" << std::dec << edges << "@0x"
                          << std::hex << edge_table << " armed=" << std::dec << armed
                          << " walker_head=0x" << std::hex << walker;
                if (walker != 0) {
                    std::uint32_t wa = 0, owner = 0, player = 0, edge = 0;
                    std::int32_t driven = 0, target = 0, current = 0;
                    if (trace_guest_range(walker, 0x40u, &wa)) {
                        std::memcpy(&owner, openrac_guest_base + wa + 0x18u, 4u);
                        std::memcpy(&driven, openrac_guest_base + wa + 0x1Cu, 4u);
                        std::memcpy(&target, openrac_guest_base + wa + 0x20u, 4u);
                        std::memcpy(&player, openrac_guest_base + wa + 0x24u, 4u);
                        std::memcpy(&current, openrac_guest_base + wa + 0x30u, 4u);
                        std::memcpy(&edge, openrac_guest_base + wa + 0x34u, 4u);
                        std::cerr << " walker_owner=0x" << owner << " driven=0x" << driven
                                  << " target=0x" << target << " player=0x" << player
                                  << std::dec << " current=" << current << " edge=0x"
                                  << std::hex << edge;
                        std::uint32_t state_address = 0, edge_address = 0;
                        if (current >= 0 && current < states &&
                            trace_guest_range(state_table + static_cast<std::uint32_t>(current) * 8u,
                                              8u, &state_address)) {
                            std::int32_t key = 0, next_key = 0;
                            std::memcpy(&key, openrac_guest_base + state_address, 4u);
                            std::memcpy(&next_key, openrac_guest_base + state_address + 4u, 4u);
                            std::cerr << " state_key=" << std::dec << key
                                      << " state_next=" << next_key;
                        }
                        if (trace_guest_range(edge, 0x14u, &edge_address)) {
                            std::uint32_t animation = 0, flags = 0;
                            std::int32_t from = 0, to = 0;
                            float weight = 0.0f;
                            std::memcpy(&animation, openrac_guest_base + edge_address, 4u);
                            std::memcpy(&from, openrac_guest_base + edge_address + 4u, 4u);
                            std::memcpy(&to, openrac_guest_base + edge_address + 8u, 4u);
                            std::memcpy(&flags, openrac_guest_base + edge_address + 0xCu, 4u);
                            std::memcpy(&weight, openrac_guest_base + edge_address + 0x10u, 4u);
                            std::cerr << " edge_anim=0x" << std::hex << animation << std::dec
                                      << " edge_states=" << from << ',' << to << " edge_flags=0x"
                                      << std::hex << flags << std::dec << " edge_weight=" << weight;
                        }
                        std::uint32_t player_address = 0, animation = 0;
                        float player_time = 0.0f, player_speed = 0.0f;
                        if (player != 0 && trace_guest_range(player, 0xECu, &player_address)) {
                            std::memcpy(&animation, openrac_guest_base + player_address + 8u, 4u);
                            std::memcpy(&player_time, openrac_guest_base + player_address + 0x14u, 4u);
                            std::memcpy(&player_speed, openrac_guest_base + player_address + 0x18u, 4u);
                            std::cerr << " player_anim=0x" << std::hex << animation
                                      << std::dec << " player_time=" << player_time
                                      << " player_speed=" << player_speed;
                        }
                    }
                }
                std::cerr << std::dec << '\n';
            } else if (type == 0x7Du && object_size >= 0x330u) {
                std::int32_t count = 0;
                std::uint32_t items = 0, post_children = 0;
                std::memcpy(&post_children, openrac_guest_base + object_address + 0x2D4u, 4u);
                std::memcpy(&count, openrac_guest_base + object_address + 0x310u, 4u);
                std::memcpy(&items, openrac_guest_base + object_address + 0x314u, 4u);
                std::cerr << "[object-subgroup-trace] object=0x" << std::hex << object
                          << std::dec << " children=" << count << "@0x" << std::hex << items
                          << " post_children=0x" << post_children << std::dec << '\n';
                if (count > 0 && count <= 256 &&
                    trace_guest_range(items, static_cast<std::size_t>(count) * 4u,
                                      &object_address)) {
                    const auto* child_words = reinterpret_cast<const std::uint32_t*>(
                        openrac_guest_base + object_address);
                    for (std::int32_t child_index = 0; child_index < count; ++child_index) {
                        const std::uint32_t child = child_words[child_index];
                        std::uint32_t child_address = 0, child_desc = 0, child_desc_address = 0;
                        std::uint32_t child_id = 0, child_type = 0xFFFFFFFFu;
                        if (!trace_guest_range(child, 0x1Cu, &child_address)) break;
                        std::memcpy(&child_desc, openrac_guest_base + child_address, 4u);
                        std::memcpy(&child_id, openrac_guest_base + child_address + 8u, 4u);
                        if (trace_guest_range(child_desc, 8u, &child_desc_address))
                            std::memcpy(&child_type, openrac_guest_base + child_desc_address + 4u, 4u);
                        std::cerr << "[object-subgroup-child] i=" << child_index << " object=0x"
                                  << std::hex << child << " type=0x" << child_type
                                  << " id=0x" << child_id << std::dec << '\n';
                    }
                }
            }
            std::cerr << "[owner-class-object] i=" << index << " object=0x"
                      << std::hex << object << " type=0x" << type
                      << " size=0x" << object_size << " id=0x" << object_id
                      << " parent=0x" << parent << " next_by_id=0x" << next_by_id
                      << " desc_flags=0x" << descriptor_flags << " (no mesh header)\n";
            break;
        }
        std::uint32_t radius_bits = 0;
        std::uint32_t vertex_count = 0, vertices = 0, face_count = 0, faces = 0;
        std::uint32_t link_capacity = 0, links = 0;
        std::memcpy(&radius_bits, openrac_guest_base + object_address + 0x380u, 4u);
        std::memcpy(&vertex_count, openrac_guest_base + object_address + 0x384u, 4u);
        std::memcpy(&vertices, openrac_guest_base + object_address + 0x388u, 4u);
        std::memcpy(&face_count, openrac_guest_base + object_address + 0x38Cu, 4u);
        std::memcpy(&faces, openrac_guest_base + object_address + 0x390u, 4u);
        std::memcpy(&link_capacity, openrac_guest_base + object_address + 0x394u, 4u);
        std::memcpy(&links, openrac_guest_base + object_address + 0x398u, 4u);
        std::memcpy(&next, openrac_guest_base + object_address + 0x24u, 4u);
        std::cerr << "[owner-class-object] i=" << std::dec << index
                  << " object=0x" << std::hex << object << " type=0x" << type
                  << " size=0x" << object_size << " id=0x" << object_id
                  << " parent=0x" << parent << " next_by_id=0x" << next_by_id
                  << " desc_flags=0x" << descriptor_flags << " radius_bits=0x" << radius_bits
                  << " vertices=" << std::dec << vertex_count << "@0x" << std::hex
                  << vertices << " faces=" << std::dec << face_count << "@0x"
                  << std::hex << faces << " link_capacity=" << std::dec << link_capacity
                  << "@0x" << std::hex << links;
        if ((type == 0x4Du || type == 0x51u) && object_size >= 0x244u) {
            std::uint32_t model_header[7]{};
            std::memcpy(model_header, openrac_guest_base + object_address + 0x22Cu,
                        sizeof(model_header));
            std::cerr << " model_header=" << std::hex << model_header[0] << ','
                      << model_header[1] << ',' << model_header[2] << ','
                      << model_header[3] << ',' << model_header[4] << ','
                      << model_header[5] << ',' << model_header[6];
        }
        std::uint32_t vertex_address = 0;
        if (vertex_count != 0 && vertex_count <= 0x100000u &&
            trace_guest_range(vertices, 16u, &vertex_address)) {
            float sample[4]{};
            std::memcpy(sample, openrac_guest_base + vertex_address, sizeof(sample));
            std::cerr << " first_vertex=" << std::dec << sample[0] << ',' << sample[1]
                      << ',' << sample[2] << ',' << sample[3];
        }
        if (index == 0 && trace_guest_range(descriptor, 0x124u, &descriptor_address)) {
            std::uint32_t method_d8 = 0, method_120 = 0, object_class = 0;
            std::uint32_t owner = 0, state_low = 0, state_high = 0;
            std::memcpy(&method_d8, openrac_guest_base + descriptor_address + 0xD8u, 4u);
            std::memcpy(&method_120, openrac_guest_base + descriptor_address + 0x120u, 4u);
            std::memcpy(&object_class, openrac_guest_base + object_address + 0x18u, 4u);
            std::memcpy(&owner, openrac_guest_base + object_address + 0x14u, 4u);
            std::memcpy(&state_low, openrac_guest_base + object_address + 0x2C8u, 4u);
            std::memcpy(&state_high, openrac_guest_base + object_address + 0x2CCu, 4u);
            std::cerr << " methods=" << std::hex << method_d8 << ',' << method_120
                      << " class=" << object_class << " owner=" << owner
                      << " state=" << state_high << ':' << state_low;
        }
        std::cerr << '\n';
        object = next;
    }
}

void trace_world_object_groups(std::uint32_t world) {
    if (std::getenv("OPENRAC_TRACE_OBJECTS") == nullptr || world == 0) return;

    std::uint32_t slot_address = 0;
    if (!trace_guest_range(world + 0x1884u + 0x53u * 4u, 4u, &slot_address)) return;
    std::uint32_t group = 0;
    std::memcpy(&group, openrac_guest_base + slot_address, 4u);
    for (unsigned group_index = 0; group != 0 && group_index < 16u; ++group_index) {
        std::uint32_t group_address = 0;
        if (!trace_guest_range(group, 0x2E0u, &group_address)) break;
        std::uint32_t descriptor = 0, next_group = 0;
        std::memcpy(&descriptor, openrac_guest_base + group_address, 4u);
        std::uint32_t descriptor_address = 0;
        if (!trace_guest_range(descriptor, 0x10u, &descriptor_address)) break;
        std::uint32_t class_id = 0;
        std::memcpy(&class_id, openrac_guest_base + descriptor_address + 4u, 4u);
        if (class_id != 0x53u) break;

        std::uint32_t cell = 0;
        std::int32_t link_offset = 0;
        std::memcpy(&cell, openrac_guest_base + group_address + 0x2D0u, 4u);
        std::memcpy(&link_offset, openrac_guest_base + group_address + 0x2D8u, 4u);
        std::memcpy(&next_group, openrac_guest_base + group_address + 0x24u, 4u);
        std::cerr << "\n[object-group] world=0x" << std::hex << world
                  << " group=0x" << group << " id=0x";
        std::uint32_t group_id = 0;
        std::memcpy(&group_id, openrac_guest_base + group_address + 8u, 4u);
        std::cerr << group_id << " cells=0x" << cell << " link=" << std::dec
                  << link_offset;

        for (unsigned member_index = 0; cell != 0 && member_index < 32u;
             ++member_index) {
            std::uint32_t cell_address = 0;
            if (!trace_guest_range(cell, 0x10u, &cell_address) ||
                link_offset < 0 || link_offset > 0x10000) break;
            std::uint32_t member = 0, member_id = 0, next_cell = 0;
            std::memcpy(&member, openrac_guest_base + cell_address, 4u);
            std::memcpy(&member_id, openrac_guest_base + cell_address + 4u, 4u);
            if (trace_guest_range(cell + static_cast<std::uint32_t>(link_offset),
                                  4u, &slot_address))
                std::memcpy(&next_cell, openrac_guest_base + slot_address, 4u);
            std::uint32_t member_class = 0xFFFFFFFFu, member_parent = 0;
            std::uint32_t member_address = 0;
            std::uint32_t notifier_state = 0, notifier_initial_mode = 0;
            std::uint32_t notifier_handle = 0, notifier_owner = 0;
            std::uint32_t notifier_graph = 0, notifier_saved_event = 0;
            std::uint32_t notifier_aux = 0;
            std::uint32_t notifier_gate_ref = 0, notifier_start = 0;
            float notifier_min = 0.0f, notifier_max = 0.0f, notifier_left = 0.0f;
            float notifier_state_time = 0.0f, notifier_event_time = 0.0f;
            bool notifier_in_root_list = false;
            if (member != 0 && trace_guest_range(member, 0x34u, &member_address)) {
                std::uint32_t member_descriptor = 0, member_desc_address = 0;
                std::memcpy(&member_descriptor, openrac_guest_base + member_address, 4u);
                std::memcpy(&member_parent, openrac_guest_base + member_address + 0x18u, 4u);
                if (trace_guest_range(member_descriptor, 8u, &member_desc_address))
                    std::memcpy(&member_class,
                                openrac_guest_base + member_desc_address + 4u, 4u);
                if (member_class == 0x57u &&
                    trace_guest_range(member, 0x39Cu, &member_address)) {
                    std::uint32_t list_address = 0;
                    if (trace_guest_range(world + 0x48u, 12u, &list_address)) {
                        std::uint32_t node = 0, link_offset = 0;
                        std::memcpy(&node, openrac_guest_base + list_address, 4u);
                        std::memcpy(&link_offset, openrac_guest_base + list_address + 8u, 4u);
                        for (unsigned i = 0; node != 0 && i < 256u; ++i) {
                            if (node == member) {
                                notifier_in_root_list = true;
                                break;
                            }
                            std::uint32_t node_address = 0, next_address = 0;
                            if (!trace_guest_range(node, 0x24u, &node_address) ||
                                link_offset > 0x10000u ||
                                !trace_guest_range(node + link_offset, 4u, &next_address))
                                break;
                            std::memcpy(&node, openrac_guest_base + next_address, 4u);
                        }
                    }
                    std::memcpy(&notifier_state, openrac_guest_base + member_address + 0x2D0u, 4u);
                    std::memcpy(&notifier_state_time, openrac_guest_base + member_address + 0x2D4u, 4u);
                    std::memcpy(&notifier_initial_mode, openrac_guest_base + member_address + 0x2D8u, 4u);
                    std::memcpy(&notifier_saved_event, openrac_guest_base + member_address + 0x2DCu, 4u);
                    std::memcpy(&notifier_event_time, openrac_guest_base + member_address + 0x2E0u, 4u);
                    std::memcpy(&notifier_graph, openrac_guest_base + member_address + 0x2E4u, 4u);
                    std::memcpy(&notifier_handle, openrac_guest_base + member_address + 0x2E8u, 4u);
                    std::memcpy(&notifier_owner, openrac_guest_base + member_address + 0x2ECu, 4u);
                    std::memcpy(&notifier_aux, openrac_guest_base + member_address + 0x334u, 4u);
                    std::memcpy(&notifier_start, openrac_guest_base + member_address + 0x00u, 4u);
                    std::uint32_t descriptor_address = 0;
                    if (trace_guest_range(notifier_start, 0x54u, &descriptor_address))
                        std::memcpy(&notifier_start,
                                    openrac_guest_base + descriptor_address + 0x50u, 4u);
                    std::memcpy(&notifier_gate_ref, openrac_guest_base + member_address + 0x338u, 4u);
                    std::memcpy(&notifier_min, openrac_guest_base + member_address + 0x390u, 4u);
                    std::memcpy(&notifier_max, openrac_guest_base + member_address + 0x394u, 4u);
                    std::memcpy(&notifier_left, openrac_guest_base + member_address + 0x398u, 4u);
                }
            }
            std::cerr << " [" << std::dec << member_index << ":0x" << std::hex
                      << member << ",id=0x" << member_id << ",class=0x"
                      << member_class << ",parent=0x" << member_parent;
            if (member_class == 0x57u)
                std::cerr << ",state=" << std::dec << notifier_state << std::hex
                          << ",initial_mode=" << std::dec << notifier_initial_mode << std::hex
                          << ",state_time=" << std::dec << notifier_state_time
                          << ",saved_event=" << std::hex << notifier_saved_event
                          << ",event_time=" << std::dec << notifier_event_time
                          << ",graph=0x" << std::hex << notifier_graph
                          << ",walker=0x" << notifier_handle << ",owner=0x"
                          << notifier_owner << ",gate_ref=0x"
                          << notifier_gate_ref << ",start=0x" << notifier_start
                          << ",aux=0x" << notifier_aux
                          << std::dec << ",root_list=" << notifier_in_root_list
                          << ",timer=" << notifier_left
                          << '(' << notifier_min << '-' << notifier_max << ')';
            std::cerr << ']';
            cell = next_cell;
        }
        std::cerr << '\n';
        group = next_group;
    }
}

#define SLY1_REGISTER_ENTRY(name, target)                                                        \
    uint64_t name(                                                                               \
        uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,          \
        float, float, float, float, float, float, float, float                                   \
    ) {                                                                                          \
        using RegisterTarget = int (*)(int, int);                                                \
        (void)reinterpret_cast<RegisterTarget>(target)(1, -1);                                   \
        return 0;                                                                                \
    }                                                                                            \
    uint64_t name##_thunk(const uint64_t*, const float*, float*) {                               \
        using RegisterTarget = int (*)(int, int);                                                \
        (void)reinterpret_cast<RegisterTarget>(target)(1, -1);                                   \
        return 0;                                                                                \
    }

SLY1_REGISTER_ENTRY(register_0016c450, func_0016C360)
SLY1_REGISTER_ENTRY(register_001963b8, func_00196330)
SLY1_REGISTER_ENTRY(register_001aec90, func_001AEA70)
SLY1_REGISTER_ENTRY(register_001e08d8, func_001E0848)
SLY1_REGISTER_ENTRY(register_001f2738, func_001F2688)

#undef SLY1_REGISTER_ENTRY

void register_sly1_subentries() {
    static const openrac_fn_entry functions[] = {
        {0x0016C450u, reinterpret_cast<openrac_host_fn>(register_0016c450), "func_0016C450"},
        {0x001963B8u, reinterpret_cast<openrac_host_fn>(register_001963b8), "func_001963B8"},
        {0x001AEC90u, reinterpret_cast<openrac_host_fn>(register_001aec90), "func_001AEC90"},
        {0x001E08D8u, reinterpret_cast<openrac_host_fn>(register_001e08d8), "func_001E08D8"},
        {0x001F2738u, reinterpret_cast<openrac_host_fn>(register_001f2738), "func_001F2738"},
    };
    static const openrac_thunk_entry thunks[] = {
        {reinterpret_cast<openrac_host_fn>(register_0016c450), register_0016c450_thunk},
        {reinterpret_cast<openrac_host_fn>(register_001963b8), register_001963b8_thunk},
        {reinterpret_cast<openrac_host_fn>(register_001aec90), register_001aec90_thunk},
        {reinterpret_cast<openrac_host_fn>(register_001e08d8), register_001e08d8_thunk},
        {reinterpret_cast<openrac_host_fn>(register_001f2738), register_001f2738_thunk},
    };
    openrac_guest_register(OPENRAC_OVERLAY_EXE, functions, sizeof(functions) / sizeof(functions[0]));
    openrac_guest_register_thunks(thunks, sizeof(thunks) / sizeof(thunks[0]));
}

void register_sly1_native_backfills() {
    static const openrac_fn_entry functions[] = {
        {0x0018E5E8u, sly1_native_start_worker, "sly1_native_start_worker"},
        {0x001E2398u, sly1_native_start_loader, "sly1_native_start_loader"},
        {0x0015E810u, reinterpret_cast<openrac_host_fn>(func_0015E810), "func_0015E810"},
        {0x0015EDB8u, reinterpret_cast<openrac_host_fn>(func_0015EDB8), "func_0015EDB8"},
        {0x0015EE80u, reinterpret_cast<openrac_host_fn>(func_0015EE80), "func_0015EE80"},
    };
    openrac_guest_replace_functions(OPENRAC_OVERLAY_EXE, functions,
                                    sizeof(functions) / sizeof(functions[0]));
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2 && argc != 3) {
        std::cerr << "usage: sly1_native <extracted Sly 1 boot executable> [disc ISO or BIN]\n";
        return 2;
    }

    std::vector<std::uint8_t> executable;
    if (!openrac::runtime::read_file(argv[1], &executable)) {
        std::cerr << "could not read the Sly 1 boot executable: " << argv[1] << '\n';
        return 1;
    }

    openrac::runtime::ElfImage image;
    std::string reason;
    if (!openrac::runtime::read_elf(executable, &image, &reason)) {
        std::cerr << "invalid Sly 1 boot executable: " << reason << '\n';
        return 1;
    }

    /* The retail heap still ends at 32 MiB (InitHeap sets 0x02000000).
     * Native large-resource streaming uses a port-only ring above that limit. */
    openrac::runtime::Memory::create(GUEST_RAM_EXPANDED_SIZE);
    if (!openrac::runtime::load_elf(executable, image, &reason)) {
        std::cerr << "could not load the Sly 1 boot executable: " << reason << '\n';
        return 1;
    }

    if (argc == 3) {
        openrac_game_disc_image = argv[2];
        if (!openrac_guest_disc_open(openrac_game_disc_image)) {
            std::cerr << "could not open the Sly 1 disc image as ISO 9660: " << argv[2] << '\n';
            return 1;
        }
    }

    openrac::runtime::install_crash_handler();
    openrac_game_register_functions();
    register_sly1_subentries();
    register_sly1_native_backfills();
    openrac_guest_set_overlay(OPENRAC_OVERLAY_EXE);
    std::atomic<bool> trace_stop{false};
    std::thread trace_thread;
    const char* trace_setting = std::getenv("OPENRAC_STARTUP_TRACE");
    if (trace_setting != nullptr && std::string(trace_setting) != "0") {
        openrac_guest_trace_enable(1);
        trace_thread = std::thread([&trace_stop] {
            std::uint32_t previous_range_state[8]{};
            bool have_previous_range_state = false;
            std::array<std::uint32_t, 11> previous_fade_state{};
            bool have_previous_fade_state = false;
            bool wrote_object_snapshot = false;
            bool wrote_ui_callback_snapshot = false;
            auto last_full_trace = std::chrono::steady_clock::now() -
                std::chrono::seconds(1);
            while (!trace_stop.load(std::memory_order_relaxed)) {
                const gaddr address = openrac_guest_last_called_address();
                const char* name = openrac_guest_function_name(address);
                const uint32_t service = openrac_guest_last_kernel_service();
                const uint32_t thread = openrac_guest_last_kernel_thread();
                const gaddr thread_entry = openrac_guest_thread_entry(thread);
                const char* thread_name = openrac_guest_function_name(thread_entry);
                uint64_t framebuffer = 0;
                uint64_t scissor = 0;
                uint64_t bitbltbuf = 0;
                uint64_t trxpos = 0;
                uint64_t trxreg = 0;
                uint64_t trxdir = 0;
                uint64_t pmode = 0;
                uint64_t dispfb1 = 0;
                uint64_t display1 = 0;
                uint64_t tex0_1 = 0;
                uint64_t tex1_1 = 0;
                uint64_t tex0_2 = 0;
                uint64_t xyoffset = 0;
                uint64_t prim = 0;
                uint64_t uv = 0;
                openrac_guest_gs_read_register(0x4Cu, &framebuffer);
                openrac_guest_gs_read_register(0x40u, &scissor);
                openrac_guest_gs_read_register(0x06u, &tex0_1);
                openrac_guest_gs_read_register(0x07u, &tex0_2);
                openrac_guest_gs_read_register(0x18u, &xyoffset);
                openrac_guest_gs_read_register(0x14u, &tex1_1);
                openrac_guest_gs_read_register(0x00u, &prim);
                openrac_guest_gs_read_register(0x03u, &uv);
                openrac_guest_gs_read_register(0x50u, &bitbltbuf);
                openrac_guest_gs_read_register(0x51u, &trxpos);
                openrac_guest_gs_read_register(0x52u, &trxreg);
                openrac_guest_gs_read_register(0x53u, &trxdir);
                openrac_guest_gs_read_privileged_register(0x00u, &pmode);
                openrac_guest_gs_read_privileged_register(0x70u, &dispfb1);
                openrac_guest_gs_read_privileged_register(0x80u, &display1);
                uint32_t vif1_chcr = 0;
                uint32_t vif1_madr = 0;
                uint32_t vif1_qwc = 0;
                uint32_t vif1_tadr = 0;
                std::memcpy(&vif1_chcr, openrac_guest_base + 0x10009000u,
                            sizeof(vif1_chcr));
                std::memcpy(&vif1_madr, openrac_guest_base + 0x10009010u,
                            sizeof(vif1_madr));
                std::memcpy(&vif1_qwc, openrac_guest_base + 0x10009020u,
                            sizeof(vif1_qwc));
                std::memcpy(&vif1_tadr, openrac_guest_base + 0x10009030u,
                            sizeof(vif1_tadr));
                std::uint32_t range_state[8]{};
                std::memcpy(range_state, openrac_guest_base + 0x00275AF0u,
                            sizeof(range_state));
                std::uint32_t frame_state[13]{};
                std::memcpy(frame_state, openrac_guest_base + 0x00275BC0u,
                            sizeof(frame_state));
                std::uint32_t frame_top = 0;
                std::uint32_t frame_top_vtable = 0;
                std::uint32_t frame_top_callback = 0;
                std::uint32_t frame_top_kind = 0;
                std::uint32_t frame_top_mode = 0;
                std::uint32_t frame_top_state = 0;
                std::uint32_t frame_top_flags = 0;
                std::uint32_t frame_top_action = 0;
                std::uint32_t frame_top_pending = 0;
                std::uint32_t frame_top_timer_bits = 0;
                std::uint32_t global_state_ptr = 0;
                std::uint32_t global_state[10]{};
                const std::uint32_t frame_count = frame_state[11];
                if (frame_count > 0 && frame_count <= 8) {
                    std::memcpy(&frame_top,
                                openrac_guest_base + 0x00275BC0u + 0x0Cu +
                                    (frame_count - 1u) * sizeof(frame_top),
                                sizeof(frame_top));
                    const std::uint32_t frame_top_physical =
                        openrac_guest_ram_address(frame_top);
                    if (frame_top != 0 && frame_top_physical <= openrac_guest_ram_size &&
                        0x28Cu <=
                            openrac_guest_ram_size - frame_top_physical) {
                        std::memcpy(&frame_top_vtable,
                                    openrac_guest_base + frame_top_physical,
                                    sizeof(frame_top_vtable));
                        std::memcpy(&frame_top_mode,
                                    openrac_guest_base + frame_top_physical + 0x250u,
                                    sizeof(frame_top_mode));
                        std::memcpy(&frame_top_kind,
                                    openrac_guest_base + frame_top_physical + 0x260u,
                                    sizeof(frame_top_kind));
                        std::memcpy(&frame_top_state,
                                    openrac_guest_base + frame_top_physical + 0x274u,
                                    sizeof(frame_top_state));
                        std::memcpy(&frame_top_flags,
                                    openrac_guest_base + frame_top_physical + 0x278u,
                                    sizeof(frame_top_flags));
                        std::memcpy(&frame_top_pending,
                                    openrac_guest_base + frame_top_physical + 0x288u,
                                    sizeof(frame_top_pending));
                        std::memcpy(&frame_top_action,
                                    openrac_guest_base + frame_top_physical + 0x27Cu,
                                    sizeof(frame_top_action));
                        std::memcpy(&frame_top_timer_bits,
                                    openrac_guest_base + frame_top_physical + 0x254u,
                                    sizeof(frame_top_timer_bits));
                        const std::uint32_t vtable_physical =
                            openrac_guest_ram_address(frame_top_vtable);
                        if (frame_top_vtable != 0 &&
                            vtable_physical <= openrac_guest_ram_size &&
                            0x14u <= openrac_guest_ram_size - vtable_physical) {
                            std::memcpy(&frame_top_callback,
                                        openrac_guest_base + vtable_physical + 0x10u,
                                        sizeof(frame_top_callback));
                        }
                    }
                }
                std::memcpy(&global_state_ptr, openrac_guest_base + 0x002623C0u,
                            sizeof(global_state_ptr));
                const std::uint32_t global_state_physical =
                    openrac_guest_ram_address(global_state_ptr);
                if (global_state_ptr != 0 && global_state_physical <= openrac_guest_ram_size &&
                    0x1A00u <= openrac_guest_ram_size - global_state_physical) {
                    std::memcpy(&global_state[0], openrac_guest_base + global_state_physical + 0x19D8u, 4);
                    std::memcpy(&global_state[1], openrac_guest_base + global_state_physical + 0x19DCu, 4);
                    std::memcpy(&global_state[2], openrac_guest_base + global_state_physical + 0x19E0u, 4);
                    std::memcpy(&global_state[3], openrac_guest_base + global_state_physical + 0x19E4u, 4);
                    std::memcpy(&global_state[4], openrac_guest_base + global_state_physical + 0x19E8u, 4);
                    std::memcpy(&global_state[5], openrac_guest_base + global_state_physical + 0x19ECu, 4);
                    std::memcpy(&global_state[6], openrac_guest_base + global_state_physical + 0x19F0u, 4);
                    std::memcpy(&global_state[7], openrac_guest_base + global_state_physical + 0x19F4u, 4);
                    std::memcpy(&global_state[8], openrac_guest_base + global_state_physical + 0x19F8u, 4);
                    std::memcpy(&global_state[9], openrac_guest_base + global_state_physical + 0x19FCu, 4);
                }
                std::uint32_t menu_state[4]{};
                std::memcpy(&menu_state[1], openrac_guest_base + 0x002701C8u,
                            sizeof(menu_state[1]));
                std::memcpy(&menu_state[2], openrac_guest_base + 0x002701CCu,
                            sizeof(menu_state[2]));
                std::memcpy(&menu_state[3], openrac_guest_base + 0x002701C4u,
                            sizeof(menu_state[3]));
                float menu_blend = 0.0f;
                float menu_since = 0.0f;
                std::uint32_t menu_clock = 0;
                float menu_clock_value = 0.0f;
                std::uint32_t menu_font = 0;
                std::uint32_t menu_title = 0;
                std::uint32_t menu_items = 0;
                std::uint32_t menu_count = 0;
                std::uint32_t menu_glyphs = 0;
                std::uint32_t menu_glyph_count = 0;
                std::uint32_t movie_player[4]{};
                std::uint32_t movie_decode_step = 0;
                std::uint32_t pending_resource = 0;
                std::uint32_t resource_state_mode = 0;
                std::uint32_t resource_state_busy = 0;
                std::memcpy(&menu_font, openrac_guest_base + 0x0026FF6Cu,
                            sizeof(menu_font));
                std::memcpy(&menu_title, openrac_guest_base + 0x00269D50u,
                            sizeof(menu_title));
                std::memcpy(&menu_count, openrac_guest_base + 0x00269D60u,
                            sizeof(menu_count));
                std::memcpy(&menu_items, openrac_guest_base + 0x00269D64u,
                            sizeof(menu_items));
                std::memcpy(movie_player,
                            openrac_guest_base + 0x00269A00u,
                            sizeof(movie_player));
                std::memcpy(&movie_decode_step,
                            openrac_guest_base + 0x00269B34u,
                            sizeof(movie_decode_step));
                std::memcpy(&pending_resource, openrac_guest_base + 0x00275AF0u,
                            sizeof(pending_resource));
                std::memcpy(&resource_state_busy, openrac_guest_base + 0x00275F84u,
                            sizeof(resource_state_busy));
                std::memcpy(&resource_state_mode, openrac_guest_base + 0x00275FA4u,
                            sizeof(resource_state_mode));
                if (menu_font != 0) {
                    const std::uint32_t font_physical = openrac_guest_ram_address(menu_font);
                    if (font_physical <= openrac_guest_ram_size &&
                        0x80u <= openrac_guest_ram_size - font_physical) {
                        std::memcpy(&menu_glyph_count,
                                    openrac_guest_base + font_physical + 0x78u,
                                    sizeof(menu_glyph_count));
                        std::memcpy(&menu_glyphs,
                                    openrac_guest_base + font_physical + 0x7Cu,
                                    sizeof(menu_glyphs));
                    }
                }
                std::memcpy(&menu_blend, openrac_guest_base + 0x002701A4u,
                            sizeof(menu_blend));
                std::memcpy(&menu_since, openrac_guest_base + 0x002701BCu,
                            sizeof(menu_since));
                std::memcpy(&menu_clock, openrac_guest_base + 0x002701C0u,
                            sizeof(menu_clock));
                const std::uint32_t menu_clock_physical =
                    openrac_guest_ram_address(menu_clock);
                if (menu_clock != 0 && menu_clock_physical <= openrac_guest_ram_size &&
                    sizeof(menu_clock_value) <= openrac_guest_ram_size - menu_clock_physical) {
                    std::memcpy(&menu_clock_value, openrac_guest_base + menu_clock_physical,
                                sizeof(menu_clock_value));
                }
                std::uint32_t overlay_state[3]{};
                std::memcpy(&overlay_state[0], openrac_guest_base + 0x00275BC4u,
                            sizeof(overlay_state[0]));
                std::memcpy(&overlay_state[1], openrac_guest_base + 0x00275BC8u,
                            sizeof(overlay_state[1]));
                std::memcpy(&overlay_state[2], openrac_guest_base + 0x00275BECu,
                            sizeof(overlay_state[2]));
                std::uint32_t ui_active_count = 0;
                std::uint32_t ui_visible_count = 0;
                std::uint32_t ui_font_count = 0;
                struct UiSample {
                    std::uint32_t index;
                    std::uint32_t object;
                    std::uint32_t mode;
                    std::uint32_t font;
                    std::uint32_t frame;
                    std::uint32_t draw;
                    std::uint32_t font_vtable;
                    std::uint32_t font_advance;
                    std::uint32_t font_height;
                    std::uint32_t font_image_a;
                    std::uint32_t font_image_b;
                    std::uint64_t font_tex0;
                    std::uint32_t glyph_count;
                    std::uint32_t glyphs;
                    std::uint32_t first_text_byte;
                    std::uint32_t menu_title;
                    std::uint32_t menu_items;
                    std::uint32_t menu_count;
                    std::uint32_t item0_index;
                    std::uint32_t item0_string;
                    std::uint32_t item0_text_byte;
                } ui_samples[8]{};
                std::uint32_t ui_sample_count = 0;
                for (std::uint32_t i = 0; i < 37; ++i) {
                    std::uint32_t object = 0;
                    std::memcpy(&object, openrac_guest_base + 0x002486B0u + i * 4u,
                                sizeof(object));
                    ui_active_count += object != 0;
                    if (object == 0) continue;
                    const std::uint32_t object_physical = openrac_guest_ram_address(object);
                    if (object_physical > openrac_guest_ram_size ||
                        0x25Du > openrac_guest_ram_size - object_physical) continue;
                    std::uint32_t mode = 0, font = 0, frame = 0, vtable = 0;
                    std::memcpy(&mode, openrac_guest_base + object_physical + 0x250u, 4);
                    std::memcpy(&font, openrac_guest_base + object_physical + 0x04u, 4);
                    std::memcpy(&frame, openrac_guest_base + object_physical + 0x210u, 4);
                    std::memcpy(&vtable, openrac_guest_base + object_physical, 4);
                    const std::uint32_t first_text_byte =
                        openrac_guest_base[object_physical + 0x08u];
                    std::uint32_t draw = 0;
                    const std::uint32_t vtable_physical = openrac_guest_ram_address(vtable);
                    if (vtable != 0 && vtable_physical <= openrac_guest_ram_size &&
                        0x28u <= openrac_guest_ram_size - vtable_physical)
                        std::memcpy(&draw, openrac_guest_base + vtable_physical + 9u * 4u, 4);
                    std::uint32_t font_vtable = 0, font_advance = 0;
                    std::uint32_t font_height = 0, glyph_count = 0, glyphs = 0;
                    std::uint32_t font_image_a = 0, font_image_b = 0;
                    std::uint64_t font_tex0 = 0;
                    const bool font_in_port_heap =
                        font >= GUEST_PORT && font - GUEST_PORT < GUEST_PORT_SIZE;
                    const std::uint32_t font_physical = font_in_port_heap
                        ? font : openrac_guest_ram_address(font);
                    const std::uint32_t font_limit = font_in_port_heap
                        ? GUEST_PORT + GUEST_PORT_SIZE : openrac_guest_ram_size;
                    if (font != 0 && font_physical <= font_limit &&
                        0x80u <= font_limit - font_physical) {
                        std::memcpy(&font_advance, openrac_guest_base + font_physical + 4u, 4);
                        std::memcpy(&font_height, openrac_guest_base + font_physical + 8u, 4);
                        std::memcpy(&font_vtable, openrac_guest_base + font_physical + 0x4Cu, 4);
                        std::memcpy(&font_image_a, openrac_guest_base + font_physical + 0x54u, 4);
                        std::memcpy(&font_image_b, openrac_guest_base + font_physical + 0x58u, 4);
                        std::memcpy(&font_tex0, openrac_guest_base + font_physical + 0x60u, 8);
                        std::memcpy(&glyph_count, openrac_guest_base + font_physical + 0x78u, 4);
                        std::memcpy(&glyphs, openrac_guest_base + font_physical + 0x7Cu, 4);
                    }
                    std::uint32_t menu_title = 0, menu_items = 0, menu_count = 0;
                    std::uint32_t item0_index = 0, item0_string = 0, item0_text_byte = 0;
                    std::uint32_t page = 0;
                    std::memcpy(&page, openrac_guest_base + object_physical + 0x260u, 4);
                    if (mode == 2 && page < 33u) {
                        const std::uint32_t menu = 0x00269D50u + page * 24u;
                        std::memcpy(&menu_title, openrac_guest_base + menu, 4);
                        std::memcpy(&menu_count, openrac_guest_base + menu + 0x10u, 4);
                        std::memcpy(&menu_items, openrac_guest_base + menu + 0x14u, 4);
                        const std::uint32_t items_physical = openrac_guest_ram_address(menu_items);
                        if (menu_count != 0 && menu_items != 0 &&
                            items_physical <= openrac_guest_ram_size &&
                            4u <= openrac_guest_ram_size - items_physical) {
                            std::memcpy(&item0_index, openrac_guest_base + items_physical, 4);
                            const std::uint32_t string_slot = 0x0026A068u + item0_index * 4u;
                            if (string_slot <= openrac_guest_ram_size - 4u) {
                                std::memcpy(&item0_string, openrac_guest_base + string_slot, 4);
                                const std::uint32_t string_physical =
                                    openrac_guest_ram_address(item0_string);
                                if (item0_string != 0 && string_physical < openrac_guest_ram_size)
                                    item0_text_byte = openrac_guest_base[string_physical];
                            }
                        }
                    }
                    ui_visible_count += mode != 0 && first_text_byte != 0;
                    ui_font_count += font != 0;
                    if (ui_sample_count < 8 && (mode != 0 || first_text_byte != 0)) {
                        ui_samples[ui_sample_count++] = {
                            i, object, mode, font, frame, draw, font_vtable,
                            font_advance, font_height, font_image_a, font_image_b,
                            font_tex0, glyph_count, glyphs,
                            first_text_byte, menu_title, menu_items, menu_count,
                            item0_index, item0_string, item0_text_byte};
                    }
                }
                std::uint32_t active_world = 0;
                std::memcpy(&active_world, openrac_guest_base + 0x00275710u, 4);
                std::uint32_t font_count = 0, font_array = 0, default_font = 0;
                std::uint32_t image_count_a = 0, image_records_a = 0;
                std::uint32_t image_count_b = 0, image_records_b = 0;
                std::uint32_t font_ids[4]{};
                std::memcpy(&font_count, openrac_guest_base + 0x00262260u, 4);
                std::memcpy(&font_array, openrac_guest_base + 0x00262264u, 4);
                std::memcpy(&default_font, openrac_guest_base + 0x0026227Cu, 4);
                std::memcpy(&image_count_a, openrac_guest_base + 0x00274500u, 4);
                std::memcpy(&image_records_a, openrac_guest_base + 0x00274504u, 4);
                std::memcpy(&image_count_b, openrac_guest_base + 0x00274508u, 4);
                std::memcpy(&image_records_b, openrac_guest_base + 0x0027450Cu, 4);
                std::memcpy(font_ids, openrac_guest_base + 0x00262268u,
                            sizeof(font_ids));
                std::uint32_t game_mode = 0;
                std::uint32_t current_object = 0;
                std::uint32_t resource_manager = 0;
                std::uint32_t camera_object = 0;
                std::uint32_t camera_stack[4]{};
                std::uint32_t resource_level = 0;
                std::uint32_t resource_record = 0;
                std::uint32_t resource_visual_state[8]{};
                std::int32_t resource_level_index = -1;
                std::int32_t resource_record_index = -1;
                std::int32_t resource_record_flags = 0;
                std::uint32_t menu_mode = 0;
                std::uint32_t world_lists[7]{};
                std::uint32_t world_job_queue[2]{};
                std::uint32_t deferred_request_count = 0;
                std::uint32_t frame_selector = 0;
                std::int32_t element_stack_state = 0;
                float element_stack_state_started_at = 0.0f;
                std::int32_t element_stack_resume_state = 0;
                float game_elapsed_time = 0.0f;
                std::uint32_t state_data_address = 0;
                std::int32_t state_data_mode = 0;
                std::uint16_t state_data_flags = 0;
                std::uint32_t state_transition_flags = 0;
                std::int32_t screen_fade_mode = 0;
                std::uint32_t screen_fade_request[6]{};
                std::uint32_t world_load_request[5]{};
                std::uint32_t last_world_key[8]{};
                std::uint32_t scene_pending = 0;
                std::int32_t scene_pending_state = -1;
                std::uint16_t frame_flags_a = 0;
                std::uint16_t frame_flags_b = 0;
                std::uint32_t pad_report = 0;
                std::uint32_t pad_mode = 0;
                std::uint32_t pad_state = 0;
                std::uint16_t pad_buttons = 0;
                std::uint16_t pad_pressed = 0;
                float frame_elapsed = 0.0f;
                std::memcpy(&game_mode, openrac_guest_base + 0x00264A58u,
                            sizeof(game_mode));
                std::memcpy(&current_object, openrac_guest_base + 0x00275710u,
                            sizeof(current_object));
                std::memcpy(&resource_manager, openrac_guest_base + 0x002623C0u,
                            sizeof(resource_manager));
                std::memcpy(&camera_object, openrac_guest_base + 0x00261990u,
                            sizeof(camera_object));
                const std::uint32_t camera_physical =
                    openrac_guest_ram_address(camera_object);
                if (camera_object != 0 && camera_physical <= openrac_guest_ram_size &&
                    0x3E0u + sizeof(camera_stack) <=
                        openrac_guest_ram_size - camera_physical) {
                    std::memcpy(&camera_stack[0], openrac_guest_base + camera_physical + 0x3D0u,
                                sizeof(camera_stack[0]));
                    std::memcpy(&camera_stack[1], openrac_guest_base + camera_physical + 0x220u,
                                sizeof(camera_stack[1]));
                    std::memcpy(&camera_stack[2], openrac_guest_base + camera_physical + 0x224u,
                                sizeof(camera_stack[2]));
                    std::memcpy(&camera_stack[3], openrac_guest_base + camera_physical + 0x3D8u,
                                sizeof(camera_stack[3]));
                }
                std::memcpy(&resource_level, openrac_guest_base + 0x002623C4u,
                            sizeof(resource_level));
                std::memcpy(&resource_record, openrac_guest_base + 0x002623C8u,
                            sizeof(resource_record));
                std::memcpy(resource_visual_state, openrac_guest_base + 0x00261EF4u,
                            sizeof(resource_visual_state));
                const std::uint32_t resource_manager_physical =
                    openrac_guest_ram_address(resource_manager);
                if (resource_manager != 0 && resource_manager_physical <= openrac_guest_ram_size &&
                    0x19E0u + sizeof(resource_level_index) <=
                        openrac_guest_ram_size - resource_manager_physical) {
                    std::memcpy(&resource_level_index,
                                openrac_guest_base + resource_manager_physical + 0x19D8u,
                                sizeof(resource_level_index));
                    std::memcpy(&resource_record_index,
                                openrac_guest_base + resource_manager_physical + 0x19DCu,
                                sizeof(resource_record_index));
                }
                const std::uint32_t resource_record_physical =
                    openrac_guest_ram_address(resource_record);
                if (resource_record != 0 && resource_record_physical <= openrac_guest_ram_size &&
                    sizeof(resource_record_flags) <=
                        openrac_guest_ram_size - resource_record_physical) {
                    std::memcpy(&resource_record_flags,
                                openrac_guest_base + resource_record_physical,
                                sizeof(resource_record_flags));
                }
                std::memcpy(&menu_mode, openrac_guest_base + 0x002701B8u,
                            sizeof(menu_mode));
                std::memcpy(&element_stack_state, openrac_guest_base + 0x00275BC4u,
                            sizeof(element_stack_state));
                std::memcpy(&element_stack_state_started_at,
                            openrac_guest_base + 0x00275BC8u,
                            sizeof(element_stack_state_started_at));
                std::memcpy(&element_stack_resume_state,
                            openrac_guest_base + 0x00275BF0u,
                            sizeof(element_stack_resume_state));
                std::memcpy(&game_elapsed_time, openrac_guest_base + 0x00261868u,
                            sizeof(game_elapsed_time));
                std::uint32_t world_address = 0;
                std::uint32_t special_scene_object = 0;
                if (current_object != 0 &&
                    trace_guest_range(current_object, 0x1ED0u, &world_address)) {
                    std::memcpy(&world_lists[0], openrac_guest_base + world_address + 0x48u,
                                sizeof(world_lists[0]));
                    std::memcpy(&world_lists[1], openrac_guest_base + world_address + 0x54u,
                                sizeof(world_lists[1]));
                    std::memcpy(&world_lists[2], openrac_guest_base + world_address + 0x60u,
                                sizeof(world_lists[2]));
                    std::memcpy(&world_lists[3], openrac_guest_base + world_address + 0x6Cu,
                                sizeof(world_lists[3]));
                    std::memcpy(&world_lists[4], openrac_guest_base + world_address + 0x78u,
                                sizeof(world_lists[4]));
                    std::memcpy(&world_lists[5], openrac_guest_base + world_address + 0x1B7Cu,
                                sizeof(world_lists[5]));
                    std::memcpy(&world_lists[6], openrac_guest_base + world_address + 0x1C54u,
                                sizeof(world_lists[6]));
                    std::memcpy(world_job_queue, openrac_guest_base + world_address + 0x1EC8u,
                                sizeof(world_job_queue));
                    std::memcpy(&special_scene_object,
                                openrac_guest_base + world_address + 0x1EC4u,
                                sizeof(special_scene_object));
                }
                std::memcpy(&frame_selector, openrac_guest_base + 0x00262C68u,
                            sizeof(frame_selector));
                state_data_address = (frame_selector & 4u) != 0
                    ? 0x00262C70u : 0x00262D38u;
                std::memcpy(&state_data_mode,
                            openrac_guest_base + state_data_address + 0x14u,
                            sizeof(state_data_mode));
                std::memcpy(&state_data_flags,
                            openrac_guest_base + state_data_address + 0xAAu,
                            sizeof(state_data_flags));
                std::memcpy(&state_transition_flags, openrac_guest_base + 0x00262C6Cu,
                            sizeof(state_transition_flags));
                std::uint32_t deferred_cursor = 0;
                std::uint32_t deferred_cursor_address = 0;
                if (trace_guest_range(0x0026AC10u, sizeof(deferred_cursor),
                                      &deferred_cursor_address)) {
                    std::memcpy(&deferred_cursor,
                                openrac_guest_base + deferred_cursor_address,
                                sizeof(deferred_cursor));
                    if (deferred_cursor >= 0x005E4700u &&
                        deferred_cursor <= 0x005E6700u) {
                        deferred_request_count =
                            (deferred_cursor - 0x005E4700u) / 0x80u;
                    }
                }
                std::memcpy(&screen_fade_mode, openrac_guest_base + 0x00275F84u,
                            sizeof(screen_fade_mode));
                std::memcpy(screen_fade_request, openrac_guest_base + 0x00275F90u,
                            sizeof(screen_fade_request));
                std::memcpy(world_load_request, openrac_guest_base + 0x00275AF0u,
                            sizeof(world_load_request));
                if (std::getenv("OPENRAC_TRACE_BOOT_TRANSITIONS") != nullptr) {
                    const std::array<std::uint32_t, 11> state = {
                        static_cast<std::uint32_t>(screen_fade_mode),
                        screen_fade_request[0], screen_fade_request[1],
                        screen_fade_request[2], screen_fade_request[3],
                        screen_fade_request[4], screen_fade_request[5],
                        world_load_request[0], world_load_request[1],
                        world_load_request[2], world_load_request[4]};
                    if (!have_previous_fade_state || state != previous_fade_state) {
                        std::cerr << "[boot-transition] game_mode=0x" << std::hex
                                  << game_mode << " fade=" << state[0] << ','
                                  << state[1] << ",key=0x" << state[2]
                                  << ",arg=0x" << state[3] << ",flags=0x"
                                  << state[5] << ",style=0x" << state[6]
                                  << " world=" << state[7] << ",key=0x"
                                  << state[8] << ",arg=0x" << state[9]
                                  << ",flags=0x" << state[10] << std::dec << '\n';
                        previous_fade_state = state;
                        have_previous_fade_state = true;
                    }
                }
                std::memcpy(last_world_key, openrac_guest_base + 0x00275B04u,
                            sizeof(last_world_key));
                std::memcpy(&frame_flags_a, openrac_guest_base + 0x00262D38u + 0xAAu,
                            sizeof(frame_flags_a));
                std::memcpy(&frame_flags_b, openrac_guest_base + 0x00262C70u + 0xAAu,
                            sizeof(frame_flags_b));
                std::memcpy(&pad_report, openrac_guest_base + 0x00262C70u + 0x0Cu,
                            sizeof(pad_report));
                std::memcpy(&pad_mode, openrac_guest_base + 0x00262C70u + 0x14u,
                            sizeof(pad_mode));
                std::memcpy(&pad_state, openrac_guest_base + 0x00262C70u + 0x18u,
                            sizeof(pad_state));
                std::memcpy(&pad_buttons, openrac_guest_base + 0x00262C70u + 0xA8u,
                            sizeof(pad_buttons));
                std::memcpy(&pad_pressed, openrac_guest_base + 0x00262C70u + 0xAAu,
                            sizeof(pad_pressed));
                std::memcpy(&frame_elapsed, openrac_guest_base + 0x0026185Cu,
                            sizeof(frame_elapsed));
                std::memcpy(&scene_pending, openrac_guest_base + 0x0026F808u + 0x260u,
                            sizeof(scene_pending));
                const std::uint32_t scene_pending_physical =
                    openrac_guest_ram_address(scene_pending);
                if (scene_pending != 0 && scene_pending_physical <= openrac_guest_ram_size &&
                    0x2D0u + sizeof(scene_pending_state) <=
                        openrac_guest_ram_size - scene_pending_physical) {
                    std::memcpy(&scene_pending_state,
                                openrac_guest_base + scene_pending_physical + 0x2D0u,
                                sizeof(scene_pending_state));
                }
                std::uint32_t range_record[8]{};
                const bool have_range_record = range_state[1] != 0 &&
                    range_state[1] < 0x02000000u;
                if (have_range_record) {
                    std::memcpy(range_record, openrac_guest_base + range_state[1],
                                sizeof(range_record));
                }
                if (!have_previous_range_state ||
                    std::memcmp(previous_range_state, range_state, sizeof(range_state)) != 0) {
                    std::cerr << "[range] state=";
                    for (const std::uint32_t word : range_state)
                        std::cerr << std::hex << word << ',';
                    if (have_range_record) {
                        std::cerr << " record=";
                        for (const std::uint32_t word : range_record)
                            std::cerr << std::hex << word << ',';
                    }
                    std::cerr << std::dec << '\n';
                    std::memcpy(previous_range_state, range_state, sizeof(range_state));
                    have_previous_range_state = true;
                }
                const auto now = std::chrono::steady_clock::now();
                if (now - last_full_trace < std::chrono::seconds(1)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                last_full_trace = now;
                std::cerr << "[trace] guest=" << (name == nullptr ? "unknown" : name)
                          << " address=0x" << std::hex << address << std::dec
                          << " kernel_service=";
                if (service == 0xFFFFFFFFu) std::cerr << "none";
                else std::cerr << "0x" << std::hex << service << std::dec;
                std::cerr << " kernel_thread=" << thread << " thread_entry="
                          << (thread_name == nullptr ? "main/unknown" : thread_name)
                          << "(0x" << std::hex << thread_entry << std::dec << ')'
                          << " gif_eop=" << openrac_guest_gif_eop_count()
                          << " frame1=0x" << std::hex << framebuffer
                          << " scissor=0x" << scissor << " blit=0x" << bitbltbuf
                          << " trxpos=0x" << trxpos << " trxreg=0x" << trxreg
                          << " trxdir=0x" << trxdir << " pmode=0x" << pmode
                          << " tex0_1=0x" << tex0_1 << " tex0_2=0x" << tex0_2
                          << " tex1_1=0x" << tex1_1 << " xyoffset=0x" << xyoffset
                          << " prim=0x" << prim
                          << " uv=0x" << uv << " vif1_chcr=0x" << vif1_chcr
                          << " vif1_madr=0x" << vif1_madr << " vif1_qwc=0x" << vif1_qwc
                          << " vif1_tadr=0x" << vif1_tadr
                          << " range_state=";
                for (const std::uint32_t word : range_state)
                    std::cerr << std::hex << word << ',';
                if (have_range_record) {
                    std::cerr << " range_record=";
                    for (const std::uint32_t word : range_record)
                        std::cerr << std::hex << word << ',';
                }
                std::cerr << std::dec
                          << " element_stack_prefix=" << frame_state[0] << ','
                          << frame_state[1] << ',' << frame_state[2]
                          << " element_stack_count_candidate=" << frame_state[11]
                          << " element_stack_top=0x" << std::hex << frame_top
                          << ",vtable=0x" << frame_top_vtable
                          << ",callback=0x" << frame_top_callback
                          << ",mode=0x" << frame_top_mode
                          << ",kind=0x" << frame_top_kind
                          << ",state=0x" << frame_top_state << std::dec
                          << std::hex << ",flags=0x" << frame_top_flags
                          << ",action=0x" << frame_top_action
                          << ",pending=0x" << frame_top_pending
                          << ",timer=0x" << frame_top_timer_bits
                          << ",global=0x" << global_state_ptr << ':'
                          << global_state[0] << ',' << global_state[1] << ','
                          << global_state[2] << ',' << global_state[3] << ','
                          << global_state[4] << ',' << global_state[5] << ','
                          << global_state[6] << ',' << global_state[7] << ','
                          << global_state[8] << ',' << global_state[9] << std::dec
                          << " raw_264a58=" << game_mode << " current_world=0x" << std::hex
                          << current_object << " menu=" << std::dec << menu_mode << ','
                          << menu_state[0] << ',' << menu_state[1] << ',' << menu_state[2]
                          << " menu_anim=" << menu_blend << ',' << menu_since << ','
                          << menu_clock << ':' << menu_clock_value
                          << " menu_data=" << std::hex << menu_font << ',' << menu_title
                          << ',' << menu_count << ',' << menu_items << ',' << menu_glyph_count
                          << ',' << menu_glyphs << std::dec
                          << " world_load_state=" << world_load_request[0]
                          << " world_load_key=0x" << std::hex << world_load_request[1]
                          << " world_load_f8=0x" << world_load_request[2]
                          << " world_load_fc=0x" << world_load_request[3]
                          << " world_load_flags=0x" << world_load_request[4]
                          << " last_world_key=" << std::hex
                          << last_world_key[0] << ',' << last_world_key[1] << ','
                          << last_world_key[2] << ',' << last_world_key[3] << ','
                          << last_world_key[4] << ',' << last_world_key[5] << ','
                          << last_world_key[6] << ',' << last_world_key[7]
                          << std::dec << " resource_request=0x" << std::hex
                          << pending_resource << std::dec
                          << " movie_index=" << std::dec << movie_player[0]
                          << " movie_queued=0x" << std::hex << movie_player[1]
                          << " movie_queued_next=0x" << movie_player[2]
                          << " movie_reader=0x" << movie_player[3]
                          << " movie_decode_step=" << std::dec << movie_decode_step
                          << " resource_draw=" << resource_state_busy << ','
                          << resource_state_mode
                          << " overlay=" << overlay_state[0] << ',' << overlay_state[1]
                          << ',' << overlay_state[2] << " ui_objects=" << ui_active_count
                          << " camera=0x" << camera_object << " camera_stack="
                          << std::dec << camera_stack[0] << ",0x" << std::hex
                          << camera_stack[3] << " camera_flags=" << camera_stack[1]
                          << ',' << camera_stack[2]
                          << " resource_manager=0x" << resource_manager
                          << " resource_level=0x" << resource_level
                          << " resource_record=0x" << resource_record
                          << " resource_visual_state=";
                for (const std::uint32_t word : resource_visual_state)
                    std::cerr << std::hex << word << ',';
                std::cerr << std::dec
                          << " resource_level_index=" << std::dec << resource_level_index
                          << " resource_record_index=" << resource_record_index
                          << " resource_record_flags=0x" << std::hex
                          << resource_record_flags
                          << " world_lists=" << world_lists[0] << ',' << world_lists[1]
                          << ',' << world_lists[2] << ',' << world_lists[3] << ','
                          << world_lists[4] << ',' << world_lists[5] << ',' << world_lists[6]
                          << " special_scene_object=0x" << std::hex
                          << special_scene_object
                          << " world_list_counts="
                          << trace_world_list_count(current_object, 0x48u) << ','
                          << trace_world_list_count(current_object, 0x54u) << ','
                          << trace_world_list_count(current_object, 0x60u) << ','
                          << trace_world_list_count(current_object, 0x6Cu) << ','
                          << trace_world_list_count(current_object, 0x78u) << ','
                          << trace_world_list_count(current_object, 0x1B7Cu) << ','
                          << trace_world_list_count(current_object, 0x1C54u)
                          << " class51_objects="
                          << trace_world_list_count(current_object, 0x1BE8u)
                          << " world_root_classes=";
                for (const std::uint32_t class_id :
                     trace_world_list_classes(current_object, 0x48u))
                    std::cerr << std::hex << class_id << ',';
                std::cerr << " family55_classes=";
                for (const std::uint32_t class_id :
                     trace_world_list_classes(current_object, 0x1C54u))
                    std::cerr << std::hex << class_id << ',';
                for (const std::uint32_t list_offset :
                     {0x54u, 0x60u, 0x6Cu, 0x78u, 0x1B7Cu, 0x1C54u}) {
                    std::cerr << " list_" << std::hex << list_offset << "_classes=";
                    for (const std::uint32_t class_id :
                         trace_world_list_classes(current_object, list_offset)) {
                        if (class_id == 0) break;
                        std::cerr << std::hex << class_id << ',';
                    }
                }
                const auto class51_fields =
                    trace_first_world_object_fields(current_object, 0x1BE8u);
                std::cerr << " class51_first=";
                for (const std::uint32_t field : class51_fields)
                    std::cerr << std::hex << field << ',';
                std::cerr << " root_objects=";
                const auto root_object_fields = trace_world_objects(current_object);
                for (const auto& fields : root_object_fields) {
                    if (fields[0] == 0) break;
                    std::cerr << '[' << std::hex << fields[0] << ':' << fields[1]
                              << ',' << fields[2] << ',' << fields[3] << ','
                              << fields[4] << ',' << fields[5] << ",children="
                              << std::dec << fields[6] << std::hex << ']';
                }
                for (std::uint32_t class_id = 0; class_id < 0x95u; ++class_id) {
                    trace_world_class_objects(current_object, class_id);
                    if (class_id == 0x53u)
                        trace_world_object_groups(current_object);
                }
                const bool has_scene_object = std::any_of(
                    root_object_fields.begin(), root_object_fields.end(),
                    [](const auto& fields) { return fields[0] != 0 && fields[4] == 0x44u; });
                if (!wrote_object_snapshot && has_scene_object &&
                    std::getenv("OPENRAC_TRACE_OBJECTS") != nullptr) {
                    wrote_object_snapshot = true;
                    for (const auto& fields : root_object_fields) {
                        if (fields[0] == 0) break;
                        if (fields[4] != 0x44u) continue;
                        std::uint32_t object_offset = 0;
                        std::uint32_t descriptor_offset = 0;
                        if (!trace_guest_range(fields[0], 0x20u, &object_offset))
                            continue;
                        std::uint32_t descriptor = 0;
                        std::memcpy(&descriptor, openrac_guest_base + object_offset, 4u);
                        if (!trace_guest_range(descriptor, 0x80u, &descriptor_offset))
                            continue;
                        std::uint32_t object_size = 0;
                        std::memcpy(&object_size,
                                    openrac_guest_base + descriptor_offset + 0x0Cu, 4u);
                        std::cerr << "\n[object-snapshot] id=" << std::hex << fields[4]
                                  << " object=" << fields[0] << " descriptor="
                                  << descriptor << " size=" << object_size << '\n';
                        const std::uint32_t descriptor_words = 0x80u / 4u;
                        for (std::uint32_t i = 0; i < descriptor_words; ++i) {
                            std::uint32_t value = 0;
                            std::memcpy(&value,
                                        openrac_guest_base + descriptor_offset + i * 4u, 4u);
                            std::cerr << "  descriptor+" << std::setw(3) << std::setfill('0')
                                      << std::hex << i * 4u << '=' << std::setw(8) << value;
                            if ((i & 3u) == 3u) std::cerr << '\n';
                        }
                        const std::uint32_t object_bytes =
                            std::min<std::uint32_t>(object_size, 0x600u);
                        if (object_offset <= openrac_guest_ram_size &&
                            object_bytes <= openrac_guest_ram_size - object_offset) {
                            for (std::uint32_t i = 0; i < object_bytes / 4u; ++i) {
                                std::uint32_t value = 0;
                                std::memcpy(&value,
                                            openrac_guest_base + object_offset + i * 4u, 4u);
                                std::cerr << "  object+" << std::setw(3) << std::setfill('0')
                                          << std::hex << i * 4u << '=' << std::setw(8) << value;
                                if ((i & 3u) == 3u) std::cerr << '\n';
                            }
                        }
                    }
                    std::cerr << std::dec << std::setfill(' ');
                }
                std::cerr
                          << " world_job_queue=0x" << std::hex
                          << world_job_queue[0] << ",0x" << world_job_queue[1]
                          << std::dec << " deferred_requests=" << deferred_request_count
                          << " raw_26f808_260=0x" << std::hex << scene_pending << std::dec
                          << " pad_routing=0x" << std::hex << frame_selector << std::dec
                          << " element_stack_state=" << element_stack_state
                          << " element_stack_state_started_at="
                          << element_stack_state_started_at
                          << " game_elapsed_time=" << game_elapsed_time
                          << " element_stack_resume_state=" << element_stack_resume_state
                          << " selected_pad_record=0x" << std::hex << state_data_address
                          << " pad_record_mode=" << std::dec << state_data_mode
                          << " pad_record_pressed=0x" << std::hex << state_data_flags
                          << " pad_routing_flags=0x" << state_transition_flags << std::dec
                          << " pad_report=0x" << std::hex << pad_report << std::dec
                          << " pad_mode=" << pad_mode << " pad_state=" << pad_state
                          << " pad_buttons=0x" << std::hex << pad_buttons
                          << " pad_pressed=0x" << pad_pressed << std::dec
                          << " screen_fade_mode=" << screen_fade_mode
                          << " screen_fade_active=" << screen_fade_request[0]
                          << ",key=0x" << std::hex << screen_fade_request[1]
                          << ",f08=0x" << screen_fade_request[2]
                          << ",fc=0x" << screen_fade_request[3]
                          << ",flags=0x" << screen_fade_request[4]
                          << " style=0x" << screen_fade_request[5] << std::dec
                          << " map_entry_raw_state="
                          << scene_pending_state << " pad_pressed_words=" << std::hex
                          << frame_flags_a << ',' << frame_flags_b << std::dec
                          << " elapsed=" << frame_elapsed
                          << " dispfb1=0x" << std::hex << dispfb1
                          << " display1=0x" << display1 << std::dec
                          << " vertices=" << std::dec << openrac_guest_gs_published_vertex_count()
                          << " image_tags=" << openrac_guest_gif_image_tag_count()
                          << " image_qwords=" << openrac_guest_gif_image_qword_count()
                          << " gif_chain_submits=" << openrac_native_renderer_dma_submit_count()
                          << " gif_chain_accepts=" << openrac_native_renderer_dma_accept_count()
                          << " gif_chain_rejects=" << openrac_native_renderer_dma_reject_count()
                          << " gif_chain_bytes=" << openrac_native_renderer_dma_bytes()
                          << " blit_writes=" << openrac_guest_gs_register_write_count(0x50u)
                          << " trxpos_writes=" << openrac_guest_gs_register_write_count(0x51u)
                          << " trxreg_writes=" << openrac_guest_gs_register_write_count(0x52u)
                          << " trxdir_writes=" << openrac_guest_gs_register_write_count(0x53u)
                          << std::dec;
                std::cerr << " active_threads=";
                for (uint32_t id = 0; id < 16; ++id) {
                    const gaddr active = openrac_guest_last_called_address_for_thread(id);
                    if (active == 0) continue;
                    const char* active_name = openrac_guest_function_name(active);
                    std::cerr << id << ':'
                              << (active_name == nullptr ? "unknown" : active_name)
                              << "(0x" << std::hex << active << std::dec << "),";
                }
                const size_t vertex_count = openrac_guest_gs_published_vertex_count();
                if (vertex_count != 0 && vertex_count <= (1u << 20)) {
                    std::vector<openrac_gs_vertex> vertices(vertex_count);
                    const size_t read_count = openrac_guest_gs_read_vertices(
                        vertices.data(), vertices.size());
                    float min_x = std::numeric_limits<float>::infinity();
                    float min_y = std::numeric_limits<float>::infinity();
                    float max_x = -std::numeric_limits<float>::infinity();
                    float max_y = -std::numeric_limits<float>::infinity();
                    size_t visible_vertices = 0;
                    size_t opaque_vertices = 0;
                    for (size_t i = 0; i < read_count; ++i) {
                        min_x = std::min(min_x, vertices[i].x);
                        min_y = std::min(min_y, vertices[i].y);
                        max_x = std::max(max_x, vertices[i].x);
                        max_y = std::max(max_y, vertices[i].y);
                        if (vertices[i].x >= 0.0f && vertices[i].x < 640.0f &&
                            vertices[i].y >= 0.0f && vertices[i].y < 224.0f)
                            ++visible_vertices;
                        if ((vertices[i].rgba >> 24u) != 0) ++opaque_vertices;
                    }
                    std::cerr << " vertex_bounds=" << min_x << ',' << min_y << '-'
                              << max_x << ',' << max_y
                              << " visible_vertices=" << visible_vertices
                              << " opaque_vertices=" << opaque_vertices;
                    std::cerr << " vertex_samples=";
                    for (size_t i = 0; i < std::min<size_t>(read_count, 3); ++i) {
                        if (i != 0) std::cerr << ';';
                        std::cerr << vertices[i].x << ',' << vertices[i].y << ":0x"
                                  << std::hex << vertices[i].rgba << std::dec;
                    }
                }
                std::cerr << '\n';
                std::cerr << "[ui trace] active_world=0x" << std::hex << active_world
                          << std::dec << " objects=" << ui_active_count
                          << " visible_text=" << ui_visible_count
                          << " fonts=" << ui_font_count << " font_registry="
                          << font_count << ",0x" << std::hex << font_array << ",0x"
                          << default_font << " ids=" << font_ids[0] << ','
                          << font_ids[1] << ',' << font_ids[2] << ',' << font_ids[3]
                          << " image_records_a=" << std::dec << image_count_a << ",0x"
                          << std::hex << image_records_a << " image_records_b="
                          << std::dec << image_count_b << ",0x" << std::hex
                          << image_records_b
                          << std::dec << " samples=";
                for (std::uint32_t i = 0; i < ui_sample_count; ++i) {
                    const UiSample& sample = ui_samples[i];
                    if (i != 0) std::cerr << ';';
                    std::cerr << sample.index << ":0x" << std::hex << sample.object
                              << "/mode=" << sample.mode << "/font=" << sample.font
                              << "/frame=" << sample.frame << "/draw=" << sample.draw
                              << "/font_vt=" << sample.font_vtable << "/adv="
                              << sample.font_advance << "/height=" << sample.font_height
                              << "/image=" << sample.font_image_a << ','
                              << sample.font_image_b << "/tex0=" << sample.font_tex0
                              << "/glyphs=" << sample.glyph_count << "/glyph_ptr="
                              << sample.glyphs << "/text_byte=" << sample.first_text_byte
                              << "/menu=" << sample.menu_title << ','
                              << sample.menu_count << ",0x" << sample.menu_items
                              << "/item0=" << sample.item0_index << ",0x"
                              << sample.item0_string << "/item_byte="
                              << sample.item0_text_byte << std::dec;
                }
                std::cerr << '\n';
                if (!wrote_ui_callback_snapshot && active_world != 0 && ui_font_count != 0 &&
                    std::getenv("OPENRAC_TRACE_UI_CALLBACKS") != nullptr) {
                    for (std::uint32_t i = 0; i < 37; ++i) {
                        std::uint32_t object = 0;
                        std::memcpy(&object, openrac_guest_base + 0x002486B0u + i * 4u,
                                    sizeof(object));
                        if (object == 0) continue;
                        const std::uint32_t object_physical =
                            openrac_guest_ram_address(object);
                        if (object_physical > openrac_guest_ram_size ||
                            0x264u > openrac_guest_ram_size - object_physical) continue;
                        std::uint32_t vtable = 0;
                        std::uint32_t mode = 0;
                        std::uint32_t page = 0;
                        std::memcpy(&vtable, openrac_guest_base + object_physical,
                                    sizeof(vtable));
                        std::memcpy(&mode, openrac_guest_base + object_physical + 0x250u,
                                    sizeof(mode));
                        std::memcpy(&page, openrac_guest_base + object_physical + 0x260u,
                                    sizeof(page));
                        std::uint32_t update = 0;
                        std::uint32_t draw = 0;
                        std::uint32_t state_264 = 0;
                        std::uint32_t state_268 = 0;
                        std::uint32_t state_26c = 0;
                        std::uint32_t state_270 = 0;
                        std::uint32_t state_274 = 0;
                        std::uint32_t state_278 = 0;
                        if (i == 17 &&
                            object_physical <= openrac_guest_ram_size &&
                            0x27Cu <= openrac_guest_ram_size - object_physical) {
                            std::memcpy(&state_264, openrac_guest_base + object_physical + 0x264u, 4u);
                            std::memcpy(&state_268, openrac_guest_base + object_physical + 0x268u, 4u);
                            std::memcpy(&state_26c, openrac_guest_base + object_physical + 0x26Cu, 4u);
                            std::memcpy(&state_270, openrac_guest_base + object_physical + 0x270u, 4u);
                            std::memcpy(&state_274, openrac_guest_base + object_physical + 0x274u, 4u);
                            std::memcpy(&state_278, openrac_guest_base + object_physical + 0x278u, 4u);
                        }
                        const std::uint32_t vtable_physical =
                            openrac_guest_ram_address(vtable);
                        if (vtable != 0 && vtable_physical <= openrac_guest_ram_size &&
                            0x28u <= openrac_guest_ram_size - vtable_physical) {
                            std::memcpy(&update, openrac_guest_base + vtable_physical + 8u,
                                        sizeof(update));
                            std::memcpy(&draw, openrac_guest_base + vtable_physical + 0x24u,
                                        sizeof(draw));
                        }
                        std::cerr << "[ui callback] index=" << i << " object=0x"
                                  << std::hex << object << " vtable=0x" << vtable
                                  << " update=0x" << update << '(';
                        if (update != 0) std::cerr << openrac_guest_function_name(update);
                        else std::cerr << "none";
                        std::cerr << ") draw=0x" << draw << '(';
                        if (draw != 0) std::cerr << openrac_guest_function_name(draw);
                        else std::cerr << "none";
                        std::cerr
                                  << ") mode=0x" << mode << " page=0x" << page
                                  << std::dec;
                        if (i == 17) {
                            std::cerr << " state264=0x" << std::hex << state_264
                                      << " state268=0x" << state_268
                                      << " state26c=0x" << state_26c
                                      << " state270=0x" << state_270
                                      << " state274=0x" << state_274
                                      << " state278=0x" << state_278 << std::dec;
                        }
                        std::cerr << '\n';
                    }
                    wrote_ui_callback_snapshot = true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }
    if (!openrac_native_window_create()) {
        trace_stop.store(true, std::memory_order_relaxed);
        if (trace_thread.joinable()) trace_thread.join();
        std::cerr << "could not create the SDL3 native presentation window: "
                  << SDL_GetError() << '\n';
        return 1;
    }

    std::atomic<bool> game_started{false};
    bool game_result = false;
    std::thread game_thread([&] {
        game_result = openrac_game_run();
        game_started.store(true, std::memory_order_release);
    });
    openrac::platform::FramePacer presenter(
        openrac::platform::field_rate(openrac::platform::VideoStandard::Ntsc));
    while (!game_started.load(std::memory_order_acquire)) {
        openrac_native_window_present();
        if (openrac_native_window_poll_input()) {
            trace_stop.store(true, std::memory_order_relaxed);
            if (trace_thread.joinable()) trace_thread.join();
            // Guest execution may still be inside translated retail code. Do
            // not detach it and then destroy process-wide guest maps while it
            // is running; terminate the process so the OS reclaims resources.
            std::_Exit(0);
        }
        presenter.wait();
    }
    game_thread.join();
    openrac_native_window_destroy();
    if (!game_result) {
        trace_stop.store(true, std::memory_order_relaxed);
        if (trace_thread.joinable()) trace_thread.join();
        std::cerr << "the Sly 1 entry point is not available in this build\n";
        return 1;
    }
    trace_stop.store(true, std::memory_order_relaxed);
    if (trace_thread.joinable()) trace_thread.join();
    openrac_guest_report_missing();
    if (openrac_guest_exit_requested()) {
        std::cerr << "Sly 1 requested guest Exit; the native frame loop is not active yet\n";
    } else {
        std::cerr << "Sly 1 entry returned without requesting guest Exit\n";
    }
    return 0;
}
