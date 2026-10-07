#include "memory.hpp"
#include "input.hpp"

bool ProcessMemory::attach(const std::vector<std::wstring>&) {
    // External mode has no JVM inventory access.
    attached_ = false;
    return false;
}

void ProcessMemory::detach() {
    attached_ = false;
}


void ProcessMemory::update_hotbar() {
    // Accumulate fractional wheel movement. Do not turn a partial notch into
    // a full slot change, and do not guess a starting slot from the wheel.
    wheel_remainder_ += input::consume_wheel_delta();
    const int steps = wheel_remainder_ / WHEEL_DELTA;
    wheel_remainder_ %= WHEEL_DELTA;
    if (steps != 0 && active_slot_.load(std::memory_order_relaxed) != 0) {
        // Scroll down (negative) advances slot (+1)
        // Scroll up (positive) moves slot backward (-1)
        int new_slot = active_slot_.load(std::memory_order_relaxed) - steps;
        while (new_slot < 1) new_slot += 9;
        while (new_slot > 9) new_slot -= 9;
        active_slot_.store(new_slot, std::memory_order_relaxed);
    }

    // Detect key-down edges instead of polling the shared low transition bit.
    for (int index = 0; index < 18; ++index) {
        const int key = index < 9 ? '1' + index : VK_NUMPAD1 + index - 9;
        const bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
        if (down && !keys_down_[index]) {
            active_slot_.store(index % 9 + 1, std::memory_order_relaxed);
        }
        keys_down_[index] = down;
    }
}

void ProcessMemory::reset_hotbar_input() {
    input::consume_wheel_delta();
    wheel_remainder_ = 0;
    for (int index = 0; index < 18; ++index) {
        const int key = index < 9 ? '1' + index : VK_NUMPAD1 + index - 9;
        keys_down_[index] = (GetAsyncKeyState(key) & 0x8000) != 0;
    }
}

bool ProcessMemory::is_slot_allowed(uint32_t weapon_slot_mask) const {
    const int active_slot = active_slot_.load(std::memory_order_relaxed);
    if (active_slot < 1 || active_slot > 9)
        return false;
    return (weapon_slot_mask & (1 << (active_slot - 1))) != 0;
}

bool ProcessMemory::is_ingame_minecraft(HWND* out_hwnd) {
    HWND fg = GetForegroundWindow();
    if (!fg) return false;

    // Cache window identification to avoid hammering Win32 APIs
    static HWND s_last_fg = nullptr;
    static bool s_last_is_mc = false;
    static uint64_t s_last_fg_check = 0;

    const uint64_t now = util::qpc();
    bool is_mc = false;

    if (fg == s_last_fg && util::ticks_to_ms(now - s_last_fg_check) < 300.0) {
        is_mc = s_last_is_mc;
    } else {
        s_last_fg = fg;
        s_last_fg_check = now;

        // 1. Must be Minecraft 1.8.9 (LWJGL 2 display class is ALWAYS "LWJGL")
        wchar_t cls[64]{};
        if (GetClassNameW(fg, cls, 64)) {
            if (_wcsicmp(cls, L"LWJGL") == 0)
                is_mc = true;
        }

        // Fast title fallback
        if (!is_mc) {
            wchar_t title[128]{};
            if (GetWindowTextW(fg, title, 128)) {
                std::wstring lower_title = util::to_lower(title);
                if (lower_title.find(L"minecraft") != std::wstring::npos ||
                    lower_title.find(L"lunar") != std::wstring::npos ||
                    lower_title.find(L"badlion") != std::wstring::npos ||
                    lower_title.find(L"feather") != std::wstring::npos ||
                    lower_title.find(L"labymod") != std::wstring::npos ||
                    lower_title.find(L"cheatbreaker") != std::wstring::npos ||
                    lower_title.find(L"pvp lounge") != std::wstring::npos) {
                    is_mc = true;
                }
            }
        }
        s_last_is_mc = is_mc;
    }

    if (!is_mc) return false;
    if (IsIconic(fg)) return false;

    if (out_hwnd) *out_hwnd = fg;

    // 2. In-game play check (cursor grabbed vs open inventory / chat / pause menu)
    CURSORINFO ci{ sizeof(ci) };
    if (!GetCursorInfo(&ci))
        return true;

    // If cursor is completely hidden or null handle, 100% in-game grabbed mode
    if (!(ci.flags & CURSOR_SHOWING) || ci.hCursor == nullptr)
        return true;

    // Fast static screen metrics (doesn't change unless monitor setup changes)
    static int s_vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    static int s_vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    static int s_vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    static int s_vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    // Check if cursor is clipped to the game window
    // In LWJGL 2, Mouse.setGrabbed(true) calls ClipCursor(&rect).
    // In menus/inventory/chat, Mouse.setGrabbed(false) calls ClipCursor(NULL).
    RECT clip{};
    if (GetClipCursor(&clip)) {
        bool is_clipped = (clip.left > s_vx || clip.top > s_vy || clip.right < (s_vx + s_vw) || clip.bottom < (s_vy + s_vh));
        if (is_clipped) {
            return true;
        }
    }

    // In LWJGL 2, when mouse is grabbed in-game, it sets a custom blank cursor.
    // When inventory/chat/pause menu is opened, standard IDC_ARROW (or IDC_IBEAM for chat typing) is used.
    static HCURSOR hArrow = LoadCursor(NULL, IDC_ARROW);
    static HCURSOR hIBeam = LoadCursor(NULL, IDC_IBEAM);
    if (ci.hCursor != hArrow && ci.hCursor != hIBeam) {
        return true; // Blank or custom in-game cursor -> in-game!
    }

    // Standard arrow or ibeam cursor visible + not clipped = definitely in a menu/GUI.
    // No fallback heuristics — this prevents false-positives on main menu, multiplayer screen, etc.
    return false;
}

