// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native implementation of Sly's draw-entry preparation routine.
// Kept out of sly1-decomp so matching source can replace it directly.
struct SlyDrawPart {
    u32 source;
    u32 transform;
    u32 object_flag;
    u32 reserved_0c;
    f32 input_position[4];         // 0x10
    f32 local_position[4];         // 0x20
    f32 reserved_vector[4];        // 0x30
};

struct SlyDrawEntry {
    SlyDrawPart parts[2];
    f32 basis[4];                  // 0x80
    f32 weight;                    // 0x90
    f32 unclamped_weight;          // 0x94
    f32 clamped_weight;            // 0x98
    u32 reserved_9c;
    u32 mode;                      // 0xa0
    u32 callback_data;             // 0xa4
    u32 reserved_a8[10];
    u32 tail;                      // 0xd0
    u32 reserved_d4[11];
};

static_assert(sizeof(SlyDrawEntry) == 0x100);
static_assert(sizeof(SlyDrawPart) == 0x40);
static_assert(__builtin_offsetof(SlyDrawPart, input_position) == 0x10);
static_assert(__builtin_offsetof(SlyDrawPart, local_position) == 0x20);
static_assert(__builtin_offsetof(SlyDrawPart, reserved_vector) == 0x30);
static_assert(__builtin_offsetof(SlyDrawEntry, basis) == 0x80);
static_assert(__builtin_offsetof(SlyDrawEntry, weight) == 0x90);
static_assert(__builtin_offsetof(SlyDrawEntry, tail) == 0xd0);

struct SlyIndexPair {
    s32 first;
    s32 second;
};

float func_00185C18(const SlyIndexPair* indices);
float func_00185CA0(const SlyIndexPair* indices);
void* func_001B5EE0(void* pool);
void* func_001BB540(void* object, s32 selector);
int func_00128018(void* first_transform, void* second_transform,
                  const void* input, void* output);
void func_001860A8(void* queue, s32 mode, void** entry);

extern "C" void func_00185D20(f32 weight, void* owner, void* alternate,
                                const f32* input_position, const f32* basis,
                                s32 mode, s32 callback_data,
                                s32 tail, void** entry_cursor) {
    auto* pool = *reinterpret_cast<unsigned char**>(
        static_cast<unsigned char*>(owner) + 0x14);
    auto* entry = static_cast<SlyDrawEntry*>(func_001B5EE0(pool + 0x1afc));
    auto* alternate_bytes = static_cast<unsigned char*>(alternate);

    entry->weight = weight;
    for (unsigned i = 0; i < 4; ++i) {
        entry->parts[0].input_position[i] = input_position[i];
        entry->parts[1].input_position[i] = input_position[i] - basis[i] * weight;
        entry->basis[i] = basis[i];
    }

    const SlyIndexPair indices{callback_data, tail};
    entry->unclamped_weight = func_00185C18(&indices);
    entry->clamped_weight = func_00185CA0(&indices);
    entry->mode = static_cast<u32>(callback_data);
    entry->callback_data = static_cast<u32>(tail);
    entry->reserved_9c = static_cast<u32>(mode);
    entry->tail = reinterpret_cast<u32>(*entry_cursor);

    for (unsigned i = 0; i < 2; ++i) {
        void* transform = i == 0 ? owner : alternate;
        auto& part = entry->parts[i];
        part.transform = reinterpret_cast<u32>(transform);

        if (transform != nullptr) {
            const auto* transform_bytes = static_cast<const unsigned char*>(transform);
            part.source = *reinterpret_cast<const u32*>(transform_bytes + 0x50);
            part.object_flag = static_cast<u32>(
                *reinterpret_cast<const s8*>(transform_bytes + 0x2c8));
            (void)func_00128018(nullptr, transform, part.input_position,
                                part.local_position);
        } else {
            part.source = 0;
            part.object_flag = 0;
            for (unsigned lane = 0; lane < 4; ++lane) {
                part.local_position[lane] = part.input_position[lane];
            }
        }

        void* callback_object = transform != nullptr
                                    ? func_001BB540(transform, 0x100)
                                    : nullptr;
        if (callback_object != nullptr) {
            auto** vtable = *reinterpret_cast<void***>(callback_object);
            using DrawCallback = void (*)(void*, void*, u32);
            auto callback = reinterpret_cast<DrawCallback>(vtable[0x100 / sizeof(void*)]);
            if (callback != nullptr) callback(callback_object, entry, i);
        }
    }

    func_001860A8(pool, 1, reinterpret_cast<void**>(&entry));
    *entry_cursor = entry;
}
