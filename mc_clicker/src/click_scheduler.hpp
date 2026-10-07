#pragma once

#include "util.hpp"
#include "config.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>

// Independent deadline scheduler shared by both clickers. It selects a
// bounded CPS value for each interval and advances from the previous deadline
// so button hold time does not reduce the configured rate. Missed deadlines
// are discarded rather than replayed as a burst.
struct ClickScheduler {
    uint64_t next_qpc = 0;
    double last_cps = 0.0;

    void arm_immediate(uint64_t now) {
        next_qpc = now;
    }

    void schedule_next(uint64_t now, double min_cps, double max_cps,
                       const RandomizationSettings& config, std::mt19937& rng) {
        double lo = std::clamp(min_cps, 1.0, 25.0);
        double hi = std::clamp(max_cps, 1.0, 25.0);
        if (lo > hi) std::swap(lo, hi);

        const double center = (lo + hi) * 0.5;
        const double span = (hi - lo) * 0.5;
        double target = center;

        std::normal_distribution<double> gauss(0.0, 1.0);
        std::uniform_real_distribution<double> uni(0.0, 1.0);
        const double strength = std::clamp(static_cast<double>(config.strength), 0.0, 1.0);
        if (span > 0.0) {
            if (config.mode == 1) {
                target += gauss(rng) * span * config.normal_variation * (0.4 + strength * 0.6);
            } else if (config.mode == 2) {
                target += gauss(rng) * span * config.extra_variation;
                const double elapsed_ms = util::ticks_to_ms(now);
                target += std::sin(elapsed_ms * 2.0 * 3.14159265358979323846 /
                                   config.extra_drift_period_ms) * span * config.extra_drift * strength;
            } else if (config.mode == 3) {
                target += gauss(rng) * span * config.plus_variation;
                const double elapsed_ms = util::ticks_to_ms(now);
                target += std::sin(elapsed_ms * 2.0 * 3.14159265358979323846 /
                                   config.plus_drift_period_ms) * span * config.plus_drift * strength;
                if (uni(rng) < config.plus_dip_chance * strength)
                    target -= span * config.plus_dip_depth;
            }
        }
        target = std::clamp(target, lo, hi);

        last_cps = target;

        double interval_ms = 1000.0 / target;

        const uint64_t interval = std::max<uint64_t>(1, util::ms_to_ticks(interval_ms));
        uint64_t candidate = (next_qpc != 0 ? next_qpc : now) + interval;
        if (candidate <= now) candidate = now + interval;
        next_qpc = candidate;
    }

    bool due(uint64_t now) const {
        return next_qpc != 0 && now >= next_qpc;
    }

    double remaining_ms(uint64_t now) const {
        if (next_qpc == 0 || now >= next_qpc) return 0.0;
        return util::ticks_to_ms(next_qpc - now);
    }

    void reset() {
        next_qpc = 0;
        last_cps = 0.0;
    }
};
