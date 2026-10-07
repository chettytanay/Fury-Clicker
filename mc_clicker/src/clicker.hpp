#pragma once
#include "config.hpp"
#include "memory.hpp"
#include "input.hpp"
#include "click_scheduler.hpp"
#include "slot_cps_tracker.hpp"
#include "click_rate_window.hpp"
#include "left_activation.hpp"
#include <thread>
#include <atomic>
#include <vector>
#include <random>
#include <mutex>

class Clicker {
public:
    enum class ActivationState : int {
        NeverSeen, Menu, Disabled, WaitingForHold, SlotUnknown,
        SlotBlocked, Ready
    };
    explicit Clicker(Config& cfg);
    ~Clicker();

    void start();
    void stop();

    void toggle_left();
    void toggle_right();
    void sync_config();

    bool left_enabled() const { return cfg_.left_enabled; }
    bool right_enabled() const { return cfg_.right_enabled; }

    float live_left_cps() const { return left_rate_.recent_cps(util::qpc()); }
    float live_right_cps() const { return right_rate_.recent_cps(util::qpc()); }
    float peak_left_cps() const { return left_rate_.peak_cps(); }
    float peak_right_cps() const { return right_rate_.peak_cps(); }
    uint64_t input_failures() const { return input_failures_.load(); }
    uint64_t left_due_count() const { return left_due_count_.load(); }
    uint64_t right_due_count() const { return right_due_count_.load(); }
    ActivationState last_left_state() const { return last_left_state_.load(); }
    ActivationState last_right_state() const { return last_right_state_.load(); }

    uint64_t total_left_clicks() const { return total_left_clicks_.load(); }
    uint64_t total_right_clicks() const { return total_right_clicks_.load(); }
    uint64_t total_blockhits() const { return total_blockhits_.load(); }

    bool is_minecraft_focused() const { return is_mc_focused_.load(); }
    bool is_minecraft_ingame() const { return is_mc_ingame_.load(); }
    int  active_hotbar_slot() const { return mem_.active_slot(); }
    void set_active_hotbar_slot(int s) { mem_.set_active_slot(s); }
    bool is_slot_allowed() const { return mem_.is_slot_allowed(cfg_.weapon_slot_mask); }
    float active_slot_cps() const { return slot_cps_.cps(mem_.active_slot(), util::qpc()); }
    uint64_t active_slot_clicks() const { return slot_cps_.total(mem_.active_slot()); }
    bool is_memory_attached() const;

    void self_destruct();

private:
    struct RuntimeSettings {
        bool left_enabled = false;
        float min_cps = 1.0f;
        float max_cps = 1.0f;
        RandomizationSettings left_randomization;
        bool weapon_only = false;
        uint32_t weapon_slot_mask = 0;
        bool require_lmb_hold = false;
        bool right_enabled = false;
        float right_min_cps = 1.0f;
        float right_max_cps = 1.0f;
        RandomizationSettings right_randomization;
        bool require_rmb_hold = false;
        bool blockhit = false;
        float blockhit_duration_ms = 0.0f;
        float blockhit_chance = 0.0f;
        float blockhit_attack_delay_ms = 0.0f;
        float blockhit_cooldown_ms = 0.0f;
        bool pause_on_gui = true;
    };

    RuntimeSettings settings_snapshot() const;

    void left_worker();
    void right_worker();
    void supervisor_worker();

    void fire_left_click();
    void fire_right_click(float hold_ms = 0.0f, bool count_click = true);
    void fire_blockhit_click(const RuntimeSettings& settings);
    void release_left();
    void release_right();
    bool pulse_context_valid() const;
    void wait_for_due(const ClickScheduler& scheduler);

    Config& cfg_;
    ProcessMemory mem_;
    SlotCpsTracker slot_cps_;
    ClickRateWindow left_rate_;
    ClickRateWindow right_rate_;

    std::thread left_th_;
    std::thread right_th_;
    std::thread sup_th_;

    std::atomic<bool> running_{false};
    std::atomic<bool> is_mc_focused_{false};
    std::atomic<bool> is_mc_ingame_{false};

    std::atomic<uint64_t> total_left_clicks_{0};
    std::atomic<uint64_t> total_right_clicks_{0};
    std::atomic<uint64_t> total_blockhits_{0};
    std::atomic<uint64_t> input_failures_{0};
    std::atomic<uint64_t> left_due_count_{0};
    std::atomic<uint64_t> right_due_count_{0};
    std::atomic<ActivationState> last_left_state_{ActivationState::NeverSeen};
    std::atomic<ActivationState> last_right_state_{ActivationState::NeverSeen};

    ClickScheduler left_scheduler_;
    ClickScheduler right_scheduler_;

    std::mutex left_input_mutex_;
    std::mutex right_input_mutex_;
    mutable std::mutex settings_mutex_;
    RuntimeSettings settings_;
    std::atomic<bool> left_injected_down_{false};
    std::atomic<bool> right_injected_down_{false};
    std::atomic<bool> blockhit_holding_{false};
    uint64_t last_blockhit_qpc_ = 0; // left worker owns this value
};
