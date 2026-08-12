#include "tray_app.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cwctype>
#include <filesystem>
#include <string>
#include <utility>

#include <objidl.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <windowsx.h>
#include <winrt/base.h>

#include <winrt/Windows.Foundation.h>

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

using namespace Gdiplus;

namespace nothing_tray {
namespace {
constexpr wchar_t kHostClassName[] = L"NothingTrayHostWindow";
constexpr wchar_t kControlClassName[] = L"NothingTrayControlWindow";
constexpr wchar_t kTrayTitle[] = L"Nothing / CMF Earbuds";
constexpr int kWindowWidth = 650;
constexpr int kWindowHeight = 580; // Слегка увеличили высоту для идеального соотношения
constexpr int kMargin = 24;

void DebugLog(const std::wstring& msg) {
    OutputDebugStringW((msg + L"\n").c_str());
    try {
        std::string text = winrt::to_string(msg);
        wchar_t temp_path[MAX_PATH];
        if (GetTempPathW(MAX_PATH, temp_path) > 0) {
            std::wstring log_file = std::wstring(temp_path) + L"nothing_tray.log";
            FILE* f = _wfsopen(log_file.c_str(), L"a", _SH_DENYNO);
            if (f) {
                SYSTEMTIME st;
                GetLocalTime(&st);
                fprintf(f, "[%02d:%02d:%02d.%03d] [TRAY] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, text.c_str());
                fflush(f);
                fclose(f);
            }
        }
    } catch (...) {}
}

std::wstring GuessSkuFromName(const std::wstring& name) {
    std::wstring lower_name = name;
    for (auto& c : lower_name) {
        c = static_cast<wchar_t>(std::towlower(c));
    }

    // 1. CMF Buds Pro 2 / CMF Buds 2 / CMF Buds 2a (espeon)
    if (lower_name.find(L"cmf buds pro 2") != std::wstring::npos ||
        lower_name.find(L"cmf buds 2") != std::wstring::npos ||
        lower_name.find(L"cmf buds 2a") != std::wstring::npos) {
        if (lower_name.find(L"orange") != std::wstring::npos) {
            return L"78";
        } else if (lower_name.find(L"blue") != std::wstring::npos) {
            return L"79";
        } else if (lower_name.find(L"white") != std::wstring::npos) {
            return L"77";
        } else {
            return L"76"; // default to black
        }
    }

    // 2. CMF Buds Pro (classic, corsola)
    if (lower_name.find(L"cmf buds pro") != std::wstring::npos) {
        if (lower_name.find(L"orange") != std::wstring::npos) {
            return L"34";
        } else if (lower_name.find(L"white") != std::wstring::npos || lower_name.find(L"light") != std::wstring::npos) {
            return L"32";
        } else {
            return L"30"; // default to black
        }
    }

    // 3. CMF Buds (classic, donphan)
    if (lower_name.find(L"cmf buds") != std::wstring::npos) {
        if (lower_name.find(L"orange") != std::wstring::npos) {
            return L"58";
        } else if (lower_name.find(L"white") != std::wstring::npos) {
            return L"56";
        } else {
            return L"54"; // default to black
        }
    }

    // 4. Nothing Ear (2) (white/black)
    if (lower_name.find(L"ear (2)") != std::wstring::npos || lower_name.find(L"ear(2)") != std::wstring::npos) {
        if (lower_name.find(L"black") != std::wstring::npos || lower_name.find(L"dark") != std::wstring::npos) {
            return L"27";
        } else {
            return L"17"; // default to white
        }
    }

    // 5. Nothing Ear (a) (crobat)
    if (lower_name.find(L"ear (a)") != std::wstring::npos || lower_name.find(L"ear(a)") != std::wstring::npos) {
        if (lower_name.find(L"black") != std::wstring::npos || lower_name.find(L"dark") != std::wstring::npos) {
            return L"50";
        } else if (lower_name.find(L"yellow") != std::wstring::npos || lower_name.find(L"orange") != std::wstring::npos) {
            return L"48";
        } else {
            return L"49"; // default to white
        }
    }

    // 6. Nothing Ear (1)
    if (lower_name.find(L"ear (1)") != std::wstring::npos || lower_name.find(L"ear(1)") != std::wstring::npos) {
        if (lower_name.find(L"black") != std::wstring::npos || lower_name.find(L"dark") != std::wstring::npos) {
            return L"02";
        } else {
            return L"01"; // default to white
        }
    }

    // 7. Nothing Ear (open) (cleffa)
    if (lower_name.find(L"ear (open)") != std::wstring::npos || lower_name.find(L"ear(open)") != std::wstring::npos || lower_name.find(L"ear open") != std::wstring::npos) {
        if (lower_name.find(L"black") != std::wstring::npos || lower_name.find(L"dark") != std::wstring::npos) {
            return L"63";
        } else if (lower_name.find(L"yellow") != std::wstring::npos) {
            return L"65";
        } else {
            return L"64"; // default to white
        }
    }

    // 8. Nothing Ear Stick
    if (lower_name.find(L"stick") != std::wstring::npos) {
        return L"14";
    }

    // 9. Nothing Ear (flagship, entei)
    if (lower_name.find(L"ear") != std::wstring::npos) {
        if (lower_name.find(L"black") != std::wstring::npos || lower_name.find(L"dark") != std::wstring::npos) {
            return L"61";
        } else {
            return L"62"; // default to white
        }
    }

    return L""; // no match
}

// Палитра дизайна по спецификации Nothing (DESIGN.md)
inline Color kCanvas() { return Color(255, 0x00, 0x00, 0x00); }               // Настоящий чёрный
inline Color kCardBg() { return Color(255, 0x14, 0x14, 0x16); }               // Тёмно-серый для карточек (#141416)
inline Color kCardBorder() { return Color(255, 0x2A, 0x2A, 0x2E); }           // Граница карточек
inline Color kTextWhite() { return Color(255, 0xF4, 0xF4, 0xF6); }            // Основной белый шрифт
inline Color kTextMuted() { return Color(255, 0x8A, 0x8A, 0x8F); }            // Приглушённый серый
inline Color kDotActive() { return Color(255, 0xFF, 0xFF, 0xFF); }            // Активная точка
inline Color kPillActive() { return Color(255, 0xFF, 0xFF, 0xFF); }           // Активная подложка-пилюля
inline Color kDarkRedFill() { return Color(255, 0x22, 0x08, 0x08); }          // Тёмно-красный для Ring
inline Color kDarkRedStroke() { return Color(255, 0x5C, 0x14, 0x14); }        // Обводка Ring

std::filesystem::path ModuleDirectory() {
    wchar_t buffer[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return std::filesystem::path(buffer).parent_path();
}

bool FileExists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

std::filesystem::path FindWorkspaceRoot() {
    auto current = ModuleDirectory();
    for (int i = 0; i < 6; ++i) {
        auto fonts_path = current / L"fonts";
        auto assets_path = current / L"assets";
        if (FileExists(fonts_path) && FileExists(assets_path)) {
            return current;
        }
        if (!current.has_parent_path()) break;
        current = current.parent_path();
    }
    return ModuleDirectory();
}

bool PtInAnyRect(const RECT& rect, POINT point) {
    return point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom;
}

void PositionNearTray(HWND hwnd) {
    RECT rect{};
    GetWindowRect(hwnd, &rect);
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    POINT cursor{};
    GetCursorPos(&cursor);
    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);
    const int x = info.rcWork.right - width - 16;
    const int y = info.rcWork.bottom - height - 16;
    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

void AddRoundRectPath(GraphicsPath& path, const RectF& rect, float radius) {
    const float diameter = radius * 2.0f;
    RectF arc(rect.X, rect.Y, diameter, diameter);
    path.AddArc(arc, 180.0f, 90.0f);
    arc.X = rect.GetRight() - diameter;
    path.AddArc(arc, 270.0f, 90.0f);
    arc.Y = rect.GetBottom() - diameter;
    path.AddArc(arc, 0.0f, 90.0f);
    arc.X = rect.X;
    path.AddArc(arc, 90.0f, 90.0f);
    path.CloseFigure();
}

void FillRoundedRect(Graphics& graphics, const RECT& rect, float radius, const Color& fill, const Color& stroke = Color(0, 0, 0, 0)) {
    GraphicsPath path;
    AddRoundRectPath(path, RectF(static_cast<REAL>(rect.left), static_cast<REAL>(rect.top),
                                 static_cast<REAL>(rect.right - rect.left), static_cast<REAL>(rect.bottom - rect.top)),
                     radius);
    SolidBrush brush(fill);
    graphics.FillPath(&brush, &path);
    if (stroke.GetAlpha() > 0) {
        Pen pen(stroke, 1.2f);
        graphics.DrawPath(&pen, &path);
    }
}

Color kAccentRed() { return Color(255, 0xAC, 0x3C, 0x3B); }

// Полностью перерисовываем наушники вектором в стиле Nothing (если WebP картинки отсутствуют)
void DrawVectorEarbud(Graphics& graphics, int x, int y, bool is_right) {
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);

    // CMF Orange color (#F9623F) earbud head & stem
    RectF head_rect(static_cast<REAL>(x + 15), static_cast<REAL>(y + 15), 45.0f, 45.0f);
    SolidBrush head_brush(Color(255, 0xF9, 0x62, 0x3F));
    graphics.FillEllipse(&head_brush, head_rect);

    // Inner earbud head
    RectF inner_head(static_cast<REAL>(x + 22), static_cast<REAL>(y + 22), 31.0f, 31.0f);
    SolidBrush inner_brush(Color(255, 0xE8, 0x50, 0x2C));
    graphics.FillEllipse(&inner_brush, inner_head);

    // CMF Stem
    GraphicsPath stem_path;
    AddRoundRectPath(stem_path, RectF(static_cast<REAL>(x + 27), static_cast<REAL>(y + 45), 21.0f, 65.0f), 8.0f);
    SolidBrush stem_brush(Color(255, 0xF9, 0x62, 0x3F));
    Pen stem_pen(Color(255, 0xD0, 0x40, 0x20), 1.0f);
    graphics.FillPath(&stem_brush, &stem_path);
    graphics.DrawPath(&stem_pen, &stem_path);

    // Accent dot
    RectF dot_rect(static_cast<REAL>(x + 34), static_cast<REAL>(y + 55), 7.0f, 7.0f);
    SolidBrush dot_brush(is_right ? Color(255, 0xFF, 0xFF, 0xFF) : Color(255, 0x1C, 0x1C, 0x1C));
    graphics.FillEllipse(&dot_brush, dot_rect);
}

void DrawCenteredText(HDC hdc, HFONT font, const std::wstring& text, RECT rect, COLORREF color) {
    HGDIOBJ old_font = SelectObject(hdc, font);
    COLORREF old_color = SetTextColor(hdc, color);
    int old_bk_mode = SetBkMode(hdc, TRANSPARENT);
    
    DrawTextW(hdc, text.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    
    SetBkMode(hdc, old_bk_mode);
    SetTextColor(hdc, old_color);
    SelectObject(hdc, old_font);
}

void DrawTextLine(HDC hdc, HFONT font, const std::wstring& text, int x, int y, COLORREF color) {
    HGDIOBJ old_font = SelectObject(hdc, font);
    COLORREF old_color = SetTextColor(hdc, color);
    int old_bk_mode = SetBkMode(hdc, TRANSPARENT);
    
    TextOutW(hdc, x, y, text.c_str(), static_cast<int>(text.length()));
    
    SetBkMode(hdc, old_bk_mode);
    SetTextColor(hdc, old_color);
    SelectObject(hdc, old_font);
}

} // namespace

std::wstring TrayApp::ResolveWorkspacePath(const wchar_t* relative_path) {
    const std::filesystem::path relative(relative_path);
    auto current = ModuleDirectory();
    for (int i = 0; i < 6; ++i) {
        const auto candidate = current / relative;
        if (FileExists(candidate)) {
            return candidate.wstring();
        }
        if (!current.has_parent_path()) break;
        current = current.parent_path();
    }
    return (ModuleDirectory() / relative).wstring();
}

TrayApp::TrayApp() {
    DebugLog(L"=== TrayApp Custom Init started ===");
    
    GdiplusStartupInput startup_input{};
    GdiplusStartup(&gdiplus_token_, &startup_input, nullptr);
    font_collection_ = new Gdiplus::PrivateFontCollection();

    ble_monitor_ = std::make_unique<BleMonitor>();
    spp_client_ = std::make_unique<SppClient>();

    tray_data_.cbSize = sizeof(tray_data_);
    tray_data_.uID = 1;
    tray_data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    tray_data_.uCallbackMessage = kTrayIconMessage;
    tray_data_.uVersion = NOTIFYICON_VERSION_4;

    LANGID lang_id = GetUserDefaultUILanguage();
    if (PRIMARYLANGID(lang_id) == LANG_RUSSIAN) {
        language_ = AppLanguage::Russian;
    } else {
        language_ = AppLanguage::English;
    }
}

TrayApp::~TrayApp() {
    shutting_down_.store(true, std::memory_order_release);
    if (host_window_) {
        KillTimer(host_window_, kBatterySmoothingTimerId);
    }
    DisconnectSpp();
    if (ble_monitor_) {
        ble_monitor_->Stop();
        ble_monitor_.reset();
    }
    DestroyControlWindow();
    RemoveTrayIcon();
    if (current_tray_icon_) {
        DestroyIcon(current_tray_icon_);
        current_tray_icon_ = nullptr;
    }
    if (spp_connect_thread_.joinable()) {
        spp_connect_thread_.join();
    }
    spp_client_.reset();
    ReleaseUiResources();
    if (font_collection_) {
        delete font_collection_;
        font_collection_ = nullptr;
    }
    if (gdiplus_token_) {
        GdiplusShutdown(gdiplus_token_);
        gdiplus_token_ = 0;
    }
}

int TrayApp::Run(HINSTANCE instance) {
    DebugLog(L"TrayApp::Run started");
    instance_ = instance;
    
    if (spp_client_) {
        spp_client_->SetUpdateCallback([this](const SppStateUpdate& update) { OnSppStateUpdate(update); });
    }

    DebugLog(L"Registering window classes...");
    RegisterWindowClasses();
    DebugLog(L"Creating host window...");
    CreateHostWindow();
    DebugLog(L"Adding tray icon...");
    AddTrayIcon();

    DebugLog(L"Starting BLE monitor...");
    if (ble_monitor_) {
        ble_monitor_->Start([this](const BatterySnapshot& snapshot) { OnBleSnapshot(snapshot); });
    }

    DebugLog(L"Showing control window...");
    ShowControlWindow();
    QueueConnection();

    DebugLog(L"Entering message loop...");
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    DebugLog(L"Message loop exited with code: " + std::to_wstring(message.wParam));
    return static_cast<int>(message.wParam);
}

void TrayApp::RegisterWindowClasses() {
    HICON hAppIcon = LoadIconW(instance_, MAKEINTRESOURCEW(1));

    WNDCLASSW host_class{};
    host_class.lpfnWndProc = HostWndProc;
    host_class.hInstance = instance_;
    host_class.lpszClassName = kHostClassName;
    host_class.hIcon = hAppIcon;
    host_class.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    RegisterClassW(&host_class);

    WNDCLASSW control_class{};
    control_class.lpfnWndProc = ControlWndProc;
    control_class.hInstance = instance_;
    control_class.lpszClassName = kControlClassName;
    control_class.hIcon = hAppIcon;
    control_class.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    control_class.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    RegisterClassW(&control_class);
}

void TrayApp::CreateHostWindow() {
    host_window_ = CreateWindowExW(0, kHostClassName, kTrayTitle, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance_, this);
    if (host_window_) {
        SetTimer(host_window_, kBatterySmoothingTimerId, 5000, nullptr);
    }
}

void TrayApp::CreateControlWindow() {
    if (control_window_) return;

    LoadUiResources();
    control_window_ = CreateWindowExW(WS_EX_APPWINDOW, kControlClassName, L"Nothing / CMF Controls",
                                      WS_POPUP | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, kWindowWidth, kWindowHeight,
                                      nullptr, nullptr, instance_, this);

    if (control_window_) {
        HRGN hrgn = CreateRoundRectRgn(0, 0, kWindowWidth + 1, kWindowHeight + 1, 24, 24);
        SetWindowRgn(control_window_, hrgn, TRUE);

        enum DWM_WINDOW_CORNER_PREFERENCE {
            DWMWCP_DEFAULT = 0,
            DWMWCP_DONOTROUND = 1,
            DWMWCP_ROUND = 2,
            DWMWCP_ROUNDSMALL = 3
        };
        DWM_WINDOW_CORNER_PREFERENCE pref = DWMWCP_ROUND;
        DwmSetWindowAttribute(control_window_, 33, &pref, sizeof(pref));
    }
}

void TrayApp::DestroyControlWindow() {
    if (control_window_) {
        DestroyWindow(control_window_);
        control_window_ = nullptr;
    }
}

void TrayApp::ShowControlWindow() {
    CreateControlWindow();
    if (!control_window_) return;

    PositionNearTray(control_window_);
    SetWindowPos(control_window_, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
    ShowWindow(control_window_, SW_SHOW);
    UpdateWindow(control_window_);
    SetForegroundWindow(control_window_);

    // Only connect if not already connected
    const bool already_connected = (spp_client_ && spp_client_->IsConnected());
    if (!already_connected) {
        QueueConnection();
    }
    RefreshControlWindow();
}

namespace {
Color GetBatteryColor(const BatteryReading& reading) {
    if (!reading.present || !reading.percent.has_value()) {
        return Color(255, 120, 120, 125); // Muted gray
    }
    uint8_t pct = *reading.percent;
    if (pct >= 60) {
        return Color(255, 0x34, 0xC7, 0x59); // Apple/Nothing green
    } else if (pct >= 20) {
        return Color(255, 0xFF, 0x95, 0x00); // Orange
    } else {
        return Color(255, 0xFF, 0x3B, 0x30); // Red
    }
}

std::wstring GetBatteryText(const BatteryReading& reading) {
    if (!reading.present || !reading.percent.has_value()) {
        return L"-";
    }
    return std::to_wstring(*reading.percent);
}
} // namespace

HICON TrayApp::CreateTrayIcon() {
    BatterySnapshot current_snap;
    {
        std::scoped_lock lock(state_mutex_);
        current_snap = snapshot_;
    }

    if (!current_snap.has_data) {
        // Fallback to static icon if no BLE/SPP data has arrived yet
        std::wstring icon_path = ResolveWorkspacePath(L"icons/32x32.png");
        Gdiplus::Bitmap bitmap(icon_path.c_str());
        HICON hIcon = nullptr;
        if (bitmap.GetLastStatus() == Gdiplus::Ok) {
            bitmap.GetHICON(&hIcon);
        }
        if (!hIcon) {
            hIcon = LoadIconW(nullptr, reinterpret_cast<LPCWSTR>(IDI_APPLICATION));
        }
        return hIcon;
    }

    // Create 32x32 transparent ARGB canvas
    Gdiplus::Bitmap bitmap(32, 32, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics graphics(&bitmap);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);

        auto draw_battery = [&](float x, const BatteryReading& reading) {
            // Outer frame bounds
            float frame_x = x;
            float frame_y = 6.0f;
            float frame_w = 11.0f;
            float frame_h = 21.0f;

            // Tip bounds
            float tip_x = x + 3.0f;
            float tip_y = 3.0f;
            float tip_w = 5.0f;
            float tip_h = 3.0f;

            Color border_color = Color(255, 140, 140, 145);
            Pen border_pen(border_color, 1.2f);
            SolidBrush tip_brush(border_color);

            // Draw Cathode/Tip
            GraphicsPath tip_path;
            AddRoundRectPath(tip_path, RectF(tip_x, tip_y, tip_w, tip_h), 0.8f);
            graphics.FillPath(&tip_brush, &tip_path);

            // Draw Outer Frame
            GraphicsPath frame_path;
            AddRoundRectPath(frame_path, RectF(frame_x, frame_y, frame_w, frame_h), 1.5f);
            graphics.DrawPath(&border_pen, &frame_path);

            if (reading.present && reading.percent.has_value()) {
                uint8_t pct = *reading.percent;
                Color fill_color = GetBatteryColor(reading);
                SolidBrush fill_brush(fill_color);

                // Inner fill bounds (margin of 2px inside)
                float fill_max_h = frame_h - 4.0f; // 17px max height
                float fill_h = (pct / 100.0f) * fill_max_h;
                if (fill_h < 2.0f && pct > 0) fill_h = 2.0f; // show at least a sliver if >0%

                float fill_x = frame_x + 2.0f;
                float fill_y = frame_y + 2.0f + (fill_max_h - fill_h);
                float fill_w = frame_w - 4.0f;

                GraphicsPath fill_path;
                AddRoundRectPath(fill_path, RectF(fill_x, fill_y, fill_w, fill_h), 0.8f);
                graphics.FillPath(&fill_brush, &fill_path);
                
                // Draw lightning bolt if charging
                if (reading.charging) {
                    PointF bolt_pts[6] = {
                        PointF(frame_x + 5.5f, frame_y + 4.0f),
                        PointF(frame_x + 2.0f, frame_y + 11.0f),
                        PointF(frame_x + 5.0f, frame_y + 11.0f),
                        PointF(frame_x + 5.5f, frame_y + 17.0f),
                        PointF(frame_x + 9.0f, frame_y + 10.0f),
                        PointF(frame_x + 6.0f, frame_y + 10.0f)
                    };
                    SolidBrush bolt_brush(Color(255, 255, 255, 255));
                    Pen bolt_outline(Color(255, 0, 0, 0), 1.0f);
                    graphics.FillPolygon(&bolt_brush, bolt_pts, 6);
                    graphics.DrawPolygon(&bolt_outline, bolt_pts, 6);
                }
            }
        };

        draw_battery(3.0f, current_snap.left);
        draw_battery(18.0f, current_snap.right);
    }

    HICON hIcon = nullptr;
    bitmap.GetHICON(&hIcon);
    if (!hIcon) {
        hIcon = LoadIconW(nullptr, reinterpret_cast<LPCWSTR>(IDI_APPLICATION));
    }
    return hIcon;
}

void TrayApp::AddTrayIcon() {
    if (!host_window_) return;

    tray_data_.hWnd = host_window_;
    tray_data_.hIcon = CreateTrayIcon();
    current_tray_icon_ = tray_data_.hIcon;
    const std::wstring tooltip = snapshot_.Tooltip();
    const size_t tip_capacity = sizeof(tray_data_.szTip) / sizeof(tray_data_.szTip[0]);
    std::wcsncpy(tray_data_.szTip, tooltip.c_str(), tip_capacity - 1);
    tray_data_.szTip[tip_capacity - 1] = L'\0';
    Shell_NotifyIconW(NIM_ADD, &tray_data_);
    Shell_NotifyIconW(NIM_SETVERSION, &tray_data_);
}

void TrayApp::RemoveTrayIcon() {
    if (host_window_) {
        Shell_NotifyIconW(NIM_DELETE, &tray_data_);
    }
}

void TrayApp::UpdateTrayIcon() {
    if (!host_window_) return;

    if (current_tray_icon_) {
        DestroyIcon(current_tray_icon_);
        current_tray_icon_ = nullptr;
    }

    tray_data_.hIcon = CreateTrayIcon();
    current_tray_icon_ = tray_data_.hIcon;
    const std::wstring tooltip = snapshot_.Tooltip();
    const size_t tip_capacity = sizeof(tray_data_.szTip) / sizeof(tray_data_.szTip[0]);
    std::wcsncpy(tray_data_.szTip, tooltip.c_str(), tip_capacity - 1);
    tray_data_.szTip[tip_capacity - 1] = L'\0';
    tray_data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &tray_data_);
}

void TrayApp::ApplySnapshot(const BatterySnapshot& snapshot) {
    bool changed = false;
    {
        std::scoped_lock lock(state_mutex_);
        if (snapshot_ != snapshot) {
            snapshot_ = snapshot;
            changed = true;
        }
    }
    if (changed) {
        PostUiMessage(kBleSnapshotMessage);
    }
}

namespace {
BatteryReading GetMode(const std::vector<BatteryReading>& samples, const BatteryReading& fallback) {
    if (samples.empty()) {
        return fallback;
    }
    const BatteryReading* best_reading = &samples[0];
    int max_count = 0;
    for (size_t i = 0; i < samples.size(); ++i) {
        int count = 0;
        for (size_t j = 0; j < samples.size(); ++j) {
            if (samples[i] == samples[j]) {
                count++;
            }
        }
        if (count > max_count) {
            max_count = count;
            best_reading = &samples[i];
        }
    }
    return *best_reading;
}
} // namespace

void TrayApp::AddBatterySamples(const BatteryReading& left, const BatteryReading& right, const BatteryReading& case_battery) {
    std::scoped_lock lock(smoothing_mutex_);
    left_samples_.push_back(left);
    right_samples_.push_back(right);
    case_samples_.push_back(case_battery);

    bool has_data = false;
    {
        std::scoped_lock state_lock(state_mutex_);
        has_data = snapshot_.has_data;
    }
    if (!has_data) {
        std::scoped_lock state_lock(state_mutex_);
        snapshot_.left = left;
        snapshot_.right = right;
        snapshot_.case_battery = case_battery;
        snapshot_.has_data = true;
        snapshot_.last_seen = std::chrono::steady_clock::now();
        PostUiMessage(kBleSnapshotMessage);
    }
}

void TrayApp::ProcessSmoothedBattery() {
    std::vector<BatteryReading> lefts;
    std::vector<BatteryReading> rights;
    std::vector<BatteryReading> cases;
    {
        std::scoped_lock lock(smoothing_mutex_);
        lefts = std::move(left_samples_);
        rights = std::move(right_samples_);
        cases = std::move(case_samples_);
        left_samples_.clear();
        right_samples_.clear();
        case_samples_.clear();
    }

    BatteryReading current_left;
    BatteryReading current_right;
    BatteryReading current_case;
    {
        std::scoped_lock lock(state_mutex_);
        current_left = snapshot_.left;
        current_right = snapshot_.right;
        current_case = snapshot_.case_battery;
    }

    BatteryReading final_left = GetMode(lefts, current_left);
    BatteryReading final_right = GetMode(rights, current_right);
    BatteryReading final_case = GetMode(cases, current_case);

    auto now = std::chrono::steady_clock::now();
    constexpr auto kDropoutTimeout = std::chrono::seconds(15);
    constexpr auto kCaseTimeout = std::chrono::seconds(5);

    // Left Earbud debounce logic
    if (final_left.present && final_left.percent.has_value()) {
        last_valid_left_ = final_left;
        last_valid_left_time_ = now;
    } else {
        if (last_valid_left_.present && last_valid_left_.percent.has_value() && (now - last_valid_left_time_ < kDropoutTimeout)) {
            final_left = last_valid_left_;
        } else {
            final_left.present = false;
            final_left.percent = std::nullopt;
            final_left.charging = false;
        }
    }

    // Right Earbud debounce logic
    if (final_right.present && final_right.percent.has_value()) {
        last_valid_right_ = final_right;
        last_valid_right_time_ = now;
    } else {
        if (last_valid_right_.present && last_valid_right_.percent.has_value() && (now - last_valid_right_time_ < kDropoutTimeout)) {
            final_right = last_valid_right_;
        } else {
            final_right.present = false;
            final_right.percent = std::nullopt;
            final_right.charging = false;
        }
    }

    // Case debounce logic (Only when case is present)
    if (final_case.present && final_case.percent.has_value()) {
        last_valid_case_ = final_case;
        last_valid_case_time_ = now;
    } else {
        if (last_valid_case_.present && last_valid_case_.percent.has_value() && (now - last_valid_case_time_ < kCaseTimeout)) {
            final_case = last_valid_case_;
        } else {
            final_case.present = false;
            final_case.percent = std::nullopt;
            final_case.charging = false;
        }
    }

    bool changed = false;
    {
        std::scoped_lock lock(state_mutex_);
        if (snapshot_.left != final_left || snapshot_.right != final_right || snapshot_.case_battery != final_case) {
            snapshot_.left = final_left;
            snapshot_.right = final_right;
            snapshot_.case_battery = final_case;
            snapshot_.has_data = (final_left.present || final_right.present || final_case.present);
            snapshot_.last_seen = now;
            changed = true;
        }
    }
    if (changed) {
        PostUiMessage(kBleSnapshotMessage);
    }
}

void TrayApp::OnBleSnapshot(const BatterySnapshot& snapshot) {
    bool should_reload_sku = false;
    std::wstring sku_to_reload = L"";

    {
        std::scoped_lock lock(state_mutex_);
        const bool spp_online = (spp_client_ && spp_client_->IsConnected());
        std::wstring real_bt_name = spp_online ? spp_client_->GetDeviceName() : L"";

        if (!real_bt_name.empty()) {
            snapshot_.device_name = real_bt_name;
        } else if (!snapshot.device_name.empty() && snapshot.device_name != L"Nothing earbuds") {
            snapshot_.device_name = snapshot.device_name;
        }

        snapshot_.bluetooth_address = snapshot.bluetooth_address;

        if (device_sku_.empty() && !snapshot_.device_name.empty()) {
            std::wstring guessed_sku = GuessSkuFromName(snapshot_.device_name);
            if (!guessed_sku.empty()) {
                device_sku_ = guessed_sku;
                sku_to_reload = guessed_sku;
                should_reload_sku = true;
            }
        }
    }

    if (should_reload_sku) {
        ReloadBudImages(sku_to_reload);
        PostUiMessage(kBleSnapshotMessage);
    }

    AddBatterySamples(snapshot.left, snapshot.right, snapshot.case_battery);
}

void TrayApp::OnSppStatusChanged(bool connected) {
    spp_connect_pending_.store(false, std::memory_order_release);

    PostUiMessage(kSppStatusMessage, connected ? 1 : 0, 0);
    if (connected && spp_client_) {
        spp_client_->QueryDeviceState();
    }
}

void TrayApp::OnSppStateUpdate(const SppStateUpdate& update) {
    if (update.type == SppStateUpdate::Type::Battery) {
        AddBatterySamples(update.left, update.right, update.case_battery);
        return;
    }

    bool state_changed = false;
    bool should_reload_sku = false;
    std::wstring sku_to_reload = L"";

    {
        std::scoped_lock lock(state_mutex_);
        switch (update.type) {
        case SppStateUpdate::Type::AncMode: {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_user_anc_click_).count() > 1500) {
                selected_anc_ = update.anc_mode;
                state_changed = true;
            }
            break;
        }
        case SppStateUpdate::Type::EqPreset: {
            if (update.eq_preset == 0) {
                selected_eq_ = EqPreset::Balanced;
            } else if (update.eq_preset == 1) {
                selected_eq_ = EqPreset::Voice;
            } else if (update.eq_preset == 2) {
                selected_eq_ = EqPreset::MoreTreble;
            } else if (update.eq_preset == 3) {
                selected_eq_ = EqPreset::MoreBass;
            }
            state_changed = true;
            break;
        }
        case SppStateUpdate::Type::BassState: {
            bass_enabled_ = update.bass_enabled;
            bass_level_ = update.bass_level;
            state_changed = true;
            break;
        }
        case SppStateUpdate::Type::FirmwareVersion: {
            state_changed = true;
            break;
        }
        case SppStateUpdate::Type::DeviceModel: {
            device_serial_ = update.text_value;
            std::wstring sku = ResolveSkuFromSerial(device_serial_);
            if (!sku.empty() && sku != device_sku_) {
                device_sku_ = sku;
                should_reload_sku = true;
                sku_to_reload = sku;
            }
            state_changed = true;
            break;
        }
        case SppStateUpdate::Type::LowLatency: {
            low_latency_enabled_ = update.low_latency_enabled;
            state_changed = true;
            break;
        }
        }
    }
    
