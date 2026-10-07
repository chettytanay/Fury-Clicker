#pragma once
#include "config.hpp"
#include "clicker.hpp"
#include <d3d11.h>
#include <dxgi.h>

struct ImGuiContext;
struct ImFont;

class UI {
public:
    UI(Config& cfg, Clicker& clicker);
    ~UI();

    bool init(HINSTANCE hInst);
    void shutdown();
    void pump();
    bool wants_exit() const { return wants_exit_; }
    bool wants_self_destruct() const { return wants_self_destruct_; }
    void request_exit(bool self_destruct = false) { wants_exit_ = true; wants_self_destruct_ = self_destruct; }
    bool is_focused() const { return hwnd_ != nullptr && !IsIconic(hwnd_) && GetForegroundWindow() == hwnd_; }
    bool is_rebinding() const { return rebind_target_ != 0; }
    HWND hwnd() const { return hwnd_; }

private:
    bool create_device(HWND hwnd);
    void cleanup_device();
    void render_frame();
    void draw_panel();
    void apply_theme();

    // Navigation & Views (Analytics removed per requirement)
    void draw_sidebar();
    void draw_header();
    void draw_tab_left_clicker();
    void draw_tab_right_clicker();
    void draw_tab_blockhit();
    void draw_tab_keybinds();
    void draw_tab_config();

    // Sleek Custom Widgets
    bool sidebar_button(const char* label, int icon_type, bool selected);
    void custom_slider(const char* label, float* v, float v_min, float v_max, const char* fmt);
    void section_header(const char* title);

    Config&  cfg_;
    Clicker& clicker_;

    HWND hwnd_ = nullptr;
    WNDCLASSEXW wc_{};

    ID3D11Device*           device_  = nullptr;
    ID3D11DeviceContext*    ctx_     = nullptr;
    IDXGISwapChain*         swap_    = nullptr;
    ID3D11RenderTargetView* rtv_     = nullptr;

    bool wants_exit_ = false;
    bool wants_self_destruct_ = false;
    int  current_tab_ = 0; // 0=Left Clicker, 1=Right Clicker, 2=Block-Hit, 3=Keybinds, 4=Config

    int rebind_target_ = 0; // 0=none, 1=left, 2=right, 3=destruct

    // Fonts
    ImFont* font_regular = nullptr;
    ImFont* font_bold    = nullptr;
    ImFont* font_title   = nullptr;
    ImFont* font_small   = nullptr;

    static constexpr int kWindowWidth  = 760;
    static constexpr int kWindowHeight = 490;
};
