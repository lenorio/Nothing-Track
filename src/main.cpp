#include "tray_app.h"

#include <exception>

#include <winrt/base.h>

#include "tray_app.h"

#include <exception>
#include <cstdio>
#include <string>
#include <windows.h>
#include <winrt/base.h>

static void LogMain(const std::string& text) {
    OutputDebugStringA((text + "\n").c_str());
    try {
        wchar_t temp_path[MAX_PATH];
        if (GetTempPathW(MAX_PATH, temp_path) > 0) {
            std::wstring log_file = std::wstring(temp_path) + L"nothing_tray.log";
            FILE* f = _wfsopen(log_file.c_str(), L"a", _SH_DENYNO);
            if (f) {
                SYSTEMTIME st;
                GetLocalTime(&st);
                fprintf(f, "[%02d:%02d:%02d.%03d] [MAIN] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, text.c_str());
                fflush(f);
                fclose(f);
            }
        }
    } catch (...) {}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    LogMain("wWinMain started");

    // Single-instance verification using a named mutex
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\NothingTraySingleInstanceMutex");
    if (mutex != nullptr && GetLastError() == ERROR_ALREADY_EXISTS) {
        LogMain("Single instance mutex already exists. Searching for existing host window...");
        HWND host = FindWindowW(L"NothingTrayHostWindow", nullptr);
        if (host && IsWindow(host)) {
            LogMain("Found existing host window. Sending kTrayOpenControl message...");
            PostMessageW(host, WM_COMMAND, MAKEWPARAM(1001, 0), 0); // kTrayOpenControl is 1001
            CloseHandle(mutex);
            return 0;
        }
        LogMain("Existing host window not found. Proceeding with new instance...");
    }

    try {
        LogMain("Initializing WinRT apartment...");
        try {
            winrt::init_apartment();
        } catch (const winrt::hresult_error& e) {
            LogMain("winrt::init_apartment warning: " + winrt::to_string(e.message()));
        }
        LogMain("Creating TrayApp dynamically...");
        auto app = std::make_unique<nothing_tray::TrayApp>();
        LogMain("Running TrayApp loop...");
        int result = app->Run(instance);
        LogMain("Destroying TrayApp...");
        app.reset();

        LogMain("TrayApp exited with code: " + std::to_string(result));
        if (mutex) CloseHandle(mutex);
        return result;
    } catch (const winrt::hresult_error& error) {
        std::string err_msg = winrt::to_string(error.message());
        LogMain("winrt::hresult_error: " + err_msg);
        MessageBoxW(nullptr, error.message().c_str(), L"Nothing Tray failed", MB_ICONERROR | MB_OK);
    } catch (const std::exception& error) {
        LogMain("std::exception: " + std::string(error.what()));
        MessageBoxA(nullptr, error.what(), "Nothing Tray failed", MB_ICONERROR | MB_OK);
    } catch (...) {
        LogMain("Unknown exception caught");
        MessageBoxW(nullptr, L"Unexpected unknown failure.", L"Nothing Tray failed", MB_ICONERROR | MB_OK);
    }

    if (mutex) CloseHandle(mutex);
    return -1;
}
