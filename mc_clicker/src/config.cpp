#include "config.hpp"
#include <charconv>

namespace {

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

bool parse_float(const std::string& v, float& out) {
    try {
        out = std::stof(v);
        return true;
    } catch (...) { return false; }
}

bool parse_int(const std::string& v, int& out) {
    try {
        out = std::stoi(v);
        return true;
    } catch (...) { return false; }
}

bool parse_bool(const std::string& v, bool& out) {
    if (v == "1" || v == "true" || v == "True" || v == "TRUE") { out = true; return true; }
    if (v == "0" || v == "false" || v == "False" || v == "FALSE") { out = false; return true; }
    return false;
}

bool parse_randomization(const std::string& key, const std::string& val,
                         const std::string& prefix, RandomizationSettings& settings) {
    if (key.rfind(prefix, 0) != 0) return false;
    const std::string field = key.substr(prefix.size());
    if (field == "mode") parse_int(val, settings.mode);
    else if (field == "strength") parse_float(val, settings.strength);
    else if (field == "normal_variation") parse_float(val, settings.normal_variation);
    else if (field == "extra_variation") parse_float(val, settings.extra_variation);
    else if (field == "extra_drift") parse_float(val, settings.extra_drift);
    else if (field == "extra_drift_period_ms") parse_float(val, settings.extra_drift_period_ms);
    else if (field == "plus_variation") parse_float(val, settings.plus_variation);
    else if (field == "plus_drift") parse_float(val, settings.plus_drift);
    else if (field == "plus_drift_period_ms") parse_float(val, settings.plus_drift_period_ms);
    else if (field == "plus_dip_chance") parse_float(val, settings.plus_dip_chance);
    else if (field == "plus_dip_depth") parse_float(val, settings.plus_dip_depth);
    return true;
}

void save_randomization(std::ostream& out, const char* prefix,
                        const RandomizationSettings& settings) {
    out << prefix << "mode=" << settings.mode << '\n';
    out << prefix << "strength=" << settings.strength << '\n';
    out << prefix << "normal_variation=" << settings.normal_variation << '\n';
    out << prefix << "extra_variation=" << settings.extra_variation << '\n';
    out << prefix << "extra_drift=" << settings.extra_drift << '\n';
    out << prefix << "extra_drift_period_ms=" << settings.extra_drift_period_ms << '\n';
    out << prefix << "plus_variation=" << settings.plus_variation << '\n';
    out << prefix << "plus_drift=" << settings.plus_drift << '\n';
    out << prefix << "plus_drift_period_ms=" << settings.plus_drift_period_ms << '\n';
    out << prefix << "plus_dip_chance=" << settings.plus_dip_chance << '\n';
    out << prefix << "plus_dip_depth=" << settings.plus_dip_depth << '\n';
}

} // namespace

