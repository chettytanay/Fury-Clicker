#include "clicker.hpp"

// ---- Clicker ----

Clicker::Clicker(Config& cfg) : cfg_(cfg) {
    sync_config();
}

Clicker::~Clicker() {
    stop();
}

void Clicker::start() {
    if (running_.exchange(true)) return;

    sup_th_   = std::thread([this] { supervisor_worker(); });
    left_th_  = std::thread([this] { left_worker(); });
    right_th_ = std::thread([this] { right_worker(); });

    SetThreadPriority(sup_th_.native_handle(),  THREAD_PRIORITY_BELOW_NORMAL);
    SetThreadPriority(left_th_.native_handle(), THREAD_PRIORITY_NORMAL);
    SetThreadPriority(right_th_.native_handle(), THREAD_PRIORITY_NORMAL);
}

void Clicker::stop() {
    running_ = false;
    release_left();
    release_right();
    if (sup_th_.joinable())  sup_th_.join();
    if (left_th_.joinable()) left_th_.join();
    if (right_th_.joinable()) right_th_.join();
    release_left();
    release_right();
    mem_.detach();
}

void Clicker::toggle_left() {
    cfg_.left_enabled = !cfg_.left_enabled;
    sync_config();
    if (!cfg_.left_enabled) {
        release_left();
    }
}

void Clicker::toggle_right() {
    cfg_.right_enabled = !cfg_.right_enabled;
    sync_config();
}

void Clicker::self_destruct() {
    cfg_.left_enabled = false;
    cfg_.right_enabled = false;
    running_ = false;
    stop();
    release_left();
    release_right();
}

bool Clicker::is_memory_attached() const {
    return mem_.is_attached();
}

void Clicker::sync_config() {
    std::lock_guard<std::mutex> lock(settings_mutex_);
    settings_.left_enabled = cfg_.left_enabled;
    settings_.min_cps = cfg_.min_cps;
    settings_.max_cps = cfg_.max_cps;
    settings_.left_randomization = cfg_.left_randomization;
    settings_.weapon_only = cfg_.weapon_only;
    settings_.weapon_slot_mask = cfg_.weapon_slot_mask;
    settings_.require_lmb_hold = cfg_.require_lmb_hold;
    settings_.right_enabled = cfg_.right_enabled;
    settings_.right_min_cps = cfg_.right_min_cps;
    settings_.right_max_cps = cfg_.right_max_cps;
    settings_.right_randomization = cfg_.right_randomization;
    settings_.require_rmb_hold = cfg_.require_rmb_hold;
    settings_.blockhit = cfg_.blockhit;
    settings_.blockhit_duration_ms = cfg_.blockhit_duration_ms;
    settings_.blockhit_chance = cfg_.blockhit_chance;
    settings_.blockhit_attack_delay_ms = cfg_.blockhit_attack_delay_ms;
    settings_.blockhit_cooldown_ms = cfg_.blockhit_cooldown_ms;
    settings_.pause_on_gui = cfg_.pause_on_gui;
}

Clicker::RuntimeSettings Clicker::settings_snapshot() const {
    std::lock_guard<std::mutex> lock(settings_mutex_);
    return settings_;
}

void Clicker::supervisor_worker() {
    while (running_) {
        const RuntimeSettings settings = settings_snapshot();
        HWND mc_hwnd = nullptr;
        bool ingame = ProcessMemory::is_ingame_minecraft(&mc_hwnd);
        is_mc_ingame_.store(ingame, std::memory_order_relaxed);

        bool mc = (mc_hwnd != nullptr);
        is_mc_focused_.store(mc, std::memory_order_relaxed);

        if (!mc) {
            release_left();
            release_right();
        } else if (settings.pause_on_gui && !ingame) {
            release_left();
            release_right();
        }
        if (blockhit_holding_.load(std::memory_order_acquire) &&
            (!settings.blockhit || !input::is_rmb_down())) {
            // A block-hit may leave synthetic RMB held to mirror the physical hold.
            // Release it as soon as the user lets go or disables the module.
            release_right();
        }

        if (mc && ingame) {
            mem_.update_hotbar();
        } else {
            mem_.reset_hotbar_input();
        }

        Sleep(mc ? 40 : 100);
    }
}

