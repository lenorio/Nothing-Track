#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <windows.h>
#include <shellapi.h>

#include <objidl.h>
#include <gdiplus.h>

#include "app_state.h"
#include "ble_monitor.h"
#include "spp_client.h"

namespace nothing_tray {

class TrayApp {
public:
    enum class EqPreset : uint8_t {
        Balanced = 0,
        MoreBass = 1,
        MoreTreble = 2,
        Voice = 3,
    };

    TrayApp();
    ~TrayApp();

    TrayApp(const TrayApp&) = delete;
    TrayApp& operator=(const TrayApp&) = delete;

    int Run(HINSTANCE instance);

private:
    enum : UINT {
        kTrayIconMessage = WM_APP + 1,
        kBleSnapshotMessage = WM_APP + 2,
        kSppStatusMessage = WM_APP + 3,
    };

    enum : int {
        kTrayOpenControl = 1001,
        kTrayExit = 1002,
        kBatterySmoothingTimerId = 2001,
    };

    void RegisterWindowClasses();
    void CreateHostWindow();
    void CreateControlWindow();
    void DestroyControlWindow();
    void ShowControlWindow();

    void AddTrayIcon();
    void RemoveTrayIcon();
    void UpdateTrayIcon();
    HICON CreateTrayIcon();

    void ApplySnapshot(const BatterySnapshot& snapshot);
    void OnBleSnapshot(const BatterySnapshot& snapshot);
    void AddBatterySamples(const BatteryReading& left, const BatteryReading& right, const BatteryReading& case_battery);
    void ProcessSmoothedBattery();
    void OnSppStatusChanged(bool connected);
    void OnSppStateUpdate(const SppStateUpdate& update);

    void RefreshControlWindow();
    void UpdateControlStatusText();
    void QueueConnection();
    void DisconnectSpp();

    AncMode SelectedAncMode() const;
    std::pair<bool, uint8_t> SelectedBassState() const;
    void ApplySelectedAnc();
    void ApplySelectedBass();
    void ApplyEqPreset(EqPreset preset);
    void TriggerRing(bool left);

    HWND GetActiveWindow() const;
    void PostUiMessage(UINT message, WPARAM wparam = 0, LPARAM lparam = 0);
    void SetTrayTooltip(const std::wstring& tooltip);

    void LoadUiResources();
    void ReleaseUiResources();
    void LayoutControlWindow();

    std::wstring ResolveWorkspacePath(const wchar_t* relative_path);

    static LRESULT CALLBACK HostWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    static LRESULT CALLBACK ControlWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

    HINSTANCE instance_{nullptr};
    HWND host_window_{nullptr};
    HWND control_window_{nullptr};
    NOTIFYICONDATAW tray_data_{};
    HICON current_tray_icon_{nullptr};

    BleMonitor ble_monitor_{};
    SppClient spp_client_{};

    std::mutex state_mutex_{};
    BatterySnapshot snapshot_{};
    std::mutex smoothing_mutex_{};
    std::vector<BatteryReading> left_samples_{};
    std::vector<BatteryReading> right_samples_{};
    std::vector<BatteryReading> case_samples_{};

    BatteryReading last_valid_left_{};
    BatteryReading last_valid_right_{};
    BatteryReading last_valid_case_{};
    std::chrono::steady_clock::time_point last_valid_left_time_{};
    std::chrono::steady_clock::time_point last_valid_right_time_{};
    std::chrono::steady_clock::time_point last_valid_case_time_{};

    std::thread spp_connect_thread_{};
    std::atomic_bool spp_connect_pending_{false};
    std::atomic_bool shutting_down_{false};

    // Отрисовка
    ULONG_PTR gdiplus_token_{0};
    Gdiplus::PrivateFontCollection font_collection_{};
    Gdiplus::Font* font_heading_{nullptr};
    Gdiplus::Font* font_body_{nullptr};
    Gdiplus::Font* font_button_{nullptr};
    Gdiplus::Font* font_small_{nullptr};
    HBRUSH brush_canvas_{nullptr};
    HBRUSH brush_surface_{nullptr};
    HBRUSH brush_surface_elevated_{nullptr};
    HBRUSH brush_surface_card_{nullptr};

    std::wstring workspace_root_{};
    std::wstring asset_root_{};
    std::wstring font_root_{};

    Gdiplus::Image* image_left_bud_{nullptr};
    Gdiplus::Image* image_right_bud_{nullptr};
    Gdiplus::Image* image_case_bud_{nullptr};
    Gdiplus::Image* image_duo_bud_{nullptr};

    std::wstring device_serial_{};
    std::wstring device_sku_{};

    std::wstring ResolveSkuFromSerial(const std::wstring& serial);
    void ReloadBudImages(const std::wstring& sku);
    bool IsSwappedAncSku(const std::wstring& sku);

    AncMode selected_anc_{AncMode::Transparency};
    bool bass_enabled_{true};
    uint8_t bass_level_{3};
    bool personalized_anc_{false};
    bool low_latency_enabled_{false};
    EqPreset selected_eq_{EqPreset::Balanced};
    std::atomic_bool left_ringing_{false};
    std::atomic_bool right_ringing_{false};

    // Сетка разметки
    RECT header_rect_{};
    RECT device_area_rect_{};
    RECT left_bud_rect_{};
    RECT right_bud_rect_{};
    RECT left_ring_rect_{};
    RECT right_ring_rect_{};
    RECT anc_card_rect_{};
    RECT eq_card_rect_{};
    RECT anc_toggle_rect_{};
    RECT eq_custom_rect_{};
    RECT low_latency_rect_{};
    RECT minimize_btn_rect_{};
    RECT close_btn_rect_{};

    std::array<RECT, 3> anc_top_rects_{};
    std::array<RECT, 4> anc_mode_rects_{};
    std::array<RECT, 4> eq_preset_rects_{};
};

} // namespace nothing_tray