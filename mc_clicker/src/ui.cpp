#include "ui.hpp"
#include "input.hpp"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <windowsx.h>
#include <filesystem>

#include <string>
#include <cstdio>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_NCCALCSIZE:
        // Strip the standard non-client area so our custom ImGui header acts as the titlebar
        return 0;
    case WM_NCHITTEST: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hWnd, &pt);
        // Header drag zone (excluding the top right minimize and close buttons)
        if (pt.y >= 0 && pt.y <= 46 && pt.x >= 0 && pt.x <= (760 - 70)) {
            return HTCAPTION;
        }
        return HTCLIENT;
    }
    case WM_SIZE:
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

const char* vk_name(int vk) {
    static char buf[32];
    UINT sc = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    if (GetKeyNameTextA(sc << 16, buf, sizeof(buf)) > 0)
        return buf;
    snprintf(buf, sizeof(buf), "0x%02X", vk);
    return buf;
}

const char* activation_state_text(Clicker::ActivationState state, bool left) {
    switch (state) {
    case Clicker::ActivationState::Menu: return "Paused by Minecraft menu";
    case Clicker::ActivationState::Disabled: return "Module disabled";
    case Clicker::ActivationState::WaitingForHold:
        return left ? "Waiting for physical LMB" : "Waiting for physical RMB";
    case Clicker::ActivationState::SlotUnknown: return "Hotbar slot unknown";
    case Clicker::ActivationState::SlotBlocked: return "Current slot not marked";
    case Clicker::ActivationState::Ready: return "Ready to inject clicks";
    default: return "Minecraft has not been focused";
    }
}

void randomization_controls(const char* id, RandomizationSettings& settings) {
    ImGui::PushID(id);
    const char* modes[] = {"Off", "Normal", "Extra", "Extra+"};
    auto slider = [](const char* name, float* value, float min, float max, const char* format) {
        ImGui::TextUnformatted(name);
        ImGui::SetNextItemWidth(-1);
        ImGui::PushID(name);
        ImGui::SliderFloat("##value", value, min, max, format);
        ImGui::PopID();
    };
    ImGui::TextUnformatted("Mode");
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##mode", &settings.mode, modes, IM_ARRAYSIZE(modes));
    if (settings.mode != 0) {
        slider("Strength", &settings.strength, 0.0f, 1.0f, "%.2f");
        if (settings.mode == 1) {
            slider("Gaussian variation", &settings.normal_variation, 0.0f, 2.0f, "%.2f");
        } else if (settings.mode == 2) {
            slider("Gaussian variation", &settings.extra_variation, 0.0f, 2.0f, "%.2f");
            slider("Drift amount", &settings.extra_drift, 0.0f, 2.0f, "%.2f");
            slider("Drift period", &settings.extra_drift_period_ms,
                   200.0f, 10000.0f, "%.0f ms");
        } else {
            slider("Gaussian variation", &settings.plus_variation, 0.0f, 2.0f, "%.2f");
            slider("Drift amount", &settings.plus_drift, 0.0f, 2.0f, "%.2f");
            slider("Drift period", &settings.plus_drift_period_ms,
                   200.0f, 10000.0f, "%.0f ms");
            slider("Dip chance", &settings.plus_dip_chance, 0.0f, 1.0f, "%.2f");
            slider("Dip depth", &settings.plus_dip_depth, 0.0f, 2.0f, "%.2f");
        }
    }
    ImGui::PopID();
}

} // namespace

UI::UI(Config& cfg, Clicker& clicker) : cfg_(cfg), clicker_(clicker) {}

UI::~UI() {
    shutdown();
}

bool UI::create_device(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate = {60, 1};
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL fl;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        levels, 2, D3D11_SDK_VERSION, &sd,
        &swap_, &device_, &fl, &ctx_);
    if (FAILED(hr)) return false;

    ID3D11Texture2D* bb = nullptr;
    swap_->GetBuffer(0, IID_PPV_ARGS(&bb));
    if (!bb) return false;
    device_->CreateRenderTargetView(bb, nullptr, &rtv_);
    bb->Release();
    return true;
}

void UI::cleanup_device() {
    if (rtv_)    { rtv_->Release();    rtv_ = nullptr; }
    if (swap_)   { swap_->Release();   swap_ = nullptr; }
    if (ctx_)    { ctx_->Release();    ctx_ = nullptr; }
    if (device_) { device_->Release(); device_ = nullptr; }
}

