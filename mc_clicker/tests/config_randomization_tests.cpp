#include "config.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

int main() {
    const auto path = std::filesystem::temp_directory_path() /
        ("mc_clicker_randomization_test_" + std::to_string(GetCurrentProcessId()) + ".cfg");
    Config source;
    source.left_randomization.mode = 3;
    source.left_randomization.plus_dip_chance = 0.42f;
    source.left_randomization.plus_drift_period_ms = 4321.0f;
    source.right_randomization.mode = 2;
    source.right_randomization.extra_variation = 1.15f;
    source.blockhit_attack_delay_ms = 7.0f;
    source.blockhit_cooldown_ms = 175.0f;
    if (!source.save(path)) return 1;
    Config loaded;
    const bool ok = loaded.load(path) && loaded.left_randomization.mode == 3 &&
                    std::abs(loaded.left_randomization.plus_dip_chance - 0.42f) < 0.001f &&
                    loaded.left_randomization.plus_drift_period_ms == 4321.0f &&
                    loaded.right_randomization.mode == 2 &&
                    std::abs(loaded.right_randomization.extra_variation - 1.15f) < 0.001f &&
                    loaded.blockhit_attack_delay_ms == 7.0f &&
                    loaded.blockhit_cooldown_ms == 175.0f;
    std::filesystem::remove(path);
    if (!ok) { std::cerr << "randomization config round-trip failed\n"; return 1; }

    {
        std::ofstream old(path);
        old << "humanize_strength=0.90\nrandom_strength=0.70\nright_humanize_strength=0.60\n";
    }
    Config legacy;
    const bool migrated = legacy.load(path) && legacy.left_randomization.mode == 3 &&
        legacy.right_randomization.mode == 2 &&
        std::abs(legacy.left_randomization.strength - 0.70f) < 0.001f;
    std::filesystem::remove(path);
    if (!migrated) { std::cerr << "legacy config migration failed\n"; return 1; }
    std::cout << "randomization config tests passed\n";
}
