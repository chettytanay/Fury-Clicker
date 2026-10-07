#include "input.hpp"
#include <atomic>
#include <future>
#include <thread>

namespace input {
namespace {
std::atomic<bool> physical_left{false};
std::atomic<bool> physical_right{false};
std::atomic<int> wheel_delta{0};
std::atomic<bool> hook_active{false};
std::thread mouse_thread;
DWORD mouse_thread_id = 0;

LRESULT CALLBACK physical_mouse_hook(int code, WPARAM message, LPARAM lp) {
    if (code == HC_ACTION) {
        const auto* event = reinterpret_cast<const MSLLHOOKSTRUCT*>(lp);
        // SendInput changes the logical button state. Only hardware events
        // may change the activation state used by Click Only.
        if (!(event->flags & LLMHF_INJECTED)) {
            if (message == WM_LBUTTONDOWN) physical_left.store(true);
            if (message == WM_LBUTTONUP) physical_left.store(false);
            if (message == WM_RBUTTONDOWN) physical_right.store(true);
            if (message == WM_RBUTTONUP) physical_right.store(false);
        }
    }
    return CallNextHookEx(nullptr, code, message, lp);
}

LRESULT CALLBACK mouse_window_proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    if (message == WM_INPUT) {
        RAWINPUT raw{};
        UINT size = sizeof(raw);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT,
                            &raw, &size, sizeof(RAWINPUTHEADER)) != UINT(-1) &&
            raw.header.dwType == RIM_TYPEMOUSE) {
            const USHORT flags = raw.data.mouse.usButtonFlags;
            if (!hook_active.load()) {
                if (flags & RI_MOUSE_LEFT_BUTTON_DOWN) physical_left.store(true);
                if (flags & RI_MOUSE_LEFT_BUTTON_UP) physical_left.store(false);
                if (flags & RI_MOUSE_RIGHT_BUTTON_DOWN) physical_right.store(true);
                if (flags & RI_MOUSE_RIGHT_BUTTON_UP) physical_right.store(false);
            }
            if (flags & RI_MOUSE_WHEEL)
                wheel_delta.fetch_add(static_cast<SHORT>(raw.data.mouse.usButtonData));
        }
    }
    return DefWindowProcW(hwnd, message, wp, lp);
}
} // namespace

bool start_mouse_tracking() {
    if (mouse_thread.joinable()) return true;
    std::promise<bool> ready;
    auto result = ready.get_future();
    mouse_thread = std::thread([ready = std::move(ready)]() mutable {
        mouse_thread_id = GetCurrentThreadId();
        const HINSTANCE instance = GetModuleHandleW(nullptr);
        WNDCLASSW wc{};
        wc.lpfnWndProc = mouse_window_proc;
        wc.hInstance = instance;
        wc.lpszClassName = L"ClickerPhysicalMouse";
        if (!RegisterClassW(&wc)) {
            ready.set_value(false);
            return;
        }
        HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0,
                                    HWND_MESSAGE, nullptr, instance, nullptr);
        RAWINPUTDEVICE device{0x01, 0x02, RIDEV_INPUTSINK, hwnd};
        const bool registered = hwnd && RegisterRawInputDevices(&device, 1, sizeof(device));
        physical_left.store((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
        physical_right.store((GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
        // Keep the callback tiny: a global hook that blocks here stalls mouse
        // input system-wide. Raw Input remains responsible for wheel tracking.
        HHOOK hook = SetWindowsHookExW(WH_MOUSE_LL, physical_mouse_hook, instance, 0);
        hook_active.store(hook != nullptr);
        ready.set_value(registered);
        if (registered) {
            MSG msg{};
            while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            device.dwFlags = RIDEV_REMOVE;
            device.hwndTarget = nullptr;
            RegisterRawInputDevices(&device, 1, sizeof(device));
        }
        hook_active.store(false);
        if (hook) UnhookWindowsHookEx(hook);
        if (hwnd) DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, instance);
    });
    if (result.get()) return true;
    mouse_thread.join();
    return false;
}

void stop_mouse_tracking() {
    if (mouse_thread.joinable()) {
        PostThreadMessageW(mouse_thread_id, WM_QUIT, 0, 0);
        mouse_thread.join();
    }
    physical_left.store(false);
    physical_right.store(false);
    wheel_delta.store(0);
}

bool is_lmb_down() {
    if (hook_active.load()) return physical_left.load();
    return physical_left.load() || (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
}
bool is_rmb_down() {
    if (hook_active.load()) return physical_right.load();
    return physical_right.load() || (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
}
int consume_wheel_delta() { return wheel_delta.exchange(0); }

bool foreground_is_minecraft(const std::vector<std::wstring>& whitelist) {
    HWND fg = GetForegroundWindow();
    if (!fg) return false;

    wchar_t cls[64]{};
    if (GetClassNameW(fg, cls, 64)) {
        if (_wcsicmp(cls, L"LWJGL") == 0)
            return true;
    }

    wchar_t title[256]{};
    if (GetWindowTextW(fg, title, 256)) {
        std::wstring lower = util::to_lower(title);
        if (lower.find(L"minecraft") != std::wstring::npos ||
            lower.find(L"lunar client") != std::wstring::npos ||
            lower.find(L"badlion") != std::wstring::npos ||
            lower.find(L"feather") != std::wstring::npos ||
            lower.find(L"labymod") != std::wstring::npos ||
            lower.find(L"cheatbreaker") != std::wstring::npos ||
            lower.find(L"pvp lounge") != std::wstring::npos) {
            return true;
        }
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    if (!pid) return false;

    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return false;

    wchar_t path[MAX_PATH]{};
    DWORD size = MAX_PATH;
    bool match = false;
    if (QueryFullProcessImageNameW(h, 0, path, &size)) {
        std::wstring name = util::to_lower(path);
        for (const auto& w : whitelist) {
            if (name.find(util::to_lower(w)) != std::wstring::npos) {
                match = true;
                break;
            }
        }
    }
    CloseHandle(h);
    return match;
}

} // namespace input
