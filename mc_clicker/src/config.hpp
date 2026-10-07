#pragma once
#include "util.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>

struct RandomizationSettings {
    int mode = 1;                 // Off, Normal, Extra, Extra+
    float strength = 0.30f;
    float normal_variation = 0.50f;
    float extra_variation = 0.60f;
    float extra_drift = 0.40f;
    float extra_drift_period_ms = 2827.0f;
    float plus_variation = 0.65f;
    float plus_drift = 0.45f;
    float plus_drift_period_ms = 2199.0f;
    float plus_dip_chance = 0.10f;
    float plus_dip_depth = 0.50f;

    void clamp() {
        mode = std::clamp(mode, 0, 3);
        strength = std::clamp(strength, 0.0f, 1.0f);
        normal_variation = std::clamp(normal_variation, 0.0f, 2.0f);
        extra_variation = std::clamp(extra_variation, 0.0f, 2.0f);
        extra_drift = std::clamp(extra_drift, 0.0f, 2.0f);
        extra_drift_period_ms = std::clamp(extra_drift_period_ms, 200.0f, 10000.0f);
        plus_variation = std::clamp(plus_variation, 0.0f, 2.0f);
        plus_drift = std::clamp(plus_drift, 0.0f, 2.0f);
        plus_drift_period_ms = std::clamp(plus_drift_period_ms, 200.0f, 10000.0f);
        plus_dip_chance = std::clamp(plus_dip_chance, 0.0f, 1.0f);
        plus_dip_depth = std::clamp(plus_dip_depth, 0.0f, 2.0f);
    }
};

struct Config {
    // Left Clicker
    bool     left_enabled              = true;
    float    min_cps                   = 9.5f;
    float    max_cps                   = 13.5f;
    float    humanize_strength         = 0.65f;  // Legacy config migration only
    float    random_strength           = 0.30f;  // Legacy config migration only
    RandomizationSettings left_randomization;
    bool     weapon_only               = true;
    uint32_t weapon_slot_mask          = 0x03; // User-marked allowed hotbar slots
    bool     require_lmb_hold          = true; // Click Only: hold LMB on a marked slot

    // Right Clicker
    bool  right_enabled             = false;
    float right_min_cps             = 10.0f;
    float right_max_cps             = 14.0f;
    float right_humanize_strength   = 0.50f; // Legacy config migration only
    RandomizationSettings right_randomization;
    bool  require_rmb_hold          = true;
    bool  blockhit                  = false;
    float blockhit_duration_ms      = 85.0f; // minimum block hold before another pulse
    float blockhit_chance           = 0.35f;
    float blockhit_attack_delay_ms  = 4.0f;
    float blockhit_cooldown_ms      = 120.0f;

    // Keybinds
    int   toggle_vk                 = VK_F6;
    int   right_toggle_vk           = VK_F7;
    int   destruct_vk               = VK_DELETE;

    // Common Gating & System
    bool  pause_on_gui              = true; // Strict in-game gating
    bool  toggle_only_when_active   = true;
    bool  clean_on_exit             = true;
    bool  self_delete_exe           = false;
    float ui_opacity                = 0.95f;
    bool  show_overlay              = true;

    std::vector<std::wstring> process_whitelist = {
        L"javaw.exe", L"java.exe",
        L"lunar client.exe", L"badlion client.exe",
        L"feather.exe", L"feather client.exe",
        L"minecraft.exe", L"prism.exe"
    };

    void clamp() {
        min_cps = std::clamp(min_cps, 1.0f, 20.0f);
        max_cps = std::clamp(max_cps, min_cps, 20.0f);
        humanize_strength = std::clamp(humanize_strength, 0.0f, 1.0f);
        random_strength = std::clamp(random_strength, 0.0f, 1.0f);
        left_randomization.clamp();
        right_randomization.clamp();

        right_min_cps = std::clamp(right_min_cps, 1.0f, 25.0f);
        right_max_cps = std::clamp(right_max_cps, right_min_cps, 25.0f);
        right_humanize_strength = std::clamp(right_humanize_strength, 0.0f, 1.0f);

        blockhit_duration_ms = std::clamp(blockhit_duration_ms, 30.0f, 200.0f);
        blockhit_chance = std::clamp(blockhit_chance, 0.0f, 1.0f);
        blockhit_attack_delay_ms = std::clamp(blockhit_attack_delay_ms, 0.0f, 25.0f);
        blockhit_cooldown_ms = std::clamp(blockhit_cooldown_ms, 30.0f, 500.0f);
        ui_opacity = std::clamp(ui_opacity, 0.2f, 1.0f);
    }


    bool load(const std::filesystem::path& p);
    bool save(const std::filesystem::path& p) const;
};