    if (should_reload_sku) {
        ReloadBudImages(sku_to_reload);
    }

    if (state_changed || should_reload_sku) {
        PostUiMessage(kBleSnapshotMessage);
    }
}

void TrayApp::RefreshControlWindow() {
    if (!control_window_) return;
    InvalidateRect(control_window_, nullptr, FALSE);
}

void TrayApp::UpdateControlStatusText() {
    if (control_window_) InvalidateRect(control_window_, nullptr, FALSE);
}

void TrayApp::QueueConnection() {
    DebugLog(L"QueueConnection requested");

    // If already connected, just re-query device state
    if (spp_client_ && spp_client_->IsConnected()) {
        DebugLog(L"QueueConnection: Already connected, re-querying state.");
        spp_client_->QueryDeviceState();
        return;
    }

    if (spp_connect_thread_.joinable()) {
        if (spp_connect_pending_.load(std::memory_order_acquire)) {
            DebugLog(L"QueueConnection: Connection already pending, skipping.");
            return;
        }
        spp_connect_thread_.join();
    }

    uint64_t address = 0;
    {
        std::scoped_lock lock(state_mutex_);
        address = snapshot_.bluetooth_address;
    }

    spp_connect_pending_.store(true, std::memory_order_release);
    if (control_window_) InvalidateRect(control_window_, nullptr, FALSE);

    spp_connect_thread_ = std::thread([this, address] {
        DebugLog(L"QueueConnection thread executing in background...");
        try {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
        } catch (...) {}
        
        // Force disconnect any stale socket before attempting connection
        if (spp_client_) {
            spp_client_->Disconnect();
        }

        bool connected = false;
        try {
            if (address != 0 && spp_client_) {
                connected = spp_client_->ConnectToAddress(address);
            }
            if (!connected && spp_client_) {
                connected = spp_client_->Connect();
            }
        } catch (...) {}

        DebugLog(L"QueueConnection thread finished, connected=" + std::to_wstring(connected ? 1 : 0));
        OnSppStatusChanged(connected);
    });
}

