#include "input.hpp"
#include <iostream>

int main() {
    // Exercise registration, repeated initialization, shutdown and restart.
    // No synthetic input is sent to the user's desktop.
    for (int i = 0; i < 5; ++i) {
        if (!input::start_mouse_tracking() || !input::start_mouse_tracking()) {
            input::stop_mouse_tracking();
            std::cerr << "Raw Input initialization failed\n";
            return 1;
        }
        input::stop_mouse_tracking();
        input::stop_mouse_tracking();
        if (input::is_lmb_down() || input::is_rmb_down() || input::consume_wheel_delta() != 0) {
            std::cerr << "Input state was not cleared on shutdown\n";
            return 1;
        }
    }
    std::cout << "input tracking lifecycle tests passed\n";
}
