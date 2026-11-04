// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native transform walk used while preparing Sly draw entries.
// The retail path uses VU0 vector instructions; this implementation keeps
// their observable vector outputs in ordinary host floats.

extern "C" {
void func_001280C0(void* first_transform, void* second_transform,
                   const f32* input, f32* output);
}

namespace {

f32 load_f32(const void* base, u32 offset) {
    return *reinterpret_cast<const f32*>(static_cast<const u8*>(base) + offset);
}

void add4(f32* dst, const f32* src) {
    for (u32 lane = 0; lane < 4; ++lane) dst[lane] += src[lane];
}

void cross3(const f32* a, const f32* b, f32* out) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
    out[3] = 0.0f;
}

}  // namespace

extern "C" s32 func_001278A8(void* object, void* base, const f32* input,
                              f32* first_axis, f32* translation_out,
                              f32* second_axis, f32* rotation_out) {
    if (first_axis != nullptr) {
        for (u32 lane = 0; lane < 4; ++lane) first_axis[lane] = 0.0f;
    }
    if (second_axis != nullptr) {
        for (u32 lane = 0; lane < 4; ++lane) second_axis[lane] = 0.0f;
    }

    void* chain[24];
    u32 count = 0;
    void* current = object;
    while (current != base && current != nullptr && count < 24) {
        chain[count++] = current;
        auto* bytes = static_cast<u8*>(current);
        void* attachment = *reinterpret_cast<void**>(bytes + 0x224);
        if (attachment != nullptr &&
            ((*reinterpret_cast<const u32*>(static_cast<const u8*>(attachment) + 0xB0) & 0xC) != 0)) {
            current = *reinterpret_cast<void**>(static_cast<u8*>(attachment) + 0x60);
        } else {
            current = *reinterpret_cast<void**>(bytes + 0x18);
        }
    }

    f32 translation[4] = {};
    f32 rotation[4] = {};
    void* previous = nullptr;
    for (u32 i = count; i != 0; --i) {
        void* node = chain[i - 1];
        auto* bytes = static_cast<u8*>(node);
        f32 local[4];
        f32 transformed[4];

        if (first_axis != nullptr || translation_out != nullptr || second_axis != nullptr) {
            for (u32 lane = 0; lane < 4; ++lane) local[lane] = load_f32(node, 0x160 + lane * 4);
            func_001280C0(previous, base, local, transformed);
            add4(translation, transformed);
        }
        if (rotation_out != nullptr || second_axis != nullptr) {
            for (u32 lane = 0; lane < 4; ++lane) local[lane] = load_f32(node, 0x180 + lane * 4);
            func_001280C0(previous, base, local, transformed);
            add4(rotation, transformed);
        }

        if (first_axis != nullptr || second_axis != nullptr) {
            f32 parent_position[4];
            if (previous != nullptr) {
                for (u32 lane = 0; lane < 4; ++lane) {
                    parent_position[lane] = load_f32(previous, 0x140 + lane * 4);
                }
            } else {
                for (u32 lane = 0; lane < 4; ++lane) parent_position[lane] = input[lane];
            }
            f32 relative[4];
            for (u32 lane = 0; lane < 4; ++lane) {
                relative[lane] = parent_position[lane] - load_f32(node, 0x140 + lane * 4);
            }
            func_001280C0(nullptr, base, relative, transformed);

            f32 normal[4];
            cross3(translation, transformed, normal);
            if (first_axis != nullptr) add4(first_axis, normal);

            if (first_axis != nullptr) {
                for (u32 lane = 0; lane < 4; ++lane) local[lane] = load_f32(node, 0x150 + lane * 4);
                func_001280C0(previous, base, local, transformed);
                add4(first_axis, transformed);
            }

            if (second_axis != nullptr) {
                f32 rotation_normal[4];
                cross3(rotation, relative, rotation_normal);
                add4(second_axis, normal);
                add4(second_axis, rotation_normal);
                for (u32 lane = 0; lane < 4; ++lane) local[lane] = load_f32(node, 0x170 + lane * 4);
                func_001280C0(previous, base, local, transformed);
                add4(second_axis, transformed);
            }
        }
        previous = node;
    }

    if (translation_out != nullptr) {
        for (u32 lane = 0; lane < 4; ++lane) translation_out[lane] = translation[lane];
    }
    if (rotation_out != nullptr) {
        for (u32 lane = 0; lane < 4; ++lane) rotation_out[lane] = rotation[lane];
    }
    return static_cast<s32>(count);
}