void UI::apply_theme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding     = ImVec2(0, 0);
    style.FramePadding      = ImVec2(8, 4);
    style.ItemSpacing       = ImVec2(0, 8);
    style.ItemInnerSpacing  = ImVec2(6, 4);
    style.ScrollbarSize     = 8.0f;

    style.WindowRounding    = 6.0f;
    style.ChildRounding     = 5.0f;
    style.FrameRounding     = 3.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 3.0f;
    style.GrabRounding      = 4.0f;
    style.TabRounding       = 4.0f;

    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 0.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                  = ImVec4(0.88f, 0.89f, 0.92f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.40f, 0.41f, 0.48f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.047f, 0.047f, 0.055f, cfg_.ui_opacity);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.063f, 0.067f, 0.082f, 0.98f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.063f, 0.067f, 0.082f, 0.98f);
    colors[ImGuiCol_Border]                = ImVec4(0.120f, 0.125f, 0.155f, 0.90f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.098f, 0.102f, 0.125f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.135f, 0.140f, 0.175f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.165f, 0.170f, 0.215f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.047f, 0.047f, 0.055f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.047f, 0.047f, 0.055f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.047f, 0.047f, 0.055f, 0.30f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.18f, 0.18f, 0.24f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.25f, 0.25f, 0.32f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.32f, 0.32f, 0.40f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.45f, 0.40f, 0.88f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.45f, 0.40f, 0.88f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.55f, 0.50f, 0.96f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.105f, 0.110f, 0.140f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.145f, 0.150f, 0.190f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.185f, 0.190f, 0.240f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.145f, 0.150f, 0.190f, 0.70f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.185f, 0.190f, 0.240f, 0.80f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.225f, 0.230f, 0.290f, 0.90f);
    colors[ImGuiCol_Separator]             = ImVec4(0.105f, 0.110f, 0.140f, 0.80f);
}

bool UI::init(HINSTANCE hInst) {
    wc_ = { sizeof(WNDCLASSEXW), CS_CLASSDC, WndProc, 0, 0, hInst,
            nullptr, nullptr, nullptr, nullptr, L"fury_clicker_ui_v3", nullptr };
    RegisterClassExW(&wc_);

    hwnd_ = CreateWindowExW(
        WS_EX_APPWINDOW | WS_EX_TOPMOST,
        wc_.lpszClassName, L"FURY CLICKER",
        WS_POPUP | WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU | WS_VISIBLE,
        140, 140, kWindowWidth, kWindowHeight,
        nullptr, nullptr, wc_.hInstance, nullptr);

    if (!hwnd_ || !create_device(hwnd_)) return false;

    ShowWindow(hwnd_, SW_SHOWDEFAULT);
    UpdateWindow(hwnd_);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    // Load Microsoft Segoe UI for smooth anti-aliased font rendering
    ImFontConfig font_cfg;
    font_cfg.OversampleH = 3;
    font_cfg.OversampleV = 3;
    font_cfg.RasterizerMultiply = 1.05f;

    if (std::filesystem::exists("C:\\Windows\\Fonts\\segoeui.ttf")) {
        font_regular = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 15.0f, &font_cfg);
        font_bold    = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 15.0f, &font_cfg);
        font_title   = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 18.0f, &font_cfg);
        font_small   = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 12.0f, &font_cfg);
    }
    if (!font_regular) {
        font_regular = io.Fonts->AddFontDefault();
        font_bold    = font_regular;
        font_title   = font_regular;
        font_small   = font_regular;
    }

    apply_theme();
    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(device_, ctx_);
    return true;
}

void UI::shutdown() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    if (ImGui::GetCurrentContext())
        ImGui::DestroyContext();
    cleanup_device();
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    UnregisterClassW(wc_.lpszClassName, wc_.hInstance);
}