void TrayApp::DisconnectSpp() {
    spp_connect_pending_.store(false, std::memory_order_release);
    if (spp_client_) spp_client_->Disconnect();
}

AncMode TrayApp::SelectedAncMode() const {
    return selected_anc_;
}

std::pair<bool, uint8_t> TrayApp::SelectedBassState() const {
    return {bass_enabled_, bass_level_};
}

void TrayApp::ApplySelectedAnc() {
    last_user_anc_click_ = std::chrono::steady_clock::now();
    if (!spp_client_ || !spp_client_->IsConnected()) {
        QueueConnection();
        return;
    }
    AncMode mode = SelectedAncMode();
    spp_client_->SendAnc(mode);
}

void TrayApp::ApplySelectedBass() {
    if (!spp_client_ || !spp_client_->IsConnected()) {
        QueueConnection();
        return;
    }
    const auto [enabled, level] = SelectedBassState();
    spp_client_->SendBass(enabled, level);
}

void TrayApp::ApplyEqPreset(EqPreset preset) {
    selected_eq_ = preset;
    uint8_t preset_val = 0;
    switch (preset) {
    case EqPreset::Balanced:
        bass_enabled_ = true;
        bass_level_ = 3;
        preset_val = 0;
        break;
    case EqPreset::MoreBass:
        bass_enabled_ = true;
        bass_level_ = 5;
        preset_val = 3;
        break;
    case EqPreset::MoreTreble:
        bass_enabled_ = true;
        bass_level_ = 1;
        preset_val = 2;
        break;
    case EqPreset::Voice:
        bass_enabled_ = false;
        bass_level_ = 2;
        preset_val = 1;
        break;
    case EqPreset::Dirac:
        bass_enabled_ = false;
        bass_level_ = 0;
        preset_val = 4;
        break;
    }
    ApplySelectedBass();
    if (spp_client_ && spp_client_->IsConnected()) {
        spp_client_->SendEq(preset_val);
    }
}