bool Clicker::pulse_context_valid() const {
    const RuntimeSettings settings = settings_snapshot();
    return running_.load(std::memory_order_relaxed) &&
           is_mc_focused_.load(std::memory_order_relaxed) &&
           (!settings.pause_on_gui || is_mc_ingame_.load(std::memory_order_relaxed));
}

void Clicker::release_left() {
    if (left_injected_down_.exchange(false, std::memory_order_acq_rel)) {
        if (!input::left_up()) input_failures_++;
    }
}

void Clicker::release_right() {
    blockhit_holding_.store(false, std::memory_order_release);
    if (right_injected_down_.exchange(false, std::memory_order_acq_rel)) {
        if (!input::right_up()) input_failures_++;
    }
}

void Clicker::wait_for_due(const ClickScheduler& scheduler) {
    const uint64_t now = util::qpc();
    const double remaining = scheduler.remaining_ms(now);
    if (remaining <= 0.0) return;
    util::wait_until(now + util::ms_to_ticks(std::min(remaining, 8.0)));
}

// Fire one left-click with a short, serialized pulse. The explicit down-state
// tracking lets every exit path release only input that this process owns.
void Clicker::fire_left_click() {
    std::lock_guard<std::mutex> lock(left_input_mutex_);
    if (!pulse_context_valid()) return;

    const int click_slot = mem_.active_slot();
    // When the physical button is held, Minecraft may already consider it
    // down. Force a release edge before each synthetic press so another click
    // can be observed by LWJGL rather than extending the original hold.
    if (input::is_lmb_down()) {
        if (!input::left_up()) {
            input_failures_++;
            return;
        }
        util::wait_until(util::qpc() + util::ms_to_ticks(2.0));
        if (!pulse_context_valid()) return;
    }
    if (!input::left_down()) {
        input_failures_++;
        return;
    }
    left_injected_down_.store(true, std::memory_order_release);

    double hold = std::clamp(8.0 + util::uniform(-2.0, 3.0), 6.0, 11.0);
    util::wait_until(util::qpc() + util::ms_to_ticks(hold));

    release_left();
    total_left_clicks_++;
    const uint64_t now = util::qpc();
    slot_cps_.record(click_slot, now);
    left_rate_.record(now);
}

// Right-click pulses are serialized with block-hit so the two producers can
// never release one another's input or leave use-item held.
void Clicker::fire_right_click(float hold_ms, bool count_click) {
    std::lock_guard<std::mutex> lock(right_input_mutex_);
    if (!pulse_context_valid()) return;

    if (input::is_rmb_down()) {
        if (!input::right_up()) {
            input_failures_++;
            return;
        }
        util::wait_until(util::qpc() + util::ms_to_ticks(2.0));
        if (!pulse_context_valid()) return;
    }
    if (!input::right_down()) {
        input_failures_++;
        return;
    }
    right_injected_down_.store(true, std::memory_order_release);

    double hold = hold_ms > 0.0f
        ? std::clamp(static_cast<double>(hold_ms), 6.0, 200.0)
        : std::clamp(8.0 + util::uniform(-2.0, 3.0), 6.0, 11.0);
    util::wait_until(util::qpc() + util::ms_to_ticks(hold));

    release_right();
    if (!count_click) return;
    total_right_clicks_++;

    right_rate_.record(util::qpc());
}

void Clicker::fire_blockhit_click(const RuntimeSettings& settings) {
    // Skip an overlapping right-click pulse; never delay the attack behind it.
    std::unique_lock<std::mutex> lock(right_input_mutex_, std::try_to_lock);
    if (!lock.owns_lock() || !pulse_context_valid() || !input::is_rmb_down()) {
        fire_left_click();
        return;
    }

    // Release use-item before attacking. A physical RMB hold can keep the
    // logical game button down, so always emit an up edge for this transition.
    right_injected_down_.store(false, std::memory_order_release);
    blockhit_holding_.store(false, std::memory_order_release);
    input::right_up();
    if (settings.blockhit_attack_delay_ms > 0.0f)
        util::wait_until(util::qpc() + util::ms_to_ticks(settings.blockhit_attack_delay_ms));

    fire_left_click();
    if (pulse_context_valid() && input::is_rmb_down() && settings_snapshot().blockhit) {
        input::right_down();
        right_injected_down_.store(true, std::memory_order_release);
        blockhit_holding_.store(true, std::memory_order_release);
        total_blockhits_.fetch_add(1, std::memory_order_relaxed);
    }
    last_blockhit_qpc_ = util::qpc();
}