bool UI::sidebar_button(const char* label, int icon_type, bool selected) {
    ImVec2 size(156.0f, 32.0f);
    bool pressed = ImGui::InvisibleButton(label, size);
    bool hovered = ImGui::IsItemHovered();

    ImVec2 min_p = ImGui::GetItemRectMin();
    ImVec2 max_p = ImGui::GetItemRectMax();
    ImDrawList* draw = ImGui::GetWindowDrawList();

    if (selected) {
        draw->AddRectFilled(min_p, max_p, IM_COL32(20, 21, 28, 255), 4.0f);
        // Vertical active accent bar (signature purple)
        draw->AddRectFilled(ImVec2(min_p.x + 2, min_p.y + 6), ImVec2(min_p.x + 5, max_p.y - 6), IM_COL32(104, 94, 216, 255), 2.0f);
    } else if (hovered) {
        draw->AddRectFilled(min_p, max_p, IM_COL32(16, 16, 22, 200), 4.0f);
    }

    // Vector Icon
    const ImU32 icon_color = selected ? IM_COL32(120, 110, 235, 255) : (hovered ? IM_COL32(160, 162, 175, 255) : IM_COL32(100, 102, 115, 255));
    const float center_x = min_p.x + 18.0f;
    const float center_y = min_p.y + 16.0f;

    if (icon_type == 0) { // Target crosshair / left click
        draw->AddCircle(ImVec2(center_x, center_y), 5.5f, icon_color, 16, 1.2f);
        draw->AddCircleFilled(ImVec2(center_x, center_y), 2.0f, icon_color);
    } else if (icon_type == 1) { // Right arrow / right click
        draw->AddTriangleFilled(
            ImVec2(center_x - 3.5f, center_y - 4.5f),
            ImVec2(center_x + 4.5f, center_y),
            ImVec2(center_x - 3.5f, center_y + 4.5f),
            icon_color);
    } else if (icon_type == 2) { // Shield / block-hit
        draw->AddRect(ImVec2(center_x - 4.5f, center_y - 5.0f), ImVec2(center_x + 4.5f, center_y + 4.5f), icon_color, 2.0f, 0, 1.2f);
    } else if (icon_type == 3) { // Keyboard / keybind
        draw->AddRect(ImVec2(center_x - 5.5f, center_y - 4.0f), ImVec2(center_x + 5.5f, center_y + 4.0f), icon_color, 2.0f, 0, 1.2f);
        draw->AddLine(ImVec2(center_x - 3.0f, center_y), ImVec2(center_x + 3.0f, center_y), icon_color, 1.0f);
    } else if (icon_type == 4) { // Gear / sliders
        draw->AddCircle(ImVec2(center_x, center_y), 5.0f, icon_color, 12, 1.2f);
    }

    // Label
    ImVec4 text_col = selected ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : (hovered ? ImVec4(0.85f, 0.86f, 0.90f, 1.0f) : ImVec4(0.55f, 0.56f, 0.62f, 1.0f));
    ImGui::PushFont(selected ? font_bold : font_regular);
    draw->AddText(ImVec2(min_p.x + 34.0f, min_p.y + 7.0f), ImGui::ColorConvertFloat4ToU32(text_col), label);
    ImGui::PopFont();

    return pressed;
}

void UI::section_header(const char* title) {
    ImGui::PushFont(font_small);
    ImGui::TextColored(ImVec4(0.48f, 0.49f, 0.58f, 1.0f), "%s", title);
    ImGui::PopFont();
    ImGui::Spacing();
}

void UI::custom_slider(const char* label, float* v, float v_min, float v_max, const char* fmt) {
    // Header row: Label on left, formatted value on right
    ImGui::TextColored(ImVec4(0.85f, 0.86f, 0.90f, 1.0f), "%s", label);

    char val_buf[32];
    snprintf(val_buf, sizeof(val_buf), fmt, *v);
    float val_w = ImGui::CalcTextSize(val_buf).x;
    float avail_w = ImGui::GetContentRegionAvail().x;
    ImGui::SameLine(ImGui::GetCursorPosX() + avail_w - val_w);
    ImGui::TextColored(ImVec4(0.55f, 0.52f, 0.88f, 1.0f), "%s", val_buf);

    // Full-width modern slider
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 12.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.100f, 0.104f, 0.128f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.130f, 0.135f, 0.165f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.150f, 0.155f, 0.190f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.45f, 0.40f, 0.88f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.55f, 0.50f, 0.96f, 1.0f));

    ImGui::SetNextItemWidth(avail_w);
    std::string slider_id = std::string("##").append(label);
    ImGui::SliderFloat(slider_id.c_str(), v, v_min, v_max, "");

    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(3);
    ImGui::Spacing();
}

