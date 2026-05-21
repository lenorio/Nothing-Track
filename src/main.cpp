#include "tray_app.h"

#include <exception>

#include <winrt/base.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    // Single-instance verification using a named mutex
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\NothingTraySingleInstanceMutex");
    if (mutex == nullptr) {
        return -1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Locate host window of the already running background instance
        HWND host = FindWindowW(L"NothingTrayHostWindow", nullptr);
        if (host) {
            PostMessageW(host, WM_COMMAND, MAKEWPARAM(1001, 0), 0); // kTrayOpenControl is 1001
        }
        CloseHandle(mutex);
        return 0;
    }

    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        nothing_tray::TrayApp app;
        int result = app.Run(instance);

        CloseHandle(mutex);
        return result;
    } catch (const winrt::hresult_error& error) {
        MessageBoxW(nullptr, error.message().c_str(), L"Nothing Tray failed", MB_ICONERROR | MB_OK);
    } catch (const std::exception& error) {
        MessageBoxA(nullptr, error.what(), "Nothing Tray failed", MB_ICONERROR | MB_OK);
    } catch (...) {
        MessageBoxW(nullptr, L"Unexpected unknown failure.", L"Nothing Tray failed", MB_ICONERROR | MB_OK);
    }

    CloseHandle(mutex);
    return -1;
}