HWND TrayApp::GetActiveWindow() const {
    return control_window_ ? control_window_ : host_window_;
}

void TrayApp::PostUiMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    if (host_window_ && !shutting_down_.load(std::memory_order_acquire)) {
        PostMessageW(host_window_, message, wparam, lparam);
    }
}

void TrayApp::SetTrayTooltip(const std::wstring& tooltip) {
    const size_t tip_capacity = sizeof(tray_data_.szTip) / sizeof(tray_data_.szTip[0]);
    std::wcsncpy(tray_data_.szTip, tooltip.c_str(), tip_capacity - 1);
    tray_data_.szTip[tip_capacity - 1] = L'\0';
}

std::wstring TrayApp::ResolveSkuFromSerial(const std::wstring& serial) {
    if (serial == L"12345678901234567") {
        return L"01";
    }
    if (serial.length() < 8) {
        return L"";
    }
    std::wstring head = serial.substr(0, 2);
    if (head == L"MA") {
        std::wstring year = serial.substr(6, 2);
        if (year == L"22" || year == L"23") {
            return L"14";
        } else if (year == L"24") {
            return L"11200005";
        }
    } else if (head == L"SH" || head == L"13") {
        if (serial.length() >= 6) {
            return serial.substr(4, 2);
        }
    }
    return L"";
}

bool TrayApp::IsSwappedAncSku(const std::wstring& sku) {
    return false;
}

void TrayApp::ReloadBudImages(const std::wstring& sku) {
    std::scoped_lock lock(state_mutex_);

    // Delete old images
    if (image_left_bud_) { delete image_left_bud_; image_left_bud_ = nullptr; }
    if (image_right_bud_) { delete image_right_bud_; image_right_bud_ = nullptr; }
    if (image_case_bud_) { delete image_case_bud_; image_case_bud_ = nullptr; }
    if (image_duo_bud_) { delete image_duo_bud_; image_duo_bud_ = nullptr; }

    std::wstring left_file = L"";
    std::wstring case_file = L"";
    std::wstring right_file = L"";
    std::wstring duo_file = L"";
    std::wstring model_display_name = L"Nothing Earbuds";

    if (sku == L"01" || sku == L"03" || sku == L"07") {
        left_file = L"ear_one_white_left.png";
        case_file = L"ear_one_white_case.png";
        right_file = L"ear_one_white_right.png";
        model_display_name = L"Nothing Ear (1)";
    } else if (sku == L"02" || sku == L"04" || sku == L"06" || sku == L"08" || sku == L"10") {
        left_file = L"ear_one_black_left.png";
        case_file = L"ear_one_black_case.png";
        right_file = L"ear_one_black_right.png";
        model_display_name = L"Nothing Ear (1)";
    } else if (sku == L"14" || sku == L"15" || sku == L"16") {
        left_file = L"ear_stick_left.png";
        case_file = L"ear_stick_case_none.png";
        right_file = L"ear_stick_right.png";
        model_display_name = L"Nothing Ear (stick)";
    } else if (sku == L"17" || sku == L"18" || sku == L"19") {
        left_file = L"ear_two_white_left.png";
        case_file = L"ear_two_white_case.png";
        right_file = L"ear_two_white_right.png";
        model_display_name = L"Nothing Ear (2)";
    } else if (sku == L"27" || sku == L"28" || sku == L"29") {
        left_file = L"ear_two_black_left.png";
        case_file = L"ear_two_black_case.png";
        right_file = L"ear_two_black_right.png";
        model_display_name = L"Nothing Ear (2)";
    } else if (sku == L"30" || sku == L"31" || sku == L"32" || sku == L"33" || sku == L"34" || sku == L"35") {
        left_file = L"ear_corsola_orange_left.png";
        case_file = L"ear_corsola_orange_case.png";
        right_file = L"ear_corsola_orange_right.png";
        model_display_name = L"CMF Buds Pro";
    } else if (sku == L"48" || sku == L"53") {
        left_file = L"ear_corsola_orange_left.png";
        case_file = L"ear_corsola_orange_case.png";
        right_file = L"ear_corsola_orange_right.png";
        model_display_name = L"CMF Buds Pro";
    } else if (sku == L"54" || sku == L"55" || sku == L"56" || sku == L"57" || sku == L"58" || sku == L"59") {
        left_file = L"espeon_orange_left.png";
        case_file = L"espeon_orange_case.png";
        right_file = L"espeon_orange_right.png";
        model_display_name = L"CMF Buds 2";
    } else if (sku == L"61" || sku == L"62" || sku == L"69" || sku == L"70" || sku == L"74" || sku == L"75") {
        left_file = L"ear_two_white_left.png";
        case_file = L"";
        right_file = L"ear_two_white_right.png";
        model_display_name = L"Nothing Ear";
    } else if (sku == L"63" || sku == L"64" || sku == L"65" || sku == L"66" || sku == L"67" || sku == L"68") {
        left_file = L"ear_color_white_left.png";
        case_file = L"ear_color_white_case.png";
        right_file = L"ear_color_white_right.png";
        model_display_name = L"Nothing Ear (open)";
    } else if (sku == L"76" || sku == L"77" || sku == L"78" || sku == L"79" || sku == L"80" || sku == L"81" || sku == L"82" || sku == L"83") {
        left_file = L"espeon_orange_left.png";
        case_file = L"espeon_orange_case.png";
        right_file = L"espeon_orange_right.png";
        model_display_name = L"CMF Buds Pro 2";
    } else if (sku == L"11200005" || sku == L"49" || sku == L"50") {
        left_file = L"flaffy_white_left.png";
        case_file = L"flaffy_white_case.png";
        right_file = L"flaffy_white_right.png";
        model_display_name = L"Nothing Ear (a)";
    } else {
        left_file = L"espeon_orange_left.png";
        case_file = L"espeon_orange_case.png";
        right_file = L"espeon_orange_right.png";
        model_display_name = L"CMF Buds 2";
    }

    auto load_img = [&](const std::wstring& filename) -> Gdiplus::Image* {
        if (filename.empty()) return nullptr;
        std::wstring full_path = (std::filesystem::path(asset_root_) / filename).wstring();
        Gdiplus::Image* img = Gdiplus::Image::FromFile(full_path.c_str(), FALSE);
        if (img && img->GetLastStatus() == Gdiplus::Ok) {
            return img;
        }
        if (img) delete img;
        return nullptr;
    };

    image_left_bud_ = load_img(left_file);
    image_case_bud_ = load_img(case_file);
    image_right_bud_ = load_img(right_file);
    image_duo_bud_ = load_img(duo_file);

    std::wstring real_bt_name = (spp_client_ && spp_client_->IsConnected()) ? spp_client_->GetDeviceName() : L"";
    if (!real_bt_name.empty()) {
        snapshot_.device_name = real_bt_name;
    } else {
        snapshot_.device_name = model_display_name;
    }
    DebugLog(L"ReloadBudImages: Loaded resources for SKU: " + sku + L" (" + snapshot_.device_name + L")");
}

