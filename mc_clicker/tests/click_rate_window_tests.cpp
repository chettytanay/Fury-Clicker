#include "click_rate_window.hpp"
#include <iostream>

int main() {
    ClickRateWindow rate;
    const uint64_t start = util::qpc();
    if (rate.recent_cps(start) != 0.0f) return 1;
    rate.record(start);
    if (rate.recent_cps(start) != 1.0f) {
        std::cerr << "first click not visible immediately\n";
        return 1;
    }
    rate.record(start + util::ms_to_ticks(100.0));
    if (rate.recent_cps(start + util::ms_to_ticks(500.0)) != 2.0f ||
        rate.peak_cps() != 2.0f) return 1;
    if (rate.recent_cps(start + util::ms_to_ticks(1200.0)) != 0.0f ||
        rate.peak_cps() != 2.0f) return 1;
    std::cout << "click rate window tests passed\n";
}