void UI::draw_sidebar() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.038f, 0.038f, 0.045f, 1.0f));
    ImGui::BeginChild("sidebar", ImVec2(175, 0), true);

    ImGui::SetCursorPos(ImVec2(14, 20));
    ImGui::PushFont(font_small);
    ImGui::TextColored(ImVec4(0.36f, 0.36f, 0.44f, 1.0f), "COMBAT");
    ImGui::PopFont();
    ImGui::Spacing();

    ImGui::SetCursorPosX(10);
    if (sidebar_button("Left Clicker", 0, current_tab_ == 0)) current_tab_ = 0;
    ImGui::SetCursorPosX(10);
    if (sidebar_button("Right Clicker", 1, current_tab_ == 1)) current_tab_ = 1;
    ImGui::SetCursorPosX(10);
    if (sidebar_button("Block-Hit", 2, current_tab_ == 2)) current_tab_ = 2;

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::SetCursorPosX(14);
    ImGui::PushFont(font_small);
    ImGui::TextColored(ImVec4(0.36f, 0.36f, 0.44f, 1.0f), "SETTINGS");
    ImGui::PopFont();
    ImGui::Spacing();

    ImGui::SetCursorPosX(10);
    if (sidebar_button("Keybinds", 3, current_tab_ == 3)) current_tab_ = 3;
    ImGui::SetCursorPosX(10);
    if (sidebar_button("Profiles & Exit", 4, current_tab_ == 4)) current_tab_ = 4;

    // Bottom telemetry indicators
    ImGui::SetCursorPos(ImVec2(14, kWindowHeight - 55));
    ImGui::PushFont(font_small);
    if (clicker_.is_minecraft_ingame()) {
        ImGui::TextColored(ImVec4(0.28f, 0.85f, 0.48f, 1.0f), "● MC: IN-GAME");
    } else if (clicker_.is_minecraft_focused()) {
        ImGui::TextColored(ImVec4(0.85f, 0.70f, 0.28f, 1.0f), "○ MC: GUI / MENU");
    } else {
        ImGui::TextColored(ImVec4(0.85f, 0.70f, 0.28f, 1.0f), "○ MC: NOT FOCUSED");
    }

    ImGui::SetCursorPos(ImVec2(14, kWindowHeight - 35));
    if (clicker_.is_slot_allowed()) {
        ImGui::TextColored(ImVec4(0.35f, 0.70f, 1.0f, 1.0f), "● SLOT: ALLOWED");
    } else {
        ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.50f, 1.0f), "○ SLOT: BLOCKED");
    }
    ImGui::PopFont();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void UI::draw_header() {
    // Top Bar Header
    ImGui::SetCursorPos(ImVec2(195, 15));
    ImGui::PushFont(font_title);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "FURY");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.48f, 0.42f, 0.90f, 1.0f), "CLICKER");
    ImGui::PopFont();

    // Top Right Window Buttons
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.08f, 0.10f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.16f, 0.22f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.12f, 0.16f, 1.0f));

    ImGui::SetCursorPos(ImVec2(kWindowWidth - 220, 11));
    if (ImGui::Button("FOCUS GAME", ImVec2(145, 29))) {
        HWND game = FindWindowW(L"LWJGL", nullptr);
        if (game) {
            ShowWindow(hwnd_, SW_MINIMIZE);
            ShowWindow(game, SW_RESTORE);
            SetForegroundWindow(game);
        }
    }

    ImGui::SetCursorPos(ImVec2(kWindowWidth - 58, 14));
    if (ImGui::Button("-##min", ImVec2(24, 22))) {
        ShowWindow(hwnd_, SW_MINIMIZE);
    }
    ImGui::SameLine(kWindowWidth - 30);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.18f, 0.18f, 1.0f));
    if (ImGui::Button("X##close", ImVec2(24, 22))) {
        wants_exit_ = true;
    }
    ImGui::PopStyleColor(4);

    // Separator under header
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddLine(
        ImVec2(185.0f, 44.0f),
        ImVec2(static_cast<float>(kWindowWidth), 44.0f),
        IM_COL32(26, 27, 34, 255),
        1.0f);
}