void TrayApp::LoadUiResources() {
    if (font_heading_) return;

    const std::filesystem::path workspace = FindWorkspaceRoot();
    workspace_root_ = workspace.wstring();
    font_root_ = (workspace / L"fonts").wstring();
    asset_root_ = (workspace / L"assets").wstring();

    const std::wstring heading_path = (std::filesystem::path(font_root_) / L"ndot_55.otf").wstring();
    const std::wstring body_path = (std::filesystem::path(font_root_) / L"space_grotesk_regular.ttf").wstring();
    const std::wstring button_path = (std::filesystem::path(font_root_) / L"manrope_regular.otf").wstring();

    AddFontResourceExW(heading_path.c_str(), FR_PRIVATE, nullptr);
    AddFontResourceExW(body_path.c_str(), FR_PRIVATE, nullptr);
    AddFontResourceExW(button_path.c_str(), FR_PRIVATE, nullptr);

    if (font_collection_) {
        font_collection_->AddFontFile(heading_path.c_str());
        font_collection_->AddFontFile(body_path.c_str());
        font_collection_->AddFontFile(button_path.c_str());
    }

    auto make_font = [&](const wchar_t* family_name, float size, int style) -> Gdiplus::Font* {
        if (font_collection_) {
            auto* family = new Gdiplus::FontFamily(family_name, font_collection_);
            if (family->GetLastStatus() == Gdiplus::Ok) {
                auto* font = new Gdiplus::Font(family, size, style, Gdiplus::UnitPixel);
                delete family;
                if (font->GetLastStatus() == Gdiplus::Ok) {
                    return font;
                }
                delete font;
            } else {
                delete family;
            }
        }

        auto* sys_family = new Gdiplus::FontFamily(family_name);
        if (sys_family->GetLastStatus() == Gdiplus::Ok) {
            auto* font = new Gdiplus::Font(sys_family, size, style, Gdiplus::UnitPixel);
            delete sys_family;
            if (font->GetLastStatus() == Gdiplus::Ok) {
                return font;
            }
            delete font;
        } else {
            delete sys_family;
        }

        return new Gdiplus::Font(Gdiplus::FontFamily::GenericSansSerif(), size, style, Gdiplus::UnitPixel);
    };

    font_serif_ = make_font(L"Georgia", 26.0f, Gdiplus::FontStyleRegular);
    font_heading_ = make_font(L"Segoe UI", 24.0f, Gdiplus::FontStyleBold);
    font_title_ = make_font(L"Segoe UI", 16.0f, Gdiplus::FontStyleBold);
    font_body_ = make_font(L"Segoe UI", 14.0f, Gdiplus::FontStyleRegular);
    font_button_ = make_font(L"Segoe UI", 14.0f, Gdiplus::FontStyleBold);
    font_small_ = make_font(L"Segoe UI", 12.0f, Gdiplus::FontStyleRegular);
    font_sub_ = make_font(L"Segoe UI", 13.0f, Gdiplus::FontStyleRegular);

    brush_canvas_ = CreateSolidBrush(RGB(0x12, 0x12, 0x12));
    brush_surface_ = CreateSolidBrush(RGB(0x1E, 0x1E, 0x1E));
    brush_surface_elevated_ = CreateSolidBrush(RGB(0x2A, 0x2A, 0x2A));
    brush_surface_card_ = CreateSolidBrush(RGB(0x1E, 0x1E, 0x1E));

    ReloadBudImages(L"");
}

void TrayApp::ReleaseUiResources() {
    if (font_serif_) { delete font_serif_; font_serif_ = nullptr; }
    if (font_heading_) { delete font_heading_; font_heading_ = nullptr; }
    if (font_title_) { delete font_title_; font_title_ = nullptr; }
    if (font_body_) { delete font_body_; font_body_ = nullptr; }
    if (font_button_) { delete font_button_; font_button_ = nullptr; }
    if (font_small_) { delete font_small_; font_small_ = nullptr; }
    if (font_sub_) { delete font_sub_; font_sub_ = nullptr; }

    const std::filesystem::path workspace = FindWorkspaceRoot();
    const std::wstring font_dir = (workspace / L"fonts").wstring();
    const std::wstring heading_path = (std::filesystem::path(font_dir) / L"ndot_55.otf").wstring();
    const std::wstring body_path = (std::filesystem::path(font_dir) / L"space_grotesk_regular.ttf").wstring();
    const std::wstring button_path = (std::filesystem::path(font_dir) / L"manrope_regular.otf").wstring();

    RemoveFontResourceExW(heading_path.c_str(), FR_PRIVATE, nullptr);
    RemoveFontResourceExW(body_path.c_str(), FR_PRIVATE, nullptr);
RemoveFontResourceExW(button_path.c_str(), FR_PRIVATE, nullptr);

    if (brush_canvas_) { DeleteObject(brush_canvas_); brush_canvas_ = nullptr; }
    if (brush_surface_) { DeleteObject(brush_surface_); brush_surface_ = nullptr; }
    if (brush_surface_elevated_) { DeleteObject(brush_surface_elevated_); brush_surface_elevated_ = nullptr; }
    if (brush_surface_card_) { DeleteObject(brush_surface_card_); brush_surface_card_ = nullptr; }

    if (image_left_bud_) { delete image_left_bud_; image_left_bud_ = nullptr; }
    if (image_right_bud_) { delete image_right_bud_; image_right_bud_ = nullptr; }
    if (image_case_bud_) { delete image_case_bud_; image_case_bud_ = nullptr; }
    if (image_duo_bud_) { delete image_duo_bud_; image_duo_bud_ = nullptr; }
}

void TrayApp::LayoutControlWindow() {
    header_rect_ = RECT{0, 0, 650, 80};
    device_area_rect_ = RECT{0, 80, 650, 320};
    
    left_bud_rect_ = RECT{100, 100, 200, 250};
    right_bud_rect_ = RECT{450, 100, 550, 250};
    
    left_ring_rect_ = RECT{100, 260, 200, 300};
    right_ring_rect_ = RECT{450, 260, 550, 300};
    
    // Сетка карточек
    anc_card_rect_ = RECT{45, 305, 310, 520};
    eq_card_rect_ = RECT{340, 305, 605, 520};
    
    // Кнопки ANC на левой карточке
    const int anc_button_y = 355;
    anc_top_rects_[0] = RECT{55, anc_button_y, 131, anc_button_y + 35};
    anc_top_rects_[1] = RECT{139, anc_button_y, 215, anc_button_y + 35};
    anc_top_rects_[2] = RECT{223, anc_button_y, 300, anc_button_y + 35};
    
    // Сегментированный селектор ANC
    const int anc_line_y = 425;
    anc_mode_rects_[0] = RECT{60, anc_line_y, 110, anc_line_y + 20};
    anc_mode_rects_[1] = RECT{120, anc_line_y, 170, anc_line_y + 20};
    anc_mode_rects_[2] = RECT{180, anc_line_y, 230, anc_line_y + 20};
    anc_mode_rects_[3] = RECT{240, anc_line_y, 290, anc_line_y + 20};
    
    // Кнопка Personalised ANC
    anc_toggle_rect_ = RECT{55, 465, 300, 505};
    
    // Кнопки пресетов эквалайзера
    const int eq_button_y = 350;
    const int eq_button_height = 28;
    const int eq_button_spacing = 5;
    eq_preset_rects_[0] = RECT{355, eq_button_y, 468, eq_button_y + eq_button_height};
    eq_preset_rects_[1] = RECT{477, eq_button_y, 590, eq_button_y + eq_button_height};
    eq_preset_rects_[2] = RECT{355, eq_button_y + eq_button_height + eq_button_spacing, 468, eq_button_y + (eq_button_height * 2) + eq_button_spacing};
    eq_preset_rects_[3] = RECT{477, eq_button_y + eq_button_height + eq_button_spacing, 590, eq_button_y + (eq_button_height * 2) + eq_button_spacing};
    eq_preset_rects_[4] = RECT{355, eq_button_y + (eq_button_height * 2) + (eq_button_spacing * 2), 590, eq_button_y + (eq_button_height * 3) + (eq_button_spacing * 2)};
    
    // Кнопка Ultra Bass
    eq_custom_rect_ = RECT{355, 465, 590, 505};
    eq_custom_toggle_rect_ = RECT{505, 473, 545, 497};

    // Кнопка выбора языка [ RU / EN ]
    lang_btn_rect_ = RECT{525, 14, 575, 40};

    // Кнопки управления в заголовке
    minimize_btn_rect_ = RECT{kWindowWidth - 70, 10, kWindowWidth - 45, 35};
    close_btn_rect_ = RECT{kWindowWidth - 40, 10, kWindowWidth - 15, 35};

    // Нижняя кнопка режимов
    low_latency_rect_ = RECT{45, 530, 605, 565};
}

void TrayApp::TriggerRing(bool left) {
    if (left) {
        if (left_ringing_.exchange(true)) return;
    } else {
        if (right_ringing_.exchange(true)) return;
    }

    MessageBeep(MB_ICONASTERISK);
    if (spp_client_ && spp_client_->IsConnected()) {
        spp_client_->SendFindMyBuds(left, true);
    }

    std::thread([this, left] {
        std::this_thread::sleep_for(std::chrono::seconds(4));
        if (spp_client_ && spp_client_->IsConnected()) {
            spp_client_->SendFindMyBuds(left, false);
        }
        if (left) left_ringing_.store(false);
        else right_ringing_.store(false);
        if (control_window_) InvalidateRect(control_window_, nullptr, FALSE);
    }).detach();
}

