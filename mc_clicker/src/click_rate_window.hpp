#pragma once
#include "util.hpp"
#include <deque>
#include <mutex>

// Number of successful injected clicks during the preceding second.
class ClickRateWindow {
public:
    void record(uint64_t now) {
        std::lock_guard<std::mutex> lock(mutex_);
        prune(now);
        clicks_.push_back(now);
        if (clicks_.size() > peak_) peak_ = clicks_.size();
    }

    float recent_cps(uint64_t now) const {
        std::lock_guard<std::mutex> lock(mutex_);
        prune(now);
        return static_cast<float>(clicks_.size());
    }

    float peak_cps() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<float>(peak_);
    }

private:
    void prune(uint64_t now) const {
        const uint64_t window = util::ms_to_ticks(1000.0);
        while (!clicks_.empty() && now >= clicks_.front() && now - clicks_.front() >= window)
            clicks_.pop_front();
    }

    mutable std::mutex mutex_;
    mutable std::deque<uint64_t> clicks_;
    size_t peak_ = 0;
};