void UI::draw_tab_left_clicker() {
    const float avail_w = (kWindowWidth - 190.0f) - 16.0f;
    const float card_w = (avail_w - 12.0f) * 0.5f;
    const float card_h = kWindowHeight - 60.0f;

    // Card 1: GENERAL
    ImGui::SetCursorPos(ImVec2(190, 50));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    ImGui::BeginChild("card_left_general", ImVec2(card_w, card_h), true);

    section_header("GENERAL");
    ImGui::Checkbox("Enable Left Clicker", &cfg_.left_enabled);
    ImGui::Spacing();
    ImGui::Spacing();

    custom_slider("Min CPS", &cfg_.min_cps, 1.0f, 20.0f, "%.1f");
    custom_slider("Max CPS", &cfg_.max_cps, 1.0f, 20.0f, "%.1f");
    if (cfg_.max_cps < cfg_.min_cps) cfg_.max_cps = cfg_.min_cps;

    ImGui::Spacing();
    ImGui::Spacing();
    section_header("GATING CONDITIONS");

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 8));
    ImGui::Checkbox("Click Only (hold LMB on marked slot)", &cfg_.require_lmb_hold);
    if (!cfg_.require_lmb_hold)
        ImGui::Checkbox("Restrict Auto Mode to Marked Slots", &cfg_.weapon_only);
    ImGui::Checkbox("Pause on Menus / Chat", &cfg_.pause_on_gui);

    if (cfg_.require_lmb_hold || cfg_.weapon_only) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.70f, 0.72f, 0.78f, 1.0f), "Allowed Slots (double-click to toggle):");
        for (int s = 1; s <= 9; ++s) {
            bool active = (cfg_.weapon_slot_mask & (1 << (s - 1))) != 0;
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.38f, 0.32f, 0.82f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.11f, 0.11f, 0.14f, 1.0f));
            }
            char btn_id[16];
            snprintf(btn_id, sizeof(btn_id), "%d##wslot", s);
            ImGui::Button(btn_id, ImVec2(21, 22));
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                cfg_.weapon_slot_mask ^= (1 << (s - 1));
            }
            ImGui::PopStyleColor();
            if (s < 9) ImGui::SameLine(0, 3);
        }

        int cur_slot = clicker_.active_hotbar_slot();
        bool allowed = clicker_.is_slot_allowed();
        if (cur_slot == 0) {
            ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.20f, 1.0f), "Slot unknown: press a hotbar key in game");
        } else if (allowed) {
            ImGui::TextColored(ImVec4(0.28f, 0.85f, 0.48f, 1.0f), "Estimated Slot: %d (Allowed)", cur_slot);
        } else {
            ImGui::TextColored(ImVec4(0.85f, 0.35f, 0.35f, 1.0f), "Estimated Slot: %d (Blocked)", cur_slot);
        }
        ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.60f, 1.0f), "Tracks wheel, 1-9 and numpad keys");
        ImGui::TextWrapped("External mode cannot verify the item in a slot. Mark only slots you reserve for weapons.");
    }

    ImGui::Spacing();
    bool mc_foc = clicker_.is_minecraft_focused();
    bool mc_ing = clicker_.is_minecraft_ingame();
    if (mc_foc) {
        if (mc_ing) {
            ImGui::TextColored(ImVec4(0.28f, 0.85f, 0.48f, 1.0f), "Status: Focused & In-Game");
        } else {
            ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.20f, 1.0f), "Status: Focused (Menu/GUI Open)");
        }
    } else {
        ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.20f, 1.0f), "Paused: focus Minecraft to click");
    }
    if (cfg_.pause_on_gui && !mc_ing)
        ImGui::TextWrapped("Close the Minecraft menu and return to the game. Clicking this settings window pauses clicking.");
    else if (cfg_.require_lmb_hold && !input::is_lmb_down())
        ImGui::TextDisabled("Waiting for physical LMB hold");
    ImGui::TextDisabled("Physical LMB: %s", input::is_lmb_down() ? "DOWN" : "UP");
    ImGui::PopStyleVar();

    ImGui::EndChild();
    ImGui::PopStyleVar();

    // Card 2: HUMANIZATION
    ImGui::SetCursorPos(ImVec2(190 + card_w + 12.0f, 50));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    ImGui::BeginChild("card_left_humanize", ImVec2(card_w, card_h), true);

    section_header("RANDOMIZATION");
    randomization_controls("left", cfg_.left_randomization);

    ImGui::Spacing();
    ImGui::Spacing();
    section_header("LIVE TELEMETRY");
    ImGui::TextColored(ImVec4(0.70f, 0.72f, 0.78f, 1.0f), "Recent 1s: %.0f CPS", clicker_.live_left_cps());
    ImGui::TextColored(ImVec4(0.70f, 0.72f, 0.78f, 1.0f), "Peak Rate:  %.1f CPS", clicker_.peak_left_cps());
    ImGui::TextColored(ImVec4(0.70f, 0.72f, 0.78f, 1.0f), "Total Hits: %llu", clicker_.total_left_clicks());
    ImGui::TextColored(ImVec4(0.70f, 0.72f, 0.78f, 1.0f), "Active Slot Rate: %.0f CPS", clicker_.active_slot_cps());
    ImGui::TextColored(ImVec4(0.70f, 0.72f, 0.78f, 1.0f), "Active Slot Hits: %llu", clicker_.active_slot_clicks());
    ImGui::TextWrapped("Last game state: %s", activation_state_text(clicker_.last_left_state(), true));
    ImGui::Text("Scheduled attempts: %llu", clicker_.left_due_count());
    ImGui::TextColored(ImVec4(0.70f, 0.72f, 0.78f, 1.0f), "Input send errors: %llu", clicker_.input_failures());

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void UI::draw_tab_right_clicker() {
    const float avail_w = (kWindowWidth - 190.0f) - 16.0f;
    const float card_w = (avail_w - 12.0f) * 0.5f;
    const float card_h = kWindowHeight - 60.0f;

    // Card 1: FAST PLACE
    ImGui::SetCursorPos(ImVec2(190, 50));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    ImGui::BeginChild("card_right_general", ImVec2(card_w, card_h), true);

    section_header("FAST PLACE / BRIDGING");
    ImGui::Checkbox("Enable Right Clicker", &cfg_.right_enabled);
    ImGui::Spacing();
    ImGui::Spacing();

    custom_slider("Right Min CPS", &cfg_.right_min_cps, 1.0f, 25.0f, "%.1f");
    custom_slider("Right Max CPS", &cfg_.right_max_cps, 1.0f, 25.0f, "%.1f");
    if (cfg_.right_max_cps < cfg_.right_min_cps) cfg_.right_max_cps = cfg_.right_min_cps;

    ImGui::Spacing();
    ImGui::Spacing();
    section_header("CONDITIONS");

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 10));
    ImGui::Checkbox("Require Physical RMB", &cfg_.require_rmb_hold);
    ImGui::TextColored(ImVec4(0.40f, 0.70f, 0.95f, 1.0f), "In-game Only (Paused in Menus)");
    ImGui::TextDisabled("Physical RMB: %s", input::is_rmb_down() ? "DOWN" : "UP");
    ImGui::PopStyleVar();

    ImGui::EndChild();
    ImGui::PopStyleVar();

    // Card 2: HUMANIZATION
    ImGui::SetCursorPos(ImVec2(190 + card_w + 12.0f, 50));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    ImGui::BeginChild("card_right_humanize", ImVec2(card_w, card_h), true);

    section_header("RANDOMIZATION");
    randomization_controls("right", cfg_.right_randomization);

    ImGui::Spacing();
    ImGui::Spacing();
    section_header("LIVE TELEMETRY");
    ImGui::TextColored(ImVec4(0.70f, 0.72f, 0.78f, 1.0f), "Recent 1s: %.0f CPS", clicker_.live_right_cps());
    ImGui::TextColored(ImVec4(0.70f, 0.72f, 0.78f, 1.0f), "Peak Rate:  %.1f CPS", clicker_.peak_right_cps());
    ImGui::TextColored(ImVec4(0.70f, 0.72f, 0.78f, 1.0f), "Total Hits: %llu", clicker_.total_right_clicks());
    if (!cfg_.right_enabled)
        ImGui::TextWrapped("Right clicker is OFF. Enable it on the left card or press its toggle key.");
    else if (!clicker_.is_minecraft_focused())
        ImGui::TextWrapped("Paused: focus Minecraft and close its menu to start right clicking.");
    else if (cfg_.require_rmb_hold && !input::is_rmb_down())
        ImGui::TextDisabled("Waiting for physical RMB hold");
    ImGui::TextWrapped("Last game state: %s", activation_state_text(clicker_.last_right_state(), false));
    ImGui::Text("Scheduled attempts: %llu", clicker_.right_due_count());
    ImGui::TextColored(ImVec4(0.70f, 0.72f, 0.78f, 1.0f), "Input send errors: %llu", clicker_.input_failures());

    ImGui::EndChild();
    ImGui::PopStyleVar();
}


