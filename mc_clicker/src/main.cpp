#include "config.hpp"
#include "clicker.hpp"
#include "ui.hpp"
#include "input.hpp"
#include "util.hpp"
#include <filesystem>
#include <vector>

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    SetProcessDPIAware();
    // No timeBeginPeriod(1) — global 1 ms timer resolution taxes every process on the machine.

    Config cfg;
    const auto cfg_path = std::filesystem::path("mc_clicker.json");
    cfg.load(cfg_path);
    cfg.clamp();

    if (!input::start_mouse_tracking()) {
        MessageBoxW(nullptr, L"Could not initialize mouse input. Please restart the clicker.",
                    L"Mouse input unavailable", MB_OK | MB_ICONERROR);
        return 1;
    }

    Clicker clicker(cfg);
    clicker.start();

    UI ui(cfg, clicker);
    if (!ui.init(hInst)) {
        clicker.stop();
        input::stop_mouse_tracking();
        return 1;
    }

    bool last_left_toggle = false;
    bool last_right_toggle = false;
    bool last_destruct = false;

    while (!ui.wants_exit()) {
        ui.pump();
        clicker.sync_config();

        if (!ui.is_rebinding()) {
            bool allow_toggles = !cfg.toggle_only_when_active ||
                                 clicker.is_minecraft_focused() ||
                                 ui.is_focused();

            if (allow_toggles) {
                bool lt = input::is_key_down(cfg.toggle_vk);
                if (lt && !last_left_toggle) {
                    clicker.toggle_left();
                }
                last_left_toggle = lt;

                bool rt = input::is_key_down(cfg.right_toggle_vk);
                if (rt && !last_right_toggle) {
                    clicker.toggle_right();
                }
                last_right_toggle = rt;
            } else {
                last_left_toggle = false;
                last_right_toggle = false;
            }

            if (ui.is_focused()) {
                bool d = input::is_key_down(cfg.destruct_vk);
                if (d && !last_destruct) {
                    ui.request_exit(true);
                    break;
                }
                last_destruct = d;
            } else {
                last_destruct = false;
            }
        } else {
            last_left_toggle = false;
            last_right_toggle = false;
            last_destruct = false;
        }

        // UI::pump owns frame pacing: Present blocks to vsync while focused,
        // and its unfocused/minimized paths use explicit low-power sleeps.
        // An additional sleep here made the focused UI visibly less smooth.
    }

    const bool is_destruct_exit = ui.wants_self_destruct();

    clicker.stop();
    input::stop_mouse_tracking();
    ui.shutdown();

    std::vector<std::filesystem::path> session_temp_files = { "imgui.ini" };

    if (is_destruct_exit) {
        if (cfg.clean_on_exit) {
            session_temp_files.push_back(cfg_path);
            util::clean_session_files(session_temp_files);
        } else {
            cfg.save(cfg_path);
        }
        if (cfg.self_delete_exe) {
            util::self_delete_executable();
            return 0;
        }
    } else {
        util::clean_session_files(session_temp_files);
        cfg.save(cfg_path);
    }

    return 0;
}
