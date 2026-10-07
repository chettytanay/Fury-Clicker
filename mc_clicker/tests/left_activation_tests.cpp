#include "left_activation.hpp"
#include <iostream>

int main() {
    // Click Only requires the conjunction, even if the legacy slot toggle is off.
    for (bool legacy_slot_toggle : {false, true}) {
        if (left_activation_allowed(true, legacy_slot_toggle, false, true) ||
            left_activation_allowed(true, legacy_slot_toggle, true, false) ||
            left_activation_allowed(true, legacy_slot_toggle, false, false) ||
            !left_activation_allowed(true, legacy_slot_toggle, true, true)) {
            std::cerr << "Click Only gating failed\n";
            return 1;
        }
    }
    if (!left_activation_allowed(false, false, false, false) ||
        left_activation_allowed(false, true, true, false) ||
        !left_activation_allowed(false, true, false, true)) return 1;
    std::cout << "left activation tests passed\n";
}
