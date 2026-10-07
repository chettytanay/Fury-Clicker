#pragma once
#include "util.hpp"
#include <vector>
#include <string>

namespace input {

// A dedicated Raw Input message thread tracks hardware independently of SendInput.
bool start_mouse_tracking();
void stop_mouse_tracking();
bool is_lmb_down();
bool is_rmb_down();
int consume_wheel_delta();

inline bool left_down() {
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    return SendInput(1, &in, sizeof(INPUT)) == 1;
}

inline bool left_up() {
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    return SendInput(1, &in, sizeof(INPUT)) == 1;
}

inline bool right_down() {
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
    return SendInput(1, &in, sizeof(INPUT)) == 1;
}

inline bool right_up() {
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
    return SendInput(1, &in, sizeof(INPUT)) == 1;
}

inline bool is_key_down(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

bool foreground_is_minecraft(const std::vector<std::wstring>& whitelist);

} // namespace input
