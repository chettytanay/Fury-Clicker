#pragma once
#include "util.hpp"
#include <optional>
#include <TlHelp32.h>
#include <vector>
#include <string>
#include <atomic>

class ProcessMemory {
public:
    bool attach(const std::vector<std::wstring>& names);
    void detach();
    bool is_attached() const { return attached_; }

    // External hotbar estimate from keyboard and physical mouse wheel.
    void update_hotbar();
    void reset_hotbar_input();
    int active_slot() const { return active_slot_.load(std::memory_order_relaxed); }
    void set_active_slot(int s) { if (s >= 1 && s <= 9) active_slot_.store(s, std::memory_order_relaxed); }
    bool is_slot_allowed(uint32_t weapon_slot_mask) const;

    // Strict in-game Minecraft 1.8.9 check (verifies 1.8.9 window + cursor grabbed, not in GUI/menus)
    static bool is_ingame_minecraft(HWND* out_hwnd = nullptr);

private:
    bool attached_ = false;
    std::atomic<int> active_slot_{0}; // Unknown until a hotbar key is observed
    int wheel_remainder_ = 0;
    bool keys_down_[18]{};
};