// Left worker: independent deadline scheduling with immediate activation.
void Clicker::left_worker() {
    std::mt19937 rng{std::random_device{}()};
    bool was_eligible = false;

    while (running_) {
        const RuntimeSettings settings = settings_snapshot();
        const bool focused = is_mc_focused_.load(std::memory_order_relaxed);
        const bool ingame = is_mc_ingame_.load(std::memory_order_relaxed);
        const bool held = !settings.require_lmb_hold || input::is_lmb_down();
        const bool slot_required = settings.require_lmb_hold || settings.weapon_only;
        const bool weapon_ok = !slot_required || mem_.is_slot_allowed(settings.weapon_slot_mask);
        const bool activation_ok = left_activation_allowed(
            settings.require_lmb_hold, settings.weapon_only,
            input::is_lmb_down(), mem_.is_slot_allowed(settings.weapon_slot_mask));
        const bool eligible = settings.left_enabled && focused &&
                              (!settings.pause_on_gui || ingame) && activation_ok;
        if (focused) {
            last_left_state_.store(
                settings.pause_on_gui && !ingame ? ActivationState::Menu :
                !settings.left_enabled ? ActivationState::Disabled :
                !held ? ActivationState::WaitingForHold :
                slot_required && mem_.active_slot() == 0 ? ActivationState::SlotUnknown :
                !weapon_ok ? ActivationState::SlotBlocked : ActivationState::Ready,
                std::memory_order_relaxed);
        }

        if (!eligible) {
            release_left();
            left_scheduler_.reset();
            was_eligible = false;
            Sleep(held ? 12 : 5);
            continue;
        }

        const uint64_t now = util::qpc();
        if (!was_eligible) {
            left_scheduler_.arm_immediate(now);
            was_eligible = true;
        }

        if (!left_scheduler_.due(now)) {
            wait_for_due(left_scheduler_);
            continue;
        }

        left_scheduler_.schedule_next(now, settings.min_cps, settings.max_cps,
                                      settings.left_randomization, rng);
        left_due_count_.fetch_add(1, std::memory_order_relaxed);

        const bool physical_rmb = input::is_rmb_down();
        const double min_gap = std::max(settings.blockhit_duration_ms,
                                        settings.blockhit_cooldown_ms);
        const bool cooldown_done = last_blockhit_qpc_ == 0 ||
            util::ticks_to_ms(now - last_blockhit_qpc_) >= min_gap;
        if (settings.blockhit && physical_rmb && cooldown_done &&
            util::chance(settings.blockhit_chance)) {
            fire_blockhit_click(settings);
        } else {
            fire_left_click();
        }
    }

    release_left();
}

// Right worker keeps its own schedule and state because use/place semantics
// differ from attack and can also share the button with block-hit.
void Clicker::right_worker() {
    std::mt19937 rng{std::random_device{}()};
    bool was_eligible = false;

    while (running_) {
        const RuntimeSettings settings = settings_snapshot();
        const bool focused = is_mc_focused_.load(std::memory_order_relaxed);
        const bool ingame = is_mc_ingame_.load(std::memory_order_relaxed);
        const bool held = !settings.require_rmb_hold || input::is_rmb_down();
        const bool eligible = settings.right_enabled && focused &&
                              (!settings.pause_on_gui || ingame) && held;
        const bool blockhit_using_rmb = settings.blockhit && input::is_rmb_down();
        if (focused) {
            last_right_state_.store(
                settings.pause_on_gui && !ingame ? ActivationState::Menu :
                !settings.right_enabled ? ActivationState::Disabled :
                !held ? ActivationState::WaitingForHold : ActivationState::Ready,
                std::memory_order_relaxed);
        }

        if (!eligible || blockhit_using_rmb) {
            right_scheduler_.reset();
            was_eligible = false;
            Sleep(held ? 12 : 5);
            continue;
        }

        const uint64_t now = util::qpc();
        if (!was_eligible) {
            right_scheduler_.arm_immediate(now);
            was_eligible = true;
        }

        if (!right_scheduler_.due(now)) {
            wait_for_due(right_scheduler_);
            continue;
        }

        right_scheduler_.schedule_next(now, settings.right_min_cps, settings.right_max_cps,
                                       settings.right_randomization, rng);
        right_due_count_.fetch_add(1, std::memory_order_relaxed);
        fire_right_click();
    }

    release_right();
}
