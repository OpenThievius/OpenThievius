// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native C++ translation of Sly's frame-state transition helper.
extern "C" {
extern unsigned char D_00261858[];
extern unsigned char D_00264A40[];
extern void* D_00272930;
extern void* D_00272B98;
extern void* D_00275710;
extern unsigned char D_00275F80[];
extern s32 D_00274738;
extern void* D_0026C6C8;
extern void* D_0026C948;
extern void* D_0026CBC8;
extern void* D_0026CE48;
}

void func_0018C178(void* object, s32 value);
void func_001F0220(void* object, s32 value);
void func_001DDA50(void* object);
void func_001DDA20(void* object);
void func_001A7E70();
void func_001E98E8(float amount);

namespace {

template <typename T>
T& at(unsigned char* base, s32 offset) {
    return *reinterpret_cast<T*>(base + offset);
}

void call_holder_method(void** holder, s32 byte_offset) {
    auto* object = static_cast<unsigned char*>(*holder);
    using Method = void (*)(void*);
    auto method = *reinterpret_cast<Method*>(object + byte_offset);
    if (method != nullptr) method(holder);
}

}  // namespace

void func_001E96F8(void* frame_state, s32 requested_state) {
    auto* object = static_cast<unsigned char*>(frame_state);
    s32 state = requested_state;
    if (state == 3 && at<s32>(object, 0x30) != -1) {
        state = at<s32>(object, 0x30);
    }

    const s32 previous = at<s32>(object, 4);
    if (previous == state) return;

    if (previous == 1) {
        call_holder_method(&D_00272930, 0x3c);
        call_holder_method(&D_00272B98, 0x3c);
        func_001DDA50(D_00275710);
    }

    at<s32>(object, 4) = state;
    at<float>(object, 8) = at<float>(D_00261858, 0x10);
    if (static_cast<u32>(state) >= 9) return;

    switch (state) {
        case 0:
            if (at<s32>(D_00264A40, 0x18) == 0) {
                func_0018C178(D_00264A40, 1);
            }
            func_001F0220(D_00275F80, 0);
            func_001E98E8(1.0f);
            break;

        case 1:
            func_001E98E8(1.0f);
            call_holder_method(&D_00272930, 0x38);
            call_holder_method(&D_00272B98, 0x38);
            func_001DDA20(D_00275710);
            func_001A7E70();
            break;

        case 2:
            func_001E98E8(1.0f);
            break;

        case 3:
            func_001E98E8(1.0f);
            D_00274738 = -100;
            break;

        case 4:
        case 5:
        case 6:
        case 7:
            func_001E98E8(0.0f);
            break;

        case 8:
            call_holder_method(&D_0026C6C8, 0x3c);
            call_holder_method(&D_0026C948, 0x3c);
            call_holder_method(&D_0026CBC8, 0x3c);
            call_holder_method(&D_0026CE48, 0x3c);
            break;
    }
}
