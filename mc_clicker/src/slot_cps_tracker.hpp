#pragma once
#include "util.hpp"
#include <array>
#include <cstdint>
#include <deque>
#include <mutex>

class SlotCpsTracker {
public:
    void record(int slot, uint64_t now) {
        if (slot < 1 || slot > 9) return;
        std::lock_guard<std::mutex> lock(mutex_);
        auto& clicks = clicks_[slot - 1];
        prune(clicks, now);
        clicks.push_back(now);
        ++totals_[slot - 1];
    }

    float cps(int slot, uint64_t now) const {
        if (slot < 1 || slot > 9) return 0.0f;
        std::lock_guard<std::mutex> lock(mutex_);
        auto& clicks = clicks_[slot - 1];
        prune(clicks, now);
        return static_cast<float>(clicks.size());
    }

    uint64_t total(int slot) const {
        if (slot < 1 || slot > 9) return 0;
        std::lock_guard<std::mutex> lock(mutex_);
        return totals_[slot - 1];
    }

private:
    static void prune(std::deque<uint64_t>& clicks, uint64_t now) {
        const uint64_t window = util::ms_to_ticks(1000.0);
        while (!clicks.empty() && now >= clicks.front() && now - clicks.front() >= window)
            clicks.pop_front();
    }

    mutable std::mutex mutex_;
    mutable std::array<std::deque<uint64_t>, 9> clicks_;
    std::array<uint64_t, 9> totals_{};
};
