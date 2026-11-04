// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native C++ translation of Sly's persistent frame loop.
extern "C" {
extern void* D_00264834;
extern void* D_00264838;
extern u32 D_002622CC;
extern void* D_00275AF0;
extern void* D_00275710;
extern void* D_00261990;
extern unsigned char D_00275BC0[];
extern unsigned char D_00269A00[];
extern unsigned char D_00275F80[];
extern unsigned char D_00262C70[];
extern unsigned char D_00261858[];
extern unsigned char D_00264A40[];
}

void func_001FAE18();
void func_00185B38();
void func_0015F210(int mode);
void func_0018F148(void* object);
void func_001E4EA8(void* resource_state);
void func_0016E238(void* object);
void func_0016F2E0();
void func_0018BDC8(void* object);
void func_001E9108(void* frame_state);
void func_001607D0(float delta);
void func_00144270(void* object);
void func_0015E9F0();
void func_00142F10(void* timer);
void func_0019D6C0(void* object, void* player);
void func_001E9668();
void func_0019DC98(void* object, void* player);
void func_001E9648();
void func_0015EC68();

namespace {

template <typename T>
T& at(unsigned char* base, u32 offset) {
    return *reinterpret_cast<T*>(base + offset);
}

}  // namespace

void func_00185758(void* game_state, void* secondary_state) {
    func_001FAE18();
    D_00264834 = game_state;
    D_00264838 = secondary_state;
    func_00185B38();

    // Retail loops continuously; the frame dispatcher updates the state and
    // the timer tick advances at the end of each iteration.
    for (;;) {
        if (at<s32>(D_00269A00, 4) != 0 &&
            at<s32>(D_00275F80, 4) != 1) {
            func_0015F210(1);
            func_0018F148(D_00269A00);
        }

        if (D_00275AF0 != nullptr) {
            func_0015F210(1);
            func_001E4EA8(&D_00275AF0);
        }

        if (at<s32>(D_00269A00, 4) != 0 &&
            at<s32>(D_00275F80, 4) != 1) {
            func_0015F210(1);
            func_0018F148(D_00269A00);
        }

        func_0016E238(D_00262C70);
        func_0016F2E0();
        func_0018BDC8(D_00264A40);
        func_001E9108(D_00275BC0);

        const float delta = at<float>(D_00261858, 8);
        func_001607D0(delta);

        if (D_00275710 != nullptr) {
            func_00144270(D_00261990);
            func_0015E9F0();
            func_00142F10(D_00261858);

            auto** vtable = *reinterpret_cast<void***>(D_00275710);
            using UpdateCallback = void (*)(void*, void*, float);
            auto update = reinterpret_cast<UpdateCallback>(vtable[0x54 / sizeof(void*)]);
            const float callback_delta = at<float>(D_00261858, 8);
            if (update != nullptr) update(D_00275710, D_00261990, callback_delta);

            func_0019D6C0(D_00275710, D_00261990);
            func_001E9668();
            func_0019DC98(D_00275710, D_00261990);
            func_001E9648();
            func_0015EC68();
        }

        ++D_002622CC;
    }
}
