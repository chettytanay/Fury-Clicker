#include "slot_cps_tracker.hpp"
#include <iostream>

int main() {
    SlotCpsTracker tracker;
    const uint64_t start = util::qpc();
    tracker.record(1, start);
    tracker.record(1, start + util::ms_to_ticks(100));
    tracker.record(2, start + util::ms_to_ticks(200));
    if (tracker.cps(1, start + util::ms_to_ticks(300)) != 2.0f ||
        tracker.cps(2, start + util::ms_to_ticks(300)) != 1.0f ||
        tracker.cps(3, start + util::ms_to_ticks(300)) != 0.0f ||
        tracker.total(1) != 2 || tracker.total(2) != 1) {
        std::cerr << "slot CPS attribution failed\n";
        return 1;
    }
    if (tracker.cps(1, start + util::ms_to_ticks(1200)) != 0.0f ||
        tracker.total(1) != 2) {
        std::cerr << "slot CPS expiry failed\n";
        return 1;
    }
    std::cout << "slot CPS tests passed\n";
}