bool Config::load(const std::filesystem::path& p) {
    std::ifstream f(p);
    if (!f) return false;

    std::string line;
    bool left_mode_loaded = false;
    bool right_mode_loaded = false;
    bool left_strength_loaded = false;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        if (key == "left_random_mode") left_mode_loaded = true;
        if (key == "right_random_mode") right_mode_loaded = true;
        if (key == "left_random_strength") left_strength_loaded = true;
        if (parse_randomization(key, val, "left_random_", left_randomization) ||
            parse_randomization(key, val, "right_random_", right_randomization)) continue;

        if (key == "left_enabled")                  parse_bool(val, left_enabled);
        else if (key == "min_cps")                  parse_float(val, min_cps);
        else if (key == "max_cps")                  parse_float(val, max_cps);
        else if (key == "humanize_strength")        parse_float(val, humanize_strength);
        else if (key == "random_strength")          parse_float(val, random_strength);
        else if (key == "weapon_only")              parse_bool(val, weapon_only);
        else if (key == "weapon_slot_mask")         { int m = 0; if (parse_int(val, m)) weapon_slot_mask = static_cast<uint32_t>(m); }
        else if (key == "require_lmb_hold")         parse_bool(val, require_lmb_hold);

        // Right Clicker
        else if (key == "right_enabled")            parse_bool(val, right_enabled);
        else if (key == "right_min_cps")            parse_float(val, right_min_cps);
        else if (key == "right_max_cps")            parse_float(val, right_max_cps);
        else if (key == "right_humanize_strength")  parse_float(val, right_humanize_strength);
        else if (key == "require_rmb_hold")         parse_bool(val, require_rmb_hold);
        else if (key == "blockhit")                 parse_bool(val, blockhit);
        else if (key == "blockhit_duration_ms")     parse_float(val, blockhit_duration_ms);
        else if (key == "blockhit_chance")          parse_float(val, blockhit_chance);
        else if (key == "blockhit_attack_delay_ms") parse_float(val, blockhit_attack_delay_ms);
        else if (key == "blockhit_cooldown_ms")     parse_float(val, blockhit_cooldown_ms);

        // Keybinds & System
        else if (key == "toggle_vk")                parse_int(val, toggle_vk);
        else if (key == "right_toggle_vk")          parse_int(val, right_toggle_vk);
        else if (key == "destruct_vk")              parse_int(val, destruct_vk);
        else if (key == "pause_on_gui")             parse_bool(val, pause_on_gui);
        else if (key == "toggle_only_when_active")  parse_bool(val, toggle_only_when_active);
        else if (key == "clean_on_exit")            parse_bool(val, clean_on_exit);
        else if (key == "self_delete_exe")          parse_bool(val, self_delete_exe);
        else if (key == "ui_opacity")               parse_float(val, ui_opacity);
        else if (key == "show_overlay")             parse_bool(val, show_overlay);
    }
    if (!left_mode_loaded) left_randomization.mode = humanize_strength > 0.85f ? 3 :
                                                      humanize_strength > 0.45f ? 2 : 1;
    if (!right_mode_loaded) right_randomization.mode = right_humanize_strength > 0.85f ? 3 :
                                                        right_humanize_strength > 0.45f ? 2 : 1;
    if (!left_strength_loaded) left_randomization.strength = random_strength;
    clamp();
    return true;
}

bool Config::save(const std::filesystem::path& p) const {
    std::ofstream f(p);
    if (!f) return false;
    f << "# mc_clicker config v4.0\n";
    f << "[LeftClicker]\n";
    f << "left_enabled=" << (left_enabled ? 1 : 0) << "\n";
    f << "min_cps=" << min_cps << "\n";
    f << "max_cps=" << max_cps << "\n";
    f << "humanize_strength=" << humanize_strength << "\n";
    f << "random_strength=" << random_strength << "\n";
    save_randomization(f, "left_random_", left_randomization);
    f << "weapon_only=" << (weapon_only ? 1 : 0) << "\n";
    f << "weapon_slot_mask=" << weapon_slot_mask << "\n";
    f << "require_lmb_hold=" << (require_lmb_hold ? 1 : 0) << "\n";

    f << "\n[RightClicker]\n";
    f << "right_enabled=" << (right_enabled ? 1 : 0) << "\n";
    f << "right_min_cps=" << right_min_cps << "\n";
    f << "right_max_cps=" << right_max_cps << "\n";
    f << "right_humanize_strength=" << right_humanize_strength << "\n";
    save_randomization(f, "right_random_", right_randomization);
    f << "require_rmb_hold=" << (require_rmb_hold ? 1 : 0) << "\n";
    f << "blockhit=" << (blockhit ? 1 : 0) << "\n";
    f << "blockhit_duration_ms=" << blockhit_duration_ms << "\n";
    f << "blockhit_chance=" << blockhit_chance << "\n";
    f << "blockhit_attack_delay_ms=" << blockhit_attack_delay_ms << "\n";
    f << "blockhit_cooldown_ms=" << blockhit_cooldown_ms << "\n";


    f << "\n[Keybinds]\n";
    f << "toggle_vk=" << toggle_vk << "\n";
    f << "right_toggle_vk=" << right_toggle_vk << "\n";
    f << "destruct_vk=" << destruct_vk << "\n";

    f << "\n[System]\n";
    f << "pause_on_gui=" << (pause_on_gui ? 1 : 0) << "\n";
    f << "toggle_only_when_active=" << (toggle_only_when_active ? 1 : 0) << "\n";
    f << "clean_on_exit=" << (clean_on_exit ? 1 : 0) << "\n";
    f << "self_delete_exe=" << (self_delete_exe ? 1 : 0) << "\n";
    f << "ui_opacity=" << ui_opacity << "\n";
    f << "show_overlay=" << (show_overlay ? 1 : 0) << "\n";
    return true;
}