void UI::draw_tab_blockhit() {
    const float avail_w = (kWindowWidth - 190.0f) - 16.0f;
    const float card_w = (avail_w - 12.0f) * 0.5f;
    const float card_h = kWindowHeight - 60.0f;

    ImGui::SetCursorPos(ImVec2(190, 50));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    ImGui::BeginChild("card_blockhit_main", ImVec2(card_w, card_h), true);

    section_header("BLOCK-HIT TIMING");
    ImGui::Checkbox("Enable Block-Hit", &cfg_.blockhit);
    ImGui::Spacing();
    ImGui::Spacing();

    custom_slider("Trigger Chance", &cfg_.blockhit_chance, 0.0f, 1.0f, "%.2f");
    custom_slider("Release to Attack", &cfg_.blockhit_attack_delay_ms, 0.0f, 25.0f, "%.0f ms");
    custom_slider("Minimum Block Hold", &cfg_.blockhit_duration_ms, 30.0f, 200.0f, "%.0f ms");
    custom_slider("Cooldown", &cfg_.blockhit_cooldown_ms, 30.0f, 500.0f, "%.0f ms");

    ImGui::Spacing();
    ImGui::TextWrapped("While holding physical RMB, releases use-item, attacks, then re-applies use-item. Requires the left clicker to be active.");

    ImGui::EndChild();
    ImGui::PopStyleVar();

    ImGui::SetCursorPos(ImVec2(190 + card_w + 12.0f, 50));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    ImGui::BeginChild("card_blockhit_info", ImVec2(card_w, card_h), true);

    section_header("BEHAVIOR");
    ImGui::BulletText("Left attack is never held behind a right-click pulse");
    ImGui::BulletText("Right clicker yields while block-hit uses RMB");
    ImGui::BulletText("Releases RMB on physical release or focus loss");
    ImGui::BulletText("Timing depends on the game and server");
    ImGui::Spacing();
    ImGui::Text("Completed block-hits: %llu", clicker_.total_blockhits());

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void UI::draw_tab_keybinds() {
    const float avail_w = (kWindowWidth - 190.0f) - 16.0f;
    const float card_w = (avail_w - 12.0f) * 0.5f;
    const float card_h = kWindowHeight - 60.0f;

    ImGui::SetCursorPos(ImVec2(190, 50));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    ImGui::BeginChild("card_binds_main", ImVec2(card_w, card_h), true);

    section_header("HOTKEYS");

    ImGui::TextColored(ImVec4(0.85f, 0.86f, 0.90f, 1.0f), "Left Clicker Toggle");
    ImGui::TextColored(ImVec4(0.48f, 0.42f, 0.90f, 1.0f), "[ %s ]", vk_name(cfg_.toggle_vk));
    if (ImGui::Button(rebind_target_ == 1 ? "Press Key (Esc to cancel)..." : "Rebind Left Toggle", ImVec2(-1, 28))) {
        rebind_target_ = (rebind_target_ == 1) ? 0 : 1;
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.85f, 0.86f, 0.90f, 1.0f), "Right Clicker Toggle");
    ImGui::TextColored(ImVec4(0.48f, 0.42f, 0.90f, 1.0f), "[ %s ]", vk_name(cfg_.right_toggle_vk));
    if (ImGui::Button(rebind_target_ == 2 ? "Press Key (Esc to cancel)..." : "Rebind Right Toggle", ImVec2(-1, 28))) {
        rebind_target_ = (rebind_target_ == 2) ? 0 : 2;
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.85f, 0.86f, 0.90f, 1.0f), "Exit / Self-Destruct Hotkey");
    ImGui::TextColored(ImVec4(0.85f, 0.35f, 0.35f, 1.0f), "[ %s ]", vk_name(cfg_.destruct_vk));
    if (ImGui::Button(rebind_target_ == 3 ? "Press Key (Esc to cancel)..." : "Rebind Exit Hotkey", ImVec2(-1, 28))) {
        rebind_target_ = (rebind_target_ == 3) ? 0 : 3;
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();

    ImGui::SetCursorPos(ImVec2(190 + card_w + 12.0f, 50));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    ImGui::BeginChild("card_binds_info", ImVec2(card_w, card_h), true);

    section_header("SECURITY & REBIND RULES");
    ImGui::BulletText("Exit hotkey only triggers when this window is focused");
    ImGui::BulletText("Key presses in other apps are strictly ignored");
    ImGui::BulletText("Toggle keys are gated to game & clicker windows");
    ImGui::BulletText("Click 'Rebind' then press key (Esc cancels)");
    ImGui::BulletText("Mouse buttons are excluded from hotkey binds");

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void UI::draw_tab_config() {
    const float avail_w = (kWindowWidth - 190.0f) - 16.0f;
    const float card_w = (avail_w - 12.0f) * 0.5f;
    const float card_h = kWindowHeight - 60.0f;

    ImGui::SetCursorPos(ImVec2(190, 50));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    ImGui::BeginChild("card_config_main", ImVec2(card_w, card_h), true);

    section_header("PROFILE MANAGEMENT");
    if (ImGui::Button("Save Configuration", ImVec2(-1, 32))) {
        cfg_.clamp();
        cfg_.save("mc_clicker.json");
    }

    ImGui::Spacing();
    if (ImGui::Button("Load Configuration", ImVec2(-1, 32))) {
        cfg_.load("mc_clicker.json");
        cfg_.clamp();
        apply_theme();
    }

    ImGui::Spacing();
    if (ImGui::Button("Reset to Defaults", ImVec2(-1, 32))) {
        cfg_ = Config{};
        apply_theme();
    }

    ImGui::Spacing();
    ImGui::Spacing();
    section_header("APPEARANCE");
    custom_slider("Window Opacity", &cfg_.ui_opacity, 0.35f, 1.0f, "%.2f");

    ImGui::EndChild();
    ImGui::PopStyleVar();

    ImGui::SetCursorPos(ImVec2(190 + card_w + 12.0f, 50));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    ImGui::BeginChild("card_config_danger", ImVec2(card_w, card_h), true);

    section_header("CLEANUP & EXIT POLICY");
    ImGui::Checkbox("Clean session & temp files on exit", &cfg_.clean_on_exit);
    ImGui::Spacing();
    ImGui::Checkbox("Self-delete executable after exit", &cfg_.self_delete_exe);
    ImGui::Spacing();
    ImGui::Checkbox("Gate toggle hotkeys to active app", &cfg_.toggle_only_when_active);

    ImGui::Spacing();
    ImGui::Spacing();
    section_header("PROCESS TERMINATION");
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.48f, 0.14f, 0.14f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.62f, 0.18f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.38f, 0.10f, 0.10f, 1.0f));
    if (ImGui::Button("SELF-DESTRUCT & CLEAN EXIT", ImVec2(-1, 36))) {
        request_exit(true);
    }
    ImGui::PopStyleColor(3);

    ImGui::Spacing();
    ImGui::TextWrapped("Self-destruct unhooks memory handles, flushes or wipes session traces, and cleanly exits.");

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void UI::draw_panel() {
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(kWindowWidth), static_cast<float>(kWindowHeight)), ImGuiCond_Always);

    ImGui::Begin("##fury_viewport", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    draw_sidebar();
    draw_header();

    switch (current_tab_) {
    case 0: draw_tab_left_clicker(); break;
    case 1: draw_tab_right_clicker(); break;
    case 2: draw_tab_blockhit(); break;
    case 3: draw_tab_keybinds(); break;
    case 4: draw_tab_config(); break;
    }

    ImGui::End();

    // Hotkey Rebind Capture - only active when clicker window is focused
    if (rebind_target_ != 0 && is_focused()) {
        for (int vk = 1; vk < 256; ++vk) {
            if (input::is_key_down(vk)) {
                if (vk == VK_ESCAPE) {
                    rebind_target_ = 0;
                    break;
                }
                if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON) continue;

                if (rebind_target_ == 1) cfg_.toggle_vk = vk;
                else if (rebind_target_ == 2) cfg_.right_toggle_vk = vk;
                else if (rebind_target_ == 3) cfg_.destruct_vk = vk;

                rebind_target_ = 0;
                while (input::is_key_down(vk)) {
                    Sleep(5);
                }
                break;
            }
        }
    }
}

void UI::render_frame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    draw_panel();

    ImGui::Render();
    const float clear[4] = { 0.047f, 0.047f, 0.055f, cfg_.ui_opacity };
    ctx_->OMSetRenderTargets(1, &rtv_, nullptr);
    ctx_->ClearRenderTargetView(rtv_, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    swap_->Present(1, 0);
}

void UI::pump() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            wants_exit_ = true;
            return;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (!IsWindow(hwnd_)) {
        wants_exit_ = true;
        return;
    }
    // Minimized: 100% idle, zero GPU/DX11 rendering
    if (IsIconic(hwnd_)) {
        Sleep(50);
        return;
    }
    // If unfocused (e.g. while playing Minecraft), throttle to ~10 FPS so telemetry updates cleanly with 0 CPU cost
    static uint64_t last_unfocused_render = 0;
    if (GetForegroundWindow() != hwnd_) {
        uint64_t now = util::qpc();
        if (util::ticks_to_ms(now - last_unfocused_render) < 100.0) {
            Sleep(25);
            return;
        }
        last_unfocused_render = now;
    }
    render_frame();
}