LRESULT CALLBACK TrayApp::HostWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    TrayApp* app = nullptr;
    if (message == WM_NCCREATE) {
        auto create_struct = reinterpret_cast<LPCREATESTRUCTW>(lparam);
        app = reinterpret_cast<TrayApp*>(create_struct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    } else {
        app = reinterpret_cast<TrayApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!app) return DefWindowProcW(hwnd, message, wparam, lparam);

    switch (message) {
    case kTrayIconMessage: {
        switch (LOWORD(lparam)) {
        case WM_LBUTTONUP:
        case NIN_SELECT:
            app->ShowControlWindow();
            break;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU: {
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, kTrayOpenControl, L"Open");
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(menu, MF_STRING, kTrayExit, L"Exit");
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hwnd);
            TrackPopupMenu(menu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, nullptr);
            DestroyMenu(menu);
            break;
        }
        }
        return 0;
    }
    case kBleSnapshotMessage:
        app->UpdateTrayIcon();
        app->RefreshControlWindow();
        return 0;
    case kSppStatusMessage:
        app->RefreshControlWindow();
        return 0;
    case WM_TIMER:
        if (wparam == kBatterySmoothingTimerId) {
            app->ProcessSmoothedBattery();
            return 0;
        }
        break;
    case WM_COMMAND: {
        int id = LOWORD(wparam);
        if (id == kTrayOpenControl) {
            app->ShowControlWindow();
        } else if (id == kTrayExit) {
            PostQuitMessage(0);
        }
        return 0;
    }
    case WM_DESTROY: {
        PostQuitMessage(0);
        return 0;
    }
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT CALLBACK TrayApp::ControlWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    TrayApp* app = nullptr;
    if (message == WM_NCCREATE) {
        auto create_struct = reinterpret_cast<LPCREATESTRUCTW>(lparam);
        app = reinterpret_cast<TrayApp*>(create_struct->lpCreateParams);
        app->control_window_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    } else {
        app = reinterpret_cast<TrayApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!app) return DefWindowProcW(hwnd, message, wparam, lparam);

    switch (message) {
    case WM_CREATE:
        app->control_window_ = hwnd;
        app->LoadUiResources();
        app->LayoutControlWindow();
        SetTimer(hwnd, 100, 40, nullptr); // 25 FPS анимация спиннера
        return 0;
    case WM_TIMER:
        if (wparam == 100) {
            const bool is_online = (app->spp_client_ && app->spp_client_->IsConnected());
            if (!is_online) {
                app->splash_anim_angle_ = (app->splash_anim_angle_ + 12) % 360;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        break;
    case WM_SIZE:
        app->LayoutControlWindow();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_NCHITTEST: {
        const LRESULT hit = DefWindowProcW(hwnd, message, wparam, lparam);
        if (hit == HTCLIENT) {
            POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            ScreenToClient(hwnd, &point);

            const bool is_online = (app->spp_client_ && app->spp_client_->IsConnected());
            RECT status_click_rect{35, 48, 300, 70};

            bool is_interactive = PtInAnyRect(app->anc_card_rect_, point) ||
                                  PtInAnyRect(app->eq_card_rect_, point) ||
                                  PtInAnyRect(app->left_ring_rect_, point) ||
                                  PtInAnyRect(app->right_ring_rect_, point) ||
                                  PtInAnyRect(app->minimize_btn_rect_, point) ||
                                  PtInAnyRect(app->close_btn_rect_, point) ||
                                  PtInAnyRect(app->lang_btn_rect_, point) ||
                                  PtInAnyRect(app->low_latency_rect_, point) ||
                                  PtInAnyRect(app->eq_custom_toggle_rect_, point) ||
                                  PtInAnyRect(app->eq_custom_rect_, point) ||
                                  PtInAnyRect(app->bass_slider_track_rect_, point) ||
                                  (!is_online && PtInAnyRect(status_click_rect, point));

            for (const auto& r : app->anc_top_rects_) { if (PtInAnyRect(r, point)) is_interactive = true; }
            for (const auto& r : app->anc_mode_rects_) { if (PtInAnyRect(r, point)) is_interactive = true; }
            for (const auto& r : app->eq_preset_rects_) { if (PtInAnyRect(r, point)) is_interactive = true; }
            for (const auto& r : app->bass_level_rects_) { if (PtInAnyRect(r, point)) is_interactive = true; }

            if (is_interactive) {
                return HTCLIENT;
            }
            return HTCAPTION;
        }
        return hit;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT client{};
        GetClientRect(hwnd, &client);

        app->LayoutControlWindow();

        const int width = client.right - client.left;
        const int height = client.bottom - client.top;

        HDC mem_dc = CreateCompatibleDC(hdc);
        HBITMAP mem_bitmap = CreateCompatibleBitmap(hdc, width, height);
        HGDIOBJ old_bitmap = SelectObject(mem_dc, mem_bitmap);

        {
            Graphics graphics(mem_dc);
            graphics.SetSmoothingMode(SmoothingModeAntiAlias);
            graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            graphics.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

            // 1. Чёрный фон (#000000)
            SolidBrush canvas_brush(Color(255, 0x00, 0x00, 0x00));
            graphics.FillRectangle(&canvas_brush, 0.0f, 0.0f, static_cast<REAL>(width), static_cast<REAL>(height));

            // Кнопки свернуть / закрыть
            {
                Pen btn_pen(kTextWhite(), 1.5f);
                REAL y_center = static_cast<REAL>((app->minimize_btn_rect_.top + app->minimize_btn_rect_.bottom) / 2);
                graphics.DrawLine(&btn_pen, 
                                  static_cast<REAL>(app->minimize_btn_rect_.left + 5), 
                                  y_center, 
                                  static_cast<REAL>(app->minimize_btn_rect_.right - 5), 
                                  y_center);
            }

            {
                Pen btn_pen(kTextWhite(), 1.5f);
                graphics.DrawLine(&btn_pen, 
                                  static_cast<REAL>(app->close_btn_rect_.left + 6), 
                                  static_cast<REAL>(app->close_btn_rect_.top + 6), 
                                  static_cast<REAL>(app->close_btn_rect_.right - 6), 
                                  static_cast<REAL>(app->close_btn_rect_.bottom - 6));
                graphics.DrawLine(&btn_pen, 
                                  static_cast<REAL>(app->close_btn_rect_.left + 6), 
                                  static_cast<REAL>(app->close_btn_rect_.bottom - 6), 
                                  static_cast<REAL>(app->close_btn_rect_.right - 6), 
                                  static_cast<REAL>(app->close_btn_rect_.top + 6));
            }

            BatterySnapshot snapshot_copy;
            {
                std::scoped_lock lock(app->state_mutex_);
                snapshot_copy = app->snapshot_;
            }

            auto draw_string = [&](const std::wstring& text, Gdiplus::Font* font, const RECT& rect, const Color& color, StringAlignment align_h = StringAlignmentCenter, StringAlignment align_v = StringAlignmentCenter) {
                SolidBrush brush(color);
                StringFormat format;
                format.SetAlignment(align_h);
                format.SetLineAlignment(align_v);
                RectF rect_f(static_cast<REAL>(rect.left), static_cast<REAL>(rect.top),
                             static_cast<REAL>(rect.right - rect.left), static_cast<REAL>(rect.bottom - rect.top));
                graphics.DrawString(text.c_str(), -1, font, rect_f, &format, &brush);
            };

            auto draw_image_fit = [&](Gdiplus::Image* img, float target_x, float target_y, float target_w, float target_h) {
                if (!img) return;
                float iw = static_cast<float>(img->GetWidth());
                float ih = static_cast<float>(img->GetHeight());
                if (iw <= 0.0f || ih <= 0.0f) return;
                float scale = (std::min)(target_w / iw, target_h / ih);
                float dw = iw * scale;
                float dh = ih * scale;
                float dx = target_x + (target_w - dw) / 2.0f;
                float dy = target_y + (target_h - dh) / 2.0f;
                graphics.DrawImage(img, dx, dy, dw, dh);
            };

            // 2. HEADER
            draw_string(app->Tr(L"Устройства", L"Devices"), app->font_heading_, RECT{35, 14, 300, 48}, kTextWhite(), StringAlignmentNear, StringAlignmentCenter);

            // Языковая кнопка [ RU / EN ]
            FillRoundedRect(graphics, app->lang_btn_rect_, 10.0f, Color(255, 0x2A, 0x2A, 0x2A), Color(255, 0x3A, 0x3A, 0x3A));
            std::wstring lang_text = (app->language_ == AppLanguage::Russian) ? L"RU" : L"EN";
            draw_string(lang_text, app->font_button_, app->lang_btn_rect_, Color(255, 255, 255, 255));

            const bool is_online = (app->spp_client_ && app->spp_client_->IsConnected());

            // 3. ЕСЛИ ОТКЛЮЧЕНО / В ПРОЦЕССЕ ПОДКЛЮЧЕНИЯ -> СПЛЕШ-СКРИН
            if (!is_online) {
                // Сплеш-карточка по центру
                RECT splash_card{35, 80, 615, 550};
                FillRoundedRect(graphics, splash_card, 20.0f, Color(255, 0x1C, 0x1C, 0x1C));

                draw_string(L"Nothing Track", app->font_serif_, RECT{35, 115, 615, 155}, kTextWhite());
                std::wstring model_name = snapshot_copy.device_name.empty() ? L"CMF Buds 2" : snapshot_copy.device_name;
                draw_string(model_name, app->font_title_, RECT{35, 158, 615, 188}, Color(255, 0x9A, 0x9A, 0x9A));

                // Иллюстрация по центру
                if (app->image_duo_bud_) {
                    draw_image_fit(app->image_duo_bud_, 150.0f, 200.0f, 350.0f, 150.0f);
                } else if (app->image_case_bud_) {
                    draw_image_fit(app->image_case_bud_, 220.0f, 200.0f, 210.0f, 150.0f);
                }

                const bool pending = app->spp_connect_pending_.load();

                // Анимированная крутяшка-спиннер (Loading Spinner Ring)
                if (pending) {
                    float spinner_cx = 325.0f;
                    float spinner_cy = 385.0f;
                    float spinner_r = 18.0f;

                    // Серый неактивный круг
                    Pen track_pen(Color(255, 0x3A, 0x3A, 0x3A), 3.5f);
                    graphics.DrawEllipse(&track_pen, spinner_cx - spinner_r, spinner_cy - spinner_r, spinner_r * 2.0f, spinner_r * 2.0f);

                    // Вращающаяся дуга
                    Pen active_arc_pen(kAccentRed(), 3.5f);
                    active_arc_pen.SetStartCap(LineCapRound);
                    active_arc_pen.SetEndCap(LineCapRound);
                    graphics.DrawArc(&active_arc_pen, spinner_cx - spinner_r, spinner_cy - spinner_r, spinner_r * 2.0f, spinner_r * 2.0f, static_cast<REAL>(app->splash_anim_angle_), 110.0f);
                }

                // Статус-бэдж
                RECT badge_rect{170, 435, 480, 480};
                FillRoundedRect(graphics, badge_rect, 15.0f, pending ? Color(255, 0x2A, 0x2A, 0x30) : Color(255, 0x2A, 0x2A, 0x2A), Color(255, 0x3A, 0x3A, 0x3A));

                std::wstring status_str = pending
                    ? app->Tr(L"Подключение к Bluetooth...", L"Connecting to Bluetooth...")
                    : app->Tr(L"● Нажмите для подключения", L"● Click to Connect");
                draw_string(status_str, app->font_button_, badge_rect, pending ? kAccentRed() : kTextWhite());
            } else {
                // 3. HERO PRODUCT CARD (КОНТЕЙНЕР #1C1C1C)
                app->hero_card_rect_ = RECT{35, 75, 615, 275};
                FillRoundedRect(graphics, app->hero_card_rect_, 20.0f, Color(255, 0x1C, 0x1C, 0x1C));

                // Отрисовка изображений наушников с 100% СОХРАНЕНИЕМ ПРОПОРЦИЙ
                if (app->image_duo_bud_) {
                    draw_image_fit(app->image_duo_bud_, 35.0f, 85.0f, 580.0f, 105.0f);
                } else {
                    if (app->image_left_bud_) {
                        draw_image_fit(app->image_left_bud_, 50.0f, 85.0f, 160.0f, 105.0f);
                    }
                    if (app->image_case_bud_) {
                        draw_image_fit(app->image_case_bud_, 230.0f, 85.0f, 190.0f, 105.0f);
                    }
                    if (app->image_right_bud_) {
                        draw_image_fit(app->image_right_bud_, 440.0f, 85.0f, 160.0f, 105.0f);
                    }
                }

                // Название модели (Georgia Serif font)
                std::wstring raw_name = snapshot_copy.device_name.empty() ? L"CMF Buds 2" : snapshot_copy.device_name;
                draw_string(raw_name, app->font_serif_, RECT{35, 198, 615, 230}, kTextWhite());

                // Индикаторы батареи (L %, Case %, R %)
                auto draw_linear_battery = [&](float x, float y, float width, const BatteryReading& reading, const std::wstring& label) {
                    SolidBrush track_brush(Color(255, 0x3A, 0x3A, 0x3A));
                    GraphicsPath track_path;
                    AddRoundRectPath(track_path, RectF(x, y, width, 4.0f), 2.0f);
                    graphics.FillPath(&track_brush, &track_path);

                    if (reading.present && reading.percent.has_value()) {
                        float pct = static_cast<float>(*reading.percent);
                        float fill_w = (pct / 100.0f) * width;
                        if (fill_w < 4.0f && pct > 0) fill_w = 4.0f;
                        SolidBrush fill_brush(Color(255, 255, 255, 255));
                        GraphicsPath fill_path;
                        AddRoundRectPath(fill_path, RectF(x, y, fill_w, 4.0f), 2.0f);
                        graphics.FillPath(&fill_brush, &fill_path);
                    }

                    std::wstring text = label + L" " + FormatBatteryReading(reading);
                    RECT label_rect{static_cast<int>(x - 20.0f), static_cast<int>(y + 5.0f), static_cast<int>(x + width + 20.0f), static_cast<int>(y + 22.0f)};
                    draw_string(text, app->font_sub_, label_rect, Color(255, 0x9A, 0x9A, 0x9A));
                };

                draw_linear_battery(80.0f, 238.0f, 130.0f, snapshot_copy.left, L"L");
                app->left_ring_rect_ = RECT{60, 238, 220, 265};

                if (snapshot_copy.case_battery.present) {
                    draw_linear_battery(260.0f, 238.0f, 130.0f, snapshot_copy.case_battery, L"Case");
                }

                draw_linear_battery(440.0f, 238.0f, 130.0f, snapshot_copy.right, L"R");
                app->right_ring_rect_ = RECT{420, 238, 590, 265};

                // 4. КАРТОЧКА: NOISE CONTROL (КОНТЕЙНЕР 2 - ЛЕВЫЙ)
                app->anc_card_rect_ = RECT{35, 287, 315, 517};
                FillRoundedRect(graphics, app->anc_card_rect_, 20.0f, Color(255, 0x1C, 0x1C, 0x1C));
                draw_string(app->Tr(L"Шумоподавление", L"Noise Control"), app->font_title_, RECT{50, 297, 300, 319}, kTextWhite(), StringAlignmentNear, StringAlignmentCenter);
                draw_string(app->Tr(L"Регулировка изоляции", L"Isolation Control"), app->font_sub_, RECT{50, 319, 300, 335}, Color(255, 0x9A, 0x9A, 0x9A), StringAlignmentNear, StringAlignmentCenter);

                // 3 Круглые Radio-кнопки (Шум, Прозрачность, Выкл.)
                const std::array<std::wstring, 3> anc_labels = {
                    app->Tr(L"Шум", L"Noise"),
                    app->Tr(L"Прозрачность", L"Trans"),
                    app->Tr(L"Выкл.", L"Off")
                };
                const std::array<bool, 3> anc_active = {
                    app->selected_anc_ == AncMode::High || app->selected_anc_ == AncMode::Low || app->selected_anc_ == AncMode::Mid || app->selected_anc_ == AncMode::Adaptive,
                    app->selected_anc_ == AncMode::Transparency,
                    app->selected_anc_ == AncMode::Off,
                };
                const int btn_start_x = 55;
                const int btn_gap = 22;
                const int btn_size = 54;
                for (size_t i = 0; i < 3; ++i) {
                    int bx = btn_start_x + static_cast<int>(i) * (btn_size + btn_gap);
                    int by = 348;
                    app->anc_top_rects_[i] = RECT{bx, by, bx + btn_size, by + btn_size};

                    const bool active = anc_active[i];
                    SolidBrush btn_brush(active ? Color(255, 255, 255, 255) : Color(255, 0x2A, 0x2A, 0x2A));
                    graphics.FillEllipse(&btn_brush, static_cast<REAL>(bx), static_cast<REAL>(by), static_cast<REAL>(btn_size), static_cast<REAL>(btn_size));
                    
                    Color icon_col = active ? Color(255, 0x1C, 0x1C, 0x1C) : Color(255, 255, 255, 255);
                    SolidBrush icon_brush(icon_col);
                    graphics.FillEllipse(&icon_brush, static_cast<REAL>(bx + 22), static_cast<REAL>(by + 22), 10.0f, 10.0f);

                    RECT label_rect{bx - 10, by + btn_size + 4, bx + btn_size + 10, by + btn_size + 20};
                    draw_string(anc_labels[i], app->font_small_, label_rect, active ? kTextWhite() : Color(255, 0x9A, 0x9A, 0x9A));
                }

                // Сегментированные селекторы уровней ANC (Высокое, Среднее, Низкое, Адаптив.)
                const std::array<AncMode, 4> anc_modes = {AncMode::High, AncMode::Mid, AncMode::Low, AncMode::Adaptive};
                const std::array<std::wstring, 4> anc_dot_labels = {
                    app->Tr(L"Высокое", L"High"),
                    app->Tr(L"Среднее", L"Mid"),
                    app->Tr(L"Низкое", L"Low"),
                    app->Tr(L"Адаптив.", L"Adaptive")
                };
                
                const int chip_y = 445;
                const int chip_w = 56;
                const int chip_gap = 6;
                for (size_t i = 0; i < 4; ++i) {
                    int cx = 48 + static_cast<int>(i) * (chip_w + chip_gap);
                    app->anc_mode_rects_[i] = RECT{cx, chip_y, cx + chip_w, chip_y + 26};

                    const bool active = (app->selected_anc_ == anc_modes[i]);
                    SolidBrush cap_brush(active ? kAccentRed() : Color(255, 0x2A, 0x2A, 0x2A));
                    GraphicsPath cap_path;
                    AddRoundRectPath(cap_path, RectF(static_cast<REAL>(cx), static_cast<REAL>(chip_y), static_cast<REAL>(chip_w), 24.0f), 10.0f);
                    graphics.FillPath(&cap_brush, &cap_path);

                    draw_string(anc_dot_labels[i], app->font_small_, app->anc_mode_rects_[i], active ? kTextWhite() : Color(255, 0x9A, 0x9A, 0x9A));
                }

                // 5. КАРТОЧКА: EQUALIZER & ULTRA BASS (КОНТЕЙНЕР 3 - ПРАВЫЙ)
                app->eq_card_rect_ = RECT{335, 287, 615, 517};
                FillRoundedRect(graphics, app->eq_card_rect_, 20.0f, Color(255, 0x1C, 0x1C, 0x1C));
                draw_string(app->Tr(L"Эквалайзер", L"Equalizer"), app->font_title_, RECT{350, 297, 600, 319}, kTextWhite(), StringAlignmentNear, StringAlignmentCenter);
                draw_string(app->Tr(L"Звуковые пресеты", L"Sound Presets"), app->font_sub_, RECT{350, 319, 600, 335}, Color(255, 0x9A, 0x9A, 0x9A), StringAlignmentNear, StringAlignmentCenter);

                // 2x2 Пресеты
                const std::array<EqPreset, 5> presets = {EqPreset::Balanced, EqPreset::MoreBass, EqPreset::MoreTreble, EqPreset::Voice, EqPreset::Dirac};
                const std::array<std::wstring, 5> preset_labels = {
                    app->Tr(L"Базовый", L"Balanced"),
                    app->Tr(L"Больше баса", L"More Bass"),
                    app->Tr(L"Больше высоких", L"More Treble"),
                    app->Tr(L"Вокал", L"Voice"),
                    L"★ Dirac Opteo"
                };

                const int preset_w = 120;
                const int preset_h = 26;
                const int preset_gap_x = 10;
                const int preset_gap_y = 6;
                const int preset_start_x = 350;
                const int preset_start_y = 345;

                app->eq_preset_rects_[0] = RECT{preset_start_x, preset_start_y, preset_start_x + preset_w, preset_start_y + preset_h};
                app->eq_preset_rects_[1] = RECT{preset_start_x + preset_w + preset_gap_x, preset_start_y, preset_start_x + (preset_w * 2) + preset_gap_x, preset_start_y + preset_h};
                app->eq_preset_rects_[2] = RECT{preset_start_x, preset_start_y + preset_h + preset_gap_y, preset_start_x + preset_w, preset_start_y + (preset_h * 2) + preset_gap_y};
                app->eq_preset_rects_[3] = RECT{preset_start_x + preset_w + preset_gap_x, preset_start_y + preset_h + preset_gap_y, preset_start_x + (preset_w * 2) + preset_gap_x, preset_start_y + (preset_h * 2) + preset_gap_y};
                app->eq_preset_rects_[4] = RECT{preset_start_x, preset_start_y + (preset_h * 2) + (preset_gap_y * 2), preset_start_x + (preset_w * 2) + preset_gap_x, preset_start_y + (preset_h * 3) + (preset_gap_y * 2)};

                for (size_t i = 0; i < 5; ++i) {
                    const bool active = app->selected_eq_ == presets[i];
                    FillRoundedRect(graphics, app->eq_preset_rects_[i], 10.0f, active ? kAccentRed() : Color(255, 0x2A, 0x2A, 0x2A));
                    draw_string(preset_labels[i], app->font_small_, app->eq_preset_rects_[i], active ? kTextWhite() : Color(255, 0x9A, 0x9A, 0x9A));
                }

                // Ultra Bass контейнер
                app->eq_custom_rect_ = RECT{350, 450, 600, 502};
                FillRoundedRect(graphics, app->eq_custom_rect_, 12.0f, Color(255, 0x2A, 0x2A, 0x2A));

                std::wstring bass_text = app->bass_enabled_ ? 
                    (std::wstring(app->Tr(L"Ultra Bass: ", L"Ultra Bass: ")) + std::to_wstring(app->bass_level_)) : 
                    app->Tr(L"Ultra Bass: Выкл.", L"Ultra Bass: Off");
                
                draw_string(bass_text, app->font_small_, RECT{app->eq_custom_rect_.left + 10, app->eq_custom_rect_.top, app->eq_custom_rect_.left + 115, app->eq_custom_rect_.bottom}, app->bass_enabled_ ? kTextWhite() : Color(255, 0x9A, 0x9A, 0x9A), StringAlignmentNear, StringAlignmentCenter);

                // iOS-style Toggle Switch capsule на правой стороне
                app->eq_custom_toggle_rect_ = RECT{app->eq_custom_rect_.right - 44, app->eq_custom_rect_.top + 14, app->eq_custom_rect_.right - 8, app->eq_custom_rect_.top + 38};
                const float toggle_x = static_cast<float>(app->eq_custom_toggle_rect_.left);
                const float toggle_y = static_cast<float>(app->eq_custom_toggle_rect_.top);
                GraphicsPath toggle_track;
                AddRoundRectPath(toggle_track, RectF(toggle_x, toggle_y, 36.0f, 22.0f), 11.0f);
                SolidBrush toggle_track_brush(app->bass_enabled_ ? kAccentRed() : Color(255, 0x3A, 0x3A, 0x3A));
                graphics.FillPath(&toggle_track_brush, &toggle_track);

                const float thumb_x = app->bass_enabled_ ? (toggle_x + 16.0f) : (toggle_x + 2.0f);
                SolidBrush thumb_brush(Color(255, 0xEE, 0xEE, 0xEE));
                graphics.FillEllipse(&thumb_brush, thumb_x, toggle_y + 2.0f, 18.0f, 18.0f);

                // ПЛАВНЫЙ СЛАЙДЕР ЗВУКА ULTRA BASS 1-5
                if (app->bass_slider_open_) {
                    const int track_left = app->eq_custom_rect_.left + 115;
                    const int track_right = app->eq_custom_toggle_rect_.left - 12;
                    const int track_y = app->eq_custom_rect_.top + 23;
                    const int track_w = track_right - track_left;

                    app->bass_slider_track_rect_ = RECT{track_left - 10, app->eq_custom_rect_.top, track_right + 10, app->eq_custom_rect_.bottom};

                    // Серая дорожка трека (Slider Track)
                    SolidBrush track_bg(Color(255, 0x3A, 0x3A, 0x3A));
                    GraphicsPath track_path;
                    AddRoundRectPath(track_path, RectF(static_cast<REAL>(track_left), static_cast<REAL>(track_y), static_cast<REAL>(track_w), 6.0f), 3.0f);
                    graphics.FillPath(&track_bg, &track_path);

                    // Заполненная часть слайдера (Active Fill)
                    float fill_ratio = app->bass_enabled_ ? (static_cast<float>(app->bass_level_ - 1) / 4.0f) : 0.0f;
                    float fill_w = fill_ratio * track_w;
                    if (fill_w > 0.0f) {
                        SolidBrush fill_brush(kAccentRed());
                        GraphicsPath fill_path;
                        AddRoundRectPath(fill_path, RectF(static_cast<REAL>(track_left), static_cast<REAL>(track_y), fill_w, 6.0f), 3.0f);
                        graphics.FillPath(&fill_brush, &fill_path);
                    }

                    // Ручка слайдера (Slider Thumb Knob)
                    float thumb_pos_x = track_left + fill_w;
                    SolidBrush thumb_knob(Color(255, 255, 255, 255));
                    graphics.FillEllipse(&thumb_knob, thumb_pos_x - 8.0f, static_cast<REAL>(track_y - 5), 16.0f, 16.0f);
                } else {
                    // Компактные 5 бургер-полосок (Burger level bars)
                    const float burger_x = static_cast<float>(app->eq_custom_toggle_rect_.left - 26);
                    const float burger_top = static_cast<float>(app->eq_custom_rect_.top + 14);
                    for (int i = 0; i < 5; ++i) {
                        int level_num = 5 - i;
                        const bool active = app->bass_enabled_ && (app->bass_level_ >= level_num);
                        SolidBrush bar_brush(active ? kAccentRed() : Color(255, 0x3A, 0x3A, 0x3A));
                        GraphicsPath bar_path;
                        AddRoundRectPath(bar_path, RectF(burger_x, burger_top + (i * 4.5f), 18.0f, 3.2f), 1.5f);
                        graphics.FillPath(&bar_brush, &bar_path);
                    }
                }

                // 6. РЕЖИМ МАЛОЙ ЗАДЕРЖКИ (КОНТЕЙНЕР 4 - НИЖНИЙ)
                app->low_latency_rect_ = RECT{35, 529, 615, 569};
                FillRoundedRect(graphics, app->low_latency_rect_, 20.0f, app->low_latency_enabled_ ? kAccentRed() : Color(255, 0x1C, 0x1C, 0x1C));
                std::wstring lat_text = app->low_latency_enabled_ ? 
                    app->Tr(L"Режим малой задержки: Вкл.", L"Low Latency Mode: On") : 
                    app->Tr(L"Режим малой задержки: Выкл.", L"Low Latency Mode: Off");
                draw_string(lat_text, app->font_button_, app->low_latency_rect_, app->low_latency_enabled_ ? kTextWhite() : Color(255, 0x9A, 0x9A, 0x9A));
            }
        }

        BitBlt(hdc, 0, 0, width, height, mem_dc, 0, 0, SRCCOPY);

        SelectObject(mem_dc, old_bitmap);
        DeleteObject(mem_bitmap);
        DeleteDC(mem_dc);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        const int x = GET_X_LPARAM(lparam);
        const int y = GET_Y_LPARAM(lparam);
        POINT point{x, y};

        // Языковая кнопка [ RU / EN ]
        if (PtInAnyRect(app->lang_btn_rect_, point)) {
            app->language_ = (app->language_ == AppLanguage::Russian) ? AppLanguage::English : AppLanguage::Russian;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        const bool is_online = (app->spp_client_ && app->spp_client_->IsConnected());
        RECT status_click_rect{35, 48, 300, 70};
        RECT splash_connect_rect{170, 435, 480, 480};
        if (!is_online && (PtInAnyRect(status_click_rect, point) || PtInAnyRect(splash_connect_rect, point))) {
            app->QueueConnection();
            return 0;
        }

        // Кнопки бузера поиска наушников
        if (PtInAnyRect(app->left_ring_rect_, point)) {
            app->TriggerRing(true);
            return 0;
        }
        if (PtInAnyRect(app->right_ring_rect_, point)) {
            app->TriggerRing(false);
            return 0;
        }

        // Режимы ANC (Noise, Transparency, Off)
        if (PtInAnyRect(app->anc_top_rects_[0], point)) {
            if (app->selected_anc_ != AncMode::High && app->selected_anc_ != AncMode::Low &&
                app->selected_anc_ != AncMode::Mid && app->selected_anc_ != AncMode::Adaptive) {
                app->selected_anc_ = AncMode::High;
            }
            app->ApplySelectedAnc();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (PtInAnyRect(app->anc_top_rects_[1], point)) {
            app->selected_anc_ = AncMode::Transparency;
            app->ApplySelectedAnc();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (PtInAnyRect(app->anc_top_rects_[2], point)) {
            app->selected_anc_ = AncMode::Off;
            app->ApplySelectedAnc();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        // Селекторы уровней ANC (Высокое, Среднее, Низкое, Адаптив.)
        const std::array<AncMode, 4> anc_modes = {AncMode::High, AncMode::Mid, AncMode::Low, AncMode::Adaptive};
        for (size_t i = 0; i < app->anc_mode_rects_.size(); ++i) {
            if (PtInAnyRect(app->anc_mode_rects_[i], point)) {
                app->selected_anc_ = anc_modes[i];
                app->ApplySelectedAnc();
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }

        // Пресеты Эквалайзера
        const std::array<EqPreset, 5> presets = {EqPreset::Balanced, EqPreset::MoreBass, EqPreset::MoreTreble, EqPreset::Voice, EqPreset::Dirac};
        for (size_t i = 0; i < app->eq_preset_rects_.size(); ++i) {
            if (PtInAnyRect(app->eq_preset_rects_[i], point)) {
                app->ApplyEqPreset(presets[i]);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }

        // Ultra Bass Toggle Switch (вкл / выкл)
        if (PtInAnyRect(app->eq_custom_toggle_rect_, point)) {
            app->bass_enabled_ = !app->bass_enabled_;
            if (app->spp_client_ && app->spp_client_->IsConnected()) {
                app->spp_client_->SendBass(app->bass_enabled_, app->bass_level_);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        // Плавный клик / таскание слайдера Ultra Bass
        if (app->bass_slider_open_ && PtInAnyRect(app->bass_slider_track_rect_, point)) {
            app->bass_dragging_ = true;
            SetCapture(hwnd);

            const int track_left = app->eq_custom_rect_.left + 115;
            const int track_right = app->eq_custom_toggle_rect_.left - 12;
            const int track_w = track_right - track_left;
            if (track_w > 0) {
                float rel_x = static_cast<float>(x - track_left) / static_cast<float>(track_w);
                if (rel_x < 0.0f) rel_x = 0.0f;
                if (rel_x > 1.0f) rel_x = 1.0f;
                uint8_t level = static_cast<uint8_t>(std::round(rel_x * 4.0f)) + 1;
                if (level != app->bass_level_) {
                    app->bass_level_ = level;
                    app->bass_enabled_ = true;
                    if (app->spp_client_ && app->spp_client_->IsConnected()) {
                        app->spp_client_->SendBass(app->bass_enabled_, app->bass_level_);
                    }
                }
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (PtInAnyRect(app->eq_custom_rect_, point)) {
            app->bass_slider_open_ = !app->bass_slider_open_;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        // Кнопка свернуть (Minimize)
        if (PtInAnyRect(app->minimize_btn_rect_, point)) {
            ShowWindow(hwnd, SW_MINIMIZE);
            return 0;
        }

        // Кнопка закрыть (Close)
        if (PtInAnyRect(app->close_btn_rect_, point)) {
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }

        // Режим малой задержки (Low Latency Mode)
        if (PtInAnyRect(app->low_latency_rect_, point)) {
            app->low_latency_enabled_ = !app->low_latency_enabled_;
            if (app->spp_client_ && app->spp_client_->IsConnected()) {
                app->spp_client_->SendLowLatency(app->low_latency_enabled_);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (app->bass_dragging_) {
            const int x = GET_X_LPARAM(lparam);
            const int track_left = app->eq_custom_rect_.left + 115;
            const int track_right = app->eq_custom_toggle_rect_.left - 12;
            const int track_w = track_right - track_left;
            if (track_w > 0) {
                float rel_x = static_cast<float>(x - track_left) / static_cast<float>(track_w);
                if (rel_x < 0.0f) rel_x = 0.0f;
                if (rel_x > 1.0f) rel_x = 1.0f;
                uint8_t level = static_cast<uint8_t>(std::round(rel_x * 4.0f)) + 1;
                if (level != app->bass_level_) {
                    app->bass_level_ = level;
                    app->bass_enabled_ = true;
                    if (app->spp_client_ && app->spp_client_->IsConnected()) {
                        app->spp_client_->SendBass(app->bass_enabled_, app->bass_level_);
                    }
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
            return 0;
        }
        break;
    }
    case WM_LBUTTONUP: {
        if (app->bass_dragging_) {
            app->bass_dragging_ = false;
            ReleaseCapture();
            return 0;
        }
        break;
    }
    case WM_SHOWWINDOW:
        if (wparam) {
            const bool already_online = (app->spp_client_ && app->spp_client_->IsConnected());
            if (!already_online) {
                app->QueueConnection();
            }
        }
        return 0;
    case WM_ACTIVATE:
        // Окно работает как обычное стабильное приложение, удаление авто-закрытия при потере фокуса
        return 0;
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    case WM_DESTROY:
        app->ReleaseUiResources();
        return 0;
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

} // namespace nothing_tray