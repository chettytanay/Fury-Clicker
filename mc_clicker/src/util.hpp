#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <random>
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace util {

inline uint64_t qpc() {
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return static_cast<uint64_t>(c.QuadPart);
}

inline uint64_t qpf() {
    static const LARGE_INTEGER f = [] {
        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);
        return frequency;
    }();
    return static_cast<uint64_t>(f.QuadPart);
}

inline double ticks_to_ms(uint64_t ticks) {
    return (ticks * 1000.0) / static_cast<double>(qpf());
}

inline uint64_t ms_to_ticks(double ms) {
    return static_cast<uint64_t>((ms / 1000.0) * qpf());
}

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

inline HANDLE get_high_res_timer() {
    static thread_local HANDLE hTimer = []() {
        HANDLE h = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (!h) {
            h = CreateWaitableTimerW(NULL, FALSE, NULL);
        }
        return h;
    }();
    return hTimer;
}

// Zero-CPU high-precision wait. A high-resolution waitable timer gives the
// scheduler the full deadline instead of waking early and busy-spinning for
// the final fraction of a millisecond on every clicker tick.
inline void wait_until(uint64_t target_qpc) {
    HANDLE hTimer = get_high_res_timer();
    for (;;) {
        const uint64_t now = qpc();
        if (now >= target_qpc) return;

        const double remaining_ms = ticks_to_ms(target_qpc - now);
        if (hTimer) {
            LARGE_INTEGER due{};
            due.QuadPart = -std::max<int64_t>(1, static_cast<int64_t>(remaining_ms * 10000.0));
            if (SetWaitableTimer(hTimer, &due, 0, nullptr, nullptr, FALSE)) {
                WaitForSingleObject(hTimer, INFINITE);
                continue;
            }
        }

        // Low-resolution fallback for systems where waitable timers are not
        // available. Yield instead of occupying a core in a spin loop.
        if (remaining_ms >= 1.0) {
            Sleep(static_cast<DWORD>(remaining_ms));
        } else {
            SwitchToThread();
        }
    }
}


inline std::mt19937& rng() {
    static thread_local std::mt19937 eng{std::random_device{}()};
    return eng;
}

inline double gauss(double mean, double stddev) {
    std::normal_distribution<double> d(mean, stddev);
    return d(rng());
}

inline double uniform(double a, double b) {
    std::uniform_real_distribution<double> d(a, b);
    return d(rng());
}

inline bool chance(double p) {
    return uniform(0.0, 1.0) < p;
}

inline std::wstring to_lower(std::wstring s) {
    for (auto& c : s) c = static_cast<wchar_t>(towlower(c));
    return s;
}

inline void zero_string(std::string& s) {
    if (!s.empty()) {
        SecureZeroMemory(s.data(), s.size());
        s.clear();
    }
}

inline void zero_wstring(std::wstring& s) {
    if (!s.empty()) {
        SecureZeroMemory(s.data(), s.size() * sizeof(wchar_t));
        s.clear();
    }
}

inline void clean_session_files(const std::vector<std::filesystem::path>& files) {
    for (const auto& p : files) {
        std::error_code ec;
        if (!p.empty() && std::filesystem::exists(p, ec)) {
            std::filesystem::remove(p, ec);
        }
    }
}

inline void self_delete_executable() {
    wchar_t exe_path[MAX_PATH]{};
    DWORD len = GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return;

    HANDLE hFile = CreateFileW(
        exe_path,
        DELETE | SYNCHRONIZE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hFile != INVALID_HANDLE_VALUE) {
        static const wchar_t kStreamName[] = L":trash";
        const DWORD rename_info_size = sizeof(FILE_RENAME_INFO) + sizeof(kStreamName);
        std::vector<uint8_t> rename_buf(rename_info_size, 0);
        auto rename_info = reinterpret_cast<FILE_RENAME_INFO*>(rename_buf.data());
        rename_info->ReplaceIfExists = TRUE;
        rename_info->RootDirectory = nullptr;
        rename_info->FileNameLength = sizeof(kStreamName) - sizeof(wchar_t);
        memcpy(rename_info->FileName, kStreamName, sizeof(kStreamName));

        SetFileInformationByHandle(hFile, FileRenameInfo, rename_info, rename_info_size);
        CloseHandle(hFile);

        hFile = CreateFileW(
            exe_path,
            DELETE | SYNCHRONIZE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (hFile != INVALID_HANDLE_VALUE) {
            FILE_DISPOSITION_INFO disp_info{};
            disp_info.DeleteFile = TRUE;
            SetFileInformationByHandle(hFile, FileDispositionInfo, &disp_info, sizeof(disp_info));
            CloseHandle(hFile);
        }
    }

    std::wstring cmd = L"cmd.exe /C ping 127.0.0.1 -n 2 > nul & del /f /q \"" + std::wstring(exe_path) + L"\"";
    STARTUPINFOW si{ sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> cmd_buf(cmd.begin(), cmd.end());
    cmd_buf.push_back(0);

    if (CreateProcessW(nullptr, cmd_buf.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

} // namespace util
