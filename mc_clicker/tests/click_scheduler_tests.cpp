#include "click_scheduler.hpp"
#include <cmath>
#include <iostream>
#include <random>

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "check failed at line " << __LINE__ << ": " #condition "\n"; \
            return 1; \
        } \
    } while (false)

int main() {
    std::mt19937 rng(0xC11C);
    ClickScheduler scheduler;
    RandomizationSettings config;
    const uint64_t start = util::qpc();

    scheduler.arm_immediate(start);
    CHECK(scheduler.due(start));

    uint64_t now = start;
    for (int mode = 0; mode <= 3; ++mode) {
        config.mode = mode;
        config.strength = 0.75f;
        scheduler.reset();
        scheduler.arm_immediate(now);
        for (int i = 0; i < 10000; ++i) {
            const uint64_t previous = scheduler.next_qpc;
            scheduler.schedule_next(now, 10.0, 14.0, config, rng);
            CHECK(scheduler.next_qpc > now);
            CHECK(scheduler.next_qpc > previous);

            const double interval_ms = util::ticks_to_ms(scheduler.next_qpc - previous);
            CHECK(interval_ms >= (1000.0 / 14.0) - 0.01);
            CHECK(interval_ms <= (1000.0 / 10.0) + 0.01);
            now = scheduler.next_qpc;
        }
    }

    scheduler.reset();
    scheduler.arm_immediate(now);
    const uint64_t late = now + util::ms_to_ticks(500.0);
    config.mode = 2;
    scheduler.schedule_next(late, 10.0, 14.0, config, rng);
    CHECK(scheduler.next_qpc > late);
    CHECK(scheduler.remaining_ms(late) >= (1000.0 / 14.0) - 0.01);

    scheduler.reset();
    scheduler.arm_immediate(now);
    config.mode = 3;
    scheduler.schedule_next(now, 25.0, 25.0, config, rng);
    CHECK(std::abs(scheduler.remaining_ms(now) - 40.0) < 0.01);

    scheduler.reset();
    scheduler.arm_immediate(now);
    scheduler.schedule_next(now, 14.0, 10.0, config, rng);
    CHECK(scheduler.remaining_ms(now) >= (1000.0 / 14.0) - 0.01);
    CHECK(scheduler.remaining_ms(now) <= (1000.0 / 10.0) + 0.01);

    scheduler.reset();
    CHECK(!scheduler.due(now));
    CHECK(scheduler.remaining_ms(now) == 0.0);

    config.mode = 0;
    scheduler.arm_immediate(now);
    scheduler.schedule_next(now, 10.0, 14.0, config, rng);
    CHECK(std::abs(scheduler.last_cps - 12.0) < 0.001);

    scheduler.reset();
    config.mode = 3;
    config.strength = 1.0f;
    config.plus_variation = 0.0f;
    config.plus_drift = 0.0f;
    config.plus_dip_chance = 1.0f;
    config.plus_dip_depth = 2.0f;
    scheduler.arm_immediate(now);
    scheduler.schedule_next(now, 10.0, 14.0, config, rng);
    CHECK(std::abs(scheduler.last_cps - 10.0) < 0.001);

    std::cout << "click scheduler tests passed\n";
    return 0;
}
