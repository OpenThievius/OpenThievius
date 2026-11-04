// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native C++ translation of Sly's frame-state dispatcher.
extern "C" {
extern u32 D_00262C68;
extern u32 D_00262C6C;
extern unsigned char D_00262C70[];
extern unsigned char D_00262D38[];
extern unsigned char D_00261858[];
extern unsigned char D_00264A40[];
extern void* D_002701B8;
extern unsigned char D_00275F80[];
extern void* D_002623C0;
extern unsigned char D_0026FF68[];
extern unsigned char D_0026F808[];
extern void* D_00275710;
}

void func_0016E118(s32 mask);
void* func_001F4378(void* object);
void func_001F4808(void* resource);
void func_0016EC20(void* state, s32 mask);
void func_0018C568();
void func_001605E8(void* object);
void func_00160948();
void func_00194D30(void* object, s32 first, s32 count);
void func_001E9498(void* object);
void func_001E96F8(void* object, s32 state);
void func_001E98E8(float amount);
void func_001409E0();
void func_001A9FD0();

namespace {

template <typename T>
T& at(unsigned char* base, s32 offset) {
    return *reinterpret_cast<T*>(base + offset);
}

}  // namespace

void func_001E9108(void* frame_state) {
    auto* object = static_cast<unsigned char*>(frame_state);
    auto* state_data = (D_00262C68 & 4) != 0 ? D_00262C70 : D_00262D38;
    s32 state = at<s32>(object, 4);
    const float elapsed = at<float>(D_00261858, 0x10) - at<float>(object, 8);
    bool repeat_transition = false;

    if ((D_00262C6C & 0x80) != 0 && at<float>(D_00261858, 4) > 0.1f) {
        func_0016E118(0x80);
    }

    switch (state) {
        case 0: {
            bool should_present = false;
            if (D_002701B8 == nullptr) {
                const s32 mode = at<s32>(D_00264A40, 0x18);
                should_present = mode == 2 ||
                    (mode == 1 && at<s32>(D_00264A40, 0x0c) == 5);
            }
            if (should_present) {
                func_001F4808(func_001F4378(frame_state));
            }
            break;
        }

        case 1: {
            const u16 flags = at<u16>(state_data, 0xaa);
            if ((flags & 0x800) != 0) {
                func_0016EC20(state_data, 0x800);
                const s32 pending = at<s32>(D_00264A40, 0x4f48);
                if (pending != 0) {
                    at<s32>(D_00264A40, 0x4f44) = pending;
                    func_0018C568();
                } else {
                    func_001605E8(D_002623C0);
                }
                func_00160948();
                state = at<s32>(object, 4);
            } else if ((flags & 0x100) != 0) {
                func_0016EC20(state_data, 0x100);
                func_00194D30(D_0026FF68, 0, 0x17);
                state = at<s32>(object, 4);
            }
            break;
        }

        case 2:
        case 5:
        case 7:
            repeat_transition = true;
            break;

        case 3: {
            if ((D_00262C6C & 2) != 0 || at<s32>(D_00275F80, 4) != 0) {
                break;
            }

            const u16 flags = at<u16>(state_data, 0xaa);
            bool reset_scene = (flags & 0x800) != 0;
            if (!reset_scene && at<s32>(state_data, 0x14) == 1) {
                reset_scene = at<float>(D_00261858, 4) >= 5.0f;
            }
            if (reset_scene) {
                func_00194D30(D_0026FF68, 0, 0);
                state = at<s32>(object, 4);
                func_0016EC20(state_data, 0x800);
            } else if ((flags & 0x100) != 0) {
                void* pending = at<void*>(D_0026F808, 0x260);
                if (pending != nullptr && at<s32>(static_cast<unsigned char*>(pending), 0x2d0) == 0) {
                    func_001E9498(frame_state);
                    state = at<s32>(object, 4);
                    func_0016EC20(state_data, 0x100);
                }
            }
            break;
        }

        case 4:
            if (elapsed <= 0.2f) {
                func_001E98E8(5.0f * (0.2f - elapsed));
            } else {
                state = 5;
            }
            break;

        case 6:
            if (elapsed <= 0.5f) {
                func_001E98E8(elapsed + elapsed);
            } else {
                state = 3;
            }
            break;

        default:
            break;
    }

    func_001E96F8(frame_state, state);

    const s32 count = at<s32>(object, 0x2c);
    void* item = at<void*>(object, 8 + count * 4);
    if (item != nullptr) {
        auto** vtable = *reinterpret_cast<void***>(item);
        using StateCallback = void (*)(void*, void*);
        auto callback = reinterpret_cast<StateCallback>(vtable[0x10 / sizeof(void*)]);
        if (callback != nullptr) callback(item, state_data);
    } else if (repeat_transition) {
        func_001E96F8(frame_state, 3);
    }

    func_001409E0();
    func_001A9FD0();
}
