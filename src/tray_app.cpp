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
const Color kCanvas(255, 0x00, 0x00, 0x00);               // Настоящий чёрный
const Color kCardBg(255, 0x14, 0x14, 0x16);               // Тёмно-серый для карточек (#141416)
const Color kCardBorder(255, 0x2A, 0x2A, 0x2E);           // Граница карточек
const Color kTextWhite(255, 0xF4, 0xF4, 0xF6);            // Основной белый шрифт
const Color kTextMuted(255, 0x8A, 0x8A, 0x8F);            // Приглушённый серый
const Color kDotActive(255, 0xFF, 0xFF, 0xFF);            // Активная точка
const Color kPillActive(255, 0xFF, 0xFF, 0xFF);           // Активная подложка-пилюля
const Color kDarkRedFill(255, 0x22, 0x08, 0x08);          // Тёмно-красный для Ring
const Color kDarkRedStroke(255, 0x5C, 0x14, 0x14);        // Обводка Ring

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

void FillRoundedRect(Graphics& graphics, const RECT& rect, float radius, const Color& fill, const Color& stroke) {
    GraphicsPath path;
    AddRoundRectPath(path, RectF(static_cast<REAL>(rect.left), static_cast<REAL>(rect.top),
                                 static_cast<REAL>(rect.right - rect.left), static_cast<REAL>(rect.bottom - rect.top)),
                     radius);
    SolidBrush brush(fill);
    Pen pen(stroke, 1.2f);
    graphics.FillPath(&brush, &path);
    graphics.DrawPath(&pen, &path);
}

// Полностью перерисовываем наушники вектором в стиле Nothing (если WebP картинки отсутствуют)
void DrawVectorEarbud(Graphics& graphics, int x, int y, bool is_right) {
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);

    // 1. Голова наушника (глянцевый серебристый круг)
    RectF head_rect(static_cast<REAL>(x + 15), static_cast<REAL>(y + 15), 45.0f, 45.0f);
    SolidBrush head_brush(Color(255, 0xE0, 0xE0, 0xE2));
    graphics.FillEllipse(&head_brush, head_rect);

    // Внутренняя структура
    RectF inner_head(static_cast<REAL>(x + 22), static_cast<REAL>(y + 22), 31.0f, 31.0f);
    SolidBrush inner_brush(Color(255, 0xB0, 0xB0, 0xB5));
    graphics.FillEllipse(&inner_brush, inner_head);

    // 2. Стержень (прозрачный тёмный закруглённый прямоугольник)
    GraphicsPath stem_path;
    AddRoundRectPath(stem_path, RectF(static_cast<REAL>(x + 27), static_cast<REAL>(y + 45), 21.0f, 65.0f), 8.0f);
    SolidBrush stem_brush(Color(180, 0x20, 0x20, 0x25)); // Полупрозрачный серый
    Pen stem_pen(Color(255, 0x50, 0x50, 0x55), 1.0f);
    graphics.FillPath(&stem_brush, &stem_path);
    graphics.DrawPath(&stem_pen, &stem_path);

    // 3. Красная (Правая) или Белая (Левая) точка позиционирования
    RectF dot_rect(static_cast<REAL>(x + 34), static_cast<REAL>(y + 55), 7.0f, 7.0f);
    SolidBrush dot_brush(is_right ? Color(255, 0xD0, 0x25, 0x25) : Color(255, 0xFA, 0xFA, 0xFA));
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
    
    tray_data_.cbSize = sizeof(tray_data_);
    tray_data_.uID = 1;
    tray_data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    tray_data_.uCallbackMessage = kTrayIconMessage;
    tray_data_.uVersion = NOTIFYICON_VERSION_4;

    GdiplusStartupInput startup_input{};
    GdiplusStartup(&gdiplus_token_, &startup_input, nullptr);
}

TrayApp::~TrayApp() {
    shutting_down_.store(true, std::memory_order_release);
    if (host_window_) {
        KillTimer(host_window_, kBatterySmoothingTimerId);
    }
    DisconnectSpp();
    ble_monitor_.Stop();
    DestroyControlWindow();
    RemoveTrayIcon();
    if (current_tray_icon_) {
        DestroyIcon(current_tray_icon_);
        current_tray_icon_ = nullptr;
    }
    if (spp_connect_thread_.joinable()) {
        spp_connect_thread_.join();
    }
    ReleaseUiResources();
    if (gdiplus_token_) {
        GdiplusShutdown(gdiplus_token_);
    }
}

int TrayApp::Run(HINSTANCE instance) {
    instance_ = instance;
    
    spp_client_.SetUpdateCallback([this](const SppStateUpdate& update) { OnSppStateUpdate(update); });

    RegisterWindowClasses();
    CreateHostWindow();
    AddTrayIcon();

    ble_monitor_.Start([this](const BatterySnapshot& snapshot) { OnBleSnapshot(snapshot); });

    // Сразу показываем окно для отладки
    ShowControlWindow();

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
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

    // Убран флаг WS_EX_TOPMOST, окно больше не липнет поверх других окон!
    // WS_EX_APPWINDOW принудительно отображает окно в панели задач при его видимости
    control_window_ = CreateWindowExW(WS_EX_APPWINDOW, kControlClassName, L"Nothing / CMF Controls",
                                      WS_POPUP | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, kWindowWidth, kWindowHeight,
                                      nullptr, nullptr, instance_, this);
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
    QueueConnection();
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
        snapshot_.device_name = snapshot.device_name;
        snapshot_.bluetooth_address = snapshot.bluetooth_address;

        if (device_sku_.empty() && !snapshot.device_name.empty()) {
            std::wstring guessed_sku = GuessSkuFromName(snapshot.device_name);
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
    if (connected && spp_connect_pending_.load(std::memory_order_acquire)) {
        spp_connect_pending_.store(false, std::memory_order_release);
    }

    PostUiMessage(kSppStatusMessage, connected ? 1 : 0, 0);
    if (connected) {
        spp_client_.QueryDeviceState();
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
            AncMode mode = update.anc_mode;
            if (IsSwappedAncSku(device_sku_)) {
                if (mode == AncMode::Transparency) mode = AncMode::Off;
                else if (mode == AncMode::Off) mode = AncMode::Transparency;
            }
            selected_anc_ = mode;
            state_changed = true;
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
            if (device_sku_.empty()) {
                snapshot_.device_name = L"Nothing Ear (" + update.text_value + L")";
            }
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
    if (spp_connect_pending_.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

    uint64_t address = 0;
    {
        std::scoped_lock lock(state_mutex_);
        address = snapshot_.bluetooth_address;
    }

    if (spp_connect_thread_.joinable()) {
        spp_connect_thread_.join();
    }

    spp_connect_thread_ = std::thread([this, address] {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        
        bool connected = false;
        if (address != 0) {
            // Подключаемся напрямую к MAC-адресу, который поймал BLE-сканнер!
            connected = spp_client_.ConnectToAddress(address);
        } else {
            // Fallback поиск
            connected = spp_client_.Connect();
        }
        OnSppStatusChanged(connected);
    });
}

void TrayApp::DisconnectSpp() {
    spp_connect_pending_.store(false, std::memory_order_release);
    spp_client_.Disconnect();
}

AncMode TrayApp::SelectedAncMode() const {
    return selected_anc_;
}

std::pair<bool, uint8_t> TrayApp::SelectedBassState() const {
    return {bass_enabled_, bass_level_};
}

void TrayApp::ApplySelectedAnc() {
    if (!spp_client_.IsConnected()) {
        QueueConnection();
        return;
    }
    AncMode mode = SelectedAncMode();
    if (IsSwappedAncSku(device_sku_)) {
        if (mode == AncMode::Transparency) mode = AncMode::Off;
        else if (mode == AncMode::Off) mode = AncMode::Transparency;
    }
    spp_client_.SendAnc(mode);
}

void TrayApp::ApplySelectedBass() {
    if (!spp_client_.IsConnected()) {
        QueueConnection();
        return;
    }
    const auto [enabled, level] = SelectedBassState();
    spp_client_.SendBass(enabled, level);
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
    }
    ApplySelectedBass();
    if (spp_client_.IsConnected()) {
        spp_client_.SendEq(preset_val);
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
    if (sku.empty()) return false;
    try {
        int sku_val = std::stoi(sku);
        if ((sku_val >= 30 && sku_val <= 35) ||
            (sku_val >= 48 && sku_val <= 53) ||
            (sku_val >= 54 && sku_val <= 59) ||
            (sku_val >= 63 && sku_val <= 68) ||
            (sku_val >= 71 && sku_val <= 73) ||
            (sku_val >= 76 && sku_val <= 83)) {
            return true;
        }
    } catch (...) {}
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
        model_display_name = L"Nothing Ear (1) White";
    } else if (sku == L"02" || sku == L"04" || sku == L"06" || sku == L"08" || sku == L"10") {
        left_file = L"ear_one_black_left.png";
        case_file = L"ear_one_black_case.png";
        right_file = L"ear_one_black_right.png";
        model_display_name = L"Nothing Ear (1) Black";
    } else if (sku == L"14" || sku == L"15" || sku == L"16") {
        left_file = L"ear_stick_left.png";
        case_file = L"ear_stick_case_none.png";
        right_file = L"ear_stick_right.png";
        model_display_name = L"Nothing Ear (stick)";
    } else if (sku == L"17" || sku == L"18" || sku == L"19") {
        left_file = L"ear_two_white_left.png";
        case_file = L"ear_two_white_case.png";
        right_file = L"ear_two_white_right.png";
        model_display_name = L"Nothing Ear (2) White";
    } else if (sku == L"27" || sku == L"28" || sku == L"29") {
        left_file = L"ear_two_black_left.png";
        case_file = L"ear_two_black_case.png";
        right_file = L"ear_two_black_right.png";
        model_display_name = L"Nothing Ear (2) Black";
    } else if (sku == L"30" || sku == L"31") {
        left_file = L"ear_corsola_black_left.png";
        case_file = L"ear_corsola_black_case.png";
        right_file = L"ear_corsola_black_right.png";
        model_display_name = L"CMF Buds Pro Black";
    } else if (sku == L"32" || sku == L"33") {
        left_file = L"ear_corsola_white_left.png";
        case_file = L"ear_corsola_white_case.png";
        right_file = L"ear_corsola_white_right.png";
        model_display_name = L"CMF Buds Pro White";
    } else if (sku == L"34" || sku == L"35") {
        left_file = L"ear_corsola_orange_left.png";
        case_file = L"ear_corsola_orange_case.png";
        right_file = L"ear_corsola_orange_right.png";
        model_display_name = L"CMF Buds Pro Orange";
    } else if (sku == L"48" || sku == L"53") {
        duo_file = L"crobat_orange.png";
        model_display_name = L"Nothing Ear (a) Orange";
    } else if (sku == L"49" || sku == L"52") {
        duo_file = L"crobat_white.png";
        model_display_name = L"Nothing Ear (a) White";
    } else if (sku == L"50" || sku == L"51") {
        duo_file = L"crobat_black.png";
        model_display_name = L"Nothing Ear (a) Black";
    } else if (sku == L"54" || sku == L"55") {
        left_file = L"donphan_black_left.png";
        case_file = L"donphan_black_case.png";
        right_file = L"donphan_black_right.png";
        model_display_name = L"CMF Buds Black";
    } else if (sku == L"56" || sku == L"57") {
        left_file = L"donphan_white_left.png";
        case_file = L"donphan_white_case.png";
        right_file = L"donphan_white_right.png";
        model_display_name = L"CMF Buds White";
    } else if (sku == L"58" || sku == L"59") {
        left_file = L"donphan_orange_left.png";
        case_file = L"donphan_orange_case.png";
        right_file = L"donphan_orange_right.png";
        model_display_name = L"CMF Buds Orange";
    } else if (sku == L"61" || sku == L"69" || sku == L"74") {
        left_file = L"ear_twos_black_left.png";
        case_file = L"ear_twos_black_case.png";
        right_file = L"ear_twos_black_right.png";
        model_display_name = L"Nothing Ear Black";
    } else if (sku == L"62" || sku == L"70" || sku == L"75") {
        left_file = L"ear_twos_white_left.png";
        case_file = L"ear_twos_white_case.png";
        right_file = L"ear_twos_white_right.png";
        model_display_name = L"Nothing Ear White";
    } else if (sku == L"63" || sku == L"66" || sku == L"71") {
        left_file = L"ear_color_black_left.png";
        case_file = L"ear_color_black_case.png";
        right_file = L"ear_color_black_right.png";
        model_display_name = L"Nothing Ear (open) Black";
    } else if (sku == L"64" || sku == L"67" || sku == L"72") {
        left_file = L"ear_color_white_left.png";
        case_file = L"ear_color_white_case.png";
        right_file = L"ear_color_white_right.png";
        model_display_name = L"Nothing Ear (open) White";
    } else if (sku == L"65" || sku == L"68" || sku == L"73") {
        left_file = L"ear_color_yellow_left.png";
        case_file = L"ear_color_yellow_case.png";
        right_file = L"ear_color_yellow_right.png";
        model_display_name = L"Nothing Ear (open) Yellow";
    } else if (sku == L"76" || sku == L"83") {
        left_file = L"espeon_black_left.png";
        case_file = L"espeon_black_case.png";
        right_file = L"espeon_black_right.png";
        model_display_name = L"CMF Buds Pro 2 Black";
    } else if (sku == L"77" || sku == L"82") {
        left_file = L"espeon_white_left.png";
        case_file = L"espeon_white_case.png";
        right_file = L"espeon_white_right.png";
        model_display_name = L"CMF Buds Pro 2 White";
    } else if (sku == L"78" || sku == L"81") {
        left_file = L"espeon_orange_left.png";
        case_file = L"espeon_orange_case.png";
        right_file = L"espeon_orange_right.png";
        model_display_name = L"CMF Buds Pro 2 Orange";
    } else if (sku == L"79" || sku == L"80") {
        left_file = L"espeon_blue_left.png";
        case_file = L"espeon_blue_case.png";
        right_file = L"espeon_blue_right.png";
        model_display_name = L"CMF Buds Pro 2 Blue";
    } else if (sku == L"11200005") {
        left_file = L"flaffy_white_left.png";
        case_file = L"flaffy_white_case.png";
        right_file = L"flaffy_white_right.png";
        model_display_name = L"Nothing Ear (open)";
    } else {
        left_file = L"donphan_white_left.png";
        case_file = L"donphan_white_case.png";
        right_file = L"donphan_white_right.png";
        model_display_name = L"Nothing Earbuds";
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

    snapshot_.device_name = model_display_name;
    DebugLog(L"ReloadBudImages: Loaded resources for SKU: " + sku + L" (" + model_display_name + L")");
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

    // Register custom fonts natively at the process-level using Win32 API to ensure correct visual rendering
    AddFontResourceExW(heading_path.c_str(), FR_PRIVATE, nullptr);
    AddFontResourceExW(body_path.c_str(), FR_PRIVATE, nullptr);
    AddFontResourceExW(button_path.c_str(), FR_PRIVATE, nullptr);

    font_collection_.AddFontFile(heading_path.c_str());
    font_collection_.AddFontFile(body_path.c_str());
    font_collection_.AddFontFile(button_path.c_str());

    auto make_font = [&](const wchar_t* family_name, float size, int style) -> Gdiplus::Font* {
        Gdiplus::FontFamily family(family_name, &font_collection_);
        if (family.GetLastStatus() == Gdiplus::Ok) {
            return new Gdiplus::Font(&family, size, style, Gdiplus::UnitPixel);
        }
        Gdiplus::FontFamily sys_family(family_name);
        if (sys_family.GetLastStatus() == Gdiplus::Ok) {
            return new Gdiplus::Font(&sys_family, size, style, Gdiplus::UnitPixel);
        }
        return new Gdiplus::Font(L"Segoe UI", size, style, Gdiplus::UnitPixel);
    };

    font_heading_ = make_font(L"Ndot 55", 28.0f, Gdiplus::FontStyleRegular);
    font_body_ = make_font(L"Space Grotesk", 16.0f, Gdiplus::FontStyleRegular);
    font_button_ = make_font(L"Manrope", 14.0f, Gdiplus::FontStyleBold);
    font_small_ = make_font(L"Manrope", 12.0f, Gdiplus::FontStyleRegular);

    brush_canvas_ = CreateSolidBrush(RGB(0, 0, 0));
    brush_surface_ = CreateSolidBrush(RGB(0x14, 0x14, 0x16));
    brush_surface_elevated_ = CreateSolidBrush(RGB(0x1B, 0x1D, 0x1F));
    brush_surface_card_ = CreateSolidBrush(RGB(0x12, 0x12, 0x12));

    ReloadBudImages(L"");
}

void TrayApp::ReleaseUiResources() {
    if (font_heading_) { delete font_heading_; font_heading_ = nullptr; }
    if (font_body_) { delete font_body_; font_body_ = nullptr; }
    if (font_button_) { delete font_button_; font_button_ = nullptr; }
    if (font_small_) { delete font_small_; font_small_ = nullptr; }

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
    
    // Симметричное позиционирование карточек: ширина 265, отступы слева и справа по 45, расстояние между карточками 30.
    anc_card_rect_ = RECT{45, 320, 310, 530};
    eq_card_rect_ = RECT{340, 320, 605, 530};
    
    // Кнопки ANC на левой карточке (аккуратный отступ 10 от краев карточки 45..310 -> кнопки 55..300, общая ширина 245)
    const int anc_button_y = 370;
    anc_top_rects_[0] = RECT{55, anc_button_y, 131, anc_button_y + 35};
    anc_top_rects_[1] = RECT{139, anc_button_y, 215, anc_button_y + 35};
    anc_top_rects_[2] = RECT{223, anc_button_y, 300, anc_button_y + 35};
    
    // Слайдер ANC (точки 103..117, 148..162, 193..207, 238..252, центры ровно на 110, 155, 200, 245)
    const int anc_line_y = 430;
    anc_mode_rects_[0] = RECT{103, anc_line_y - 7, 117, anc_line_y + 7};
    anc_mode_rects_[1] = RECT{148, anc_line_y - 7, 162, anc_line_y + 7};
    anc_mode_rects_[2] = RECT{193, anc_line_y - 7, 207, anc_line_y + 7};
    anc_mode_rects_[3] = RECT{238, anc_line_y - 7, 252, anc_line_y + 7};
    
    // Кнопка Personalised ANC
    anc_toggle_rect_ = RECT{55, 475, 300, 515};
    
    // Кнопки пресетов эквалайзера (карточка 340..605 -> кнопки 355..590, общая ширина 235)
    const int eq_button_y = 370;
    const int eq_button_height = 40;
    const int eq_button_spacing = 8;
    eq_preset_rects_[0] = RECT{355, eq_button_y, 468, eq_button_y + eq_button_height};
    eq_preset_rects_[1] = RECT{477, eq_button_y, 590, eq_button_y + eq_button_height};
    eq_preset_rects_[2] = RECT{355, eq_button_y + eq_button_height + eq_button_spacing, 468, eq_button_y + (eq_button_height * 2) + eq_button_spacing};
    eq_preset_rects_[3] = RECT{477, eq_button_y + eq_button_height + eq_button_spacing, 590, eq_button_y + (eq_button_height * 2) + eq_button_spacing};
    
    // Кнопка Bass Enhance
    eq_custom_rect_ = RECT{355, 475, 590, 515};

    // Кнопки управления в заголовке
    minimize_btn_rect_ = RECT{kWindowWidth - 70, 10, kWindowWidth - 45, 35};
    close_btn_rect_ = RECT{kWindowWidth - 40, 10, kWindowWidth - 15, 35};

    // Нижняя кнопка-переключатель режима низкой задержки
    low_latency_rect_ = RECT{45, 535, 605, 565};
}

void TrayApp::TriggerRing(bool left) {
    if (left) {
        if (left_ringing_.exchange(true)) return;
    } else {
        if (right_ringing_.exchange(true)) return;
    }

    MessageBeep(MB_ICONASTERISK);
    if (control_window_) {
        InvalidateRect(control_window_, nullptr, FALSE);
    }

    if (spp_client_.IsConnected()) {
        spp_client_.SendFindMyBuds(left, true);
    }

    std::thread([this, left]() {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        if (spp_client_.IsConnected()) {
            spp_client_.SendFindMyBuds(left, false);
        }
        if (left) {
            left_ringing_.store(false);
        } else {
            right_ringing_.store(false);
        }
        if (control_window_) {
            InvalidateRect(control_window_, nullptr, FALSE);
        }
    }).detach();
}

LRESULT CALLBACK TrayApp::HostWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    TrayApp* app = reinterpret_cast<TrayApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        app = static_cast<TrayApp*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        return TRUE;
    }

    if (!app) return DefWindowProcW(hwnd, message, wparam, lparam);

    switch (message) {
    case kTrayIconMessage:
        if (LOWORD(lparam) == WM_LBUTTONUP || LOWORD(lparam) == WM_LBUTTONDBLCLK) {
            if (app->control_window_ && IsWindowVisible(app->control_window_)) {
                ShowWindow(app->control_window_, SW_HIDE);
            } else {
                app->ShowControlWindow();
            }
        } else if (LOWORD(lparam) == WM_CONTEXTMENU) {
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, kTrayOpenControl, L"Открыть панель");
            AppendMenuW(menu, MF_STRING, kTrayExit, L"Выход");
            POINT cursor{}; GetCursorPos(&cursor);
            SetForegroundWindow(hwnd);
            const int selection = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, hwnd, nullptr);
            DestroyMenu(menu);
            if (selection == kTrayOpenControl) app->ShowControlWindow();
            else if (selection == kTrayExit) DestroyWindow(hwnd);
        }
        return 0;
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
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case kTrayOpenControl: app->ShowControlWindow(); return 0;
        case kTrayExit: DestroyWindow(hwnd); return 0;
        }
        break;
    case WM_DESTROY:
        app->shutting_down_.store(true, std::memory_order_release);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT CALLBACK TrayApp::ControlWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    TrayApp* app = reinterpret_cast<TrayApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        app = static_cast<TrayApp*>(create->lpCreateParams);
        app->control_window_ = hwnd; // Принудительно присваиваем дескриптор на самом раннем этапе
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        return TRUE;
    }

    if (!app) return DefWindowProcW(hwnd, message, wparam, lparam);

    switch (message) {
    case WM_CREATE:
        app->LoadUiResources();
        app->LayoutControlWindow();
        return 0;
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
            bool is_interactive = PtInAnyRect(app->anc_card_rect_, point) ||
                                  PtInAnyRect(app->eq_card_rect_, point) ||
                                  PtInAnyRect(app->left_ring_rect_, point) ||
                                  PtInAnyRect(app->right_ring_rect_, point) ||
                                  PtInAnyRect(app->minimize_btn_rect_, point) ||
                                  PtInAnyRect(app->close_btn_rect_, point) ||
                                  PtInAnyRect(app->low_latency_rect_, point);
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

        // Гарантируем, что сетка координат разметки всегда инициализирована перед отрисовкой!
        app->LayoutControlWindow();

        const int width = client.right - client.left;
        const int height = client.bottom - client.top;

        // Полноценный double buffering на совместимом контексте устройства
        HDC mem_dc = CreateCompatibleDC(hdc);
        HBITMAP mem_bitmap = CreateCompatibleBitmap(hdc, width, height);
        HGDIOBJ old_bitmap = SelectObject(mem_dc, mem_bitmap);

        {
            // Отрисовка в закадровый контекст (mem_dc)
            Graphics graphics(mem_dc);
            graphics.SetSmoothingMode(SmoothingModeAntiAlias);
            graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            graphics.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
            
            // 1. Чёрный холст
            SolidBrush canvas_brush(kCanvas);
            graphics.FillRectangle(&canvas_brush, 0.0f, 0.0f, static_cast<REAL>(width), static_cast<REAL>(height));

            // Рисуем кнопки заголовка (Minimize и Close)
            // Draw Custom Minimize Button
            {
                Pen btn_pen(kTextWhite, 1.5f);
                REAL y_center = static_cast<REAL>((app->minimize_btn_rect_.top + app->minimize_btn_rect_.bottom) / 2);
                graphics.DrawLine(&btn_pen, 
                                  static_cast<REAL>(app->minimize_btn_rect_.left + 5), 
                                  y_center, 
                                  static_cast<REAL>(app->minimize_btn_rect_.right - 5), 
                                  y_center);
            }

            // Draw Custom Close Button
            {
                Pen btn_pen(kTextWhite, 1.5f);
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

            // Локальный GDI+ хелпер для рисования строк (препятствует наложению шрифтов)
            auto draw_string = [&](const std::wstring& text, Gdiplus::Font* font, const RECT& rect, const Color& color, StringAlignment align_h = StringAlignmentCenter, StringAlignment align_v = StringAlignmentCenter) {
                SolidBrush brush(color);
                StringFormat format;
                format.SetAlignment(align_h);
                format.SetLineAlignment(align_v);
                RectF rect_f(static_cast<REAL>(rect.left), static_cast<REAL>(rect.top),
                             static_cast<REAL>(rect.right - rect.left), static_cast<REAL>(rect.bottom - rect.top));
                graphics.DrawString(text.c_str(), -1, font, rect_f, &format, &brush);
            };

            // 2. РЕНДЕРИНГ ЗАГОЛОВКА по центру (шрифт ndot_55)
            std::wstring device_name = snapshot_copy.device_name.empty() ? L"Nothing Earbuds" : snapshot_copy.device_name;
            RECT title_rect{0, 24, 650, 70};
            draw_string(device_name, app->font_heading_, title_rect, kTextWhite);

            // 3. ОТРИСОВКА НАУШНИКОВ (динамический выбор 1-столбцового или 3-столбцового макета)
            if (app->image_duo_bud_) {
                // 1-столбцовый макет для спаренных наушников (центрируем по X=225)
                graphics.DrawImage(app->image_duo_bud_, 225.0f, 90.0f, 200.0f, 150.0f);
            } else {
                // 3-столбцовый макет (Левый X=100, Кейс X=275, Правый X=450)
                if (app->image_left_bud_) {
                    graphics.DrawImage(app->image_left_bud_, 100.0f, 90.0f, 100.0f, 150.0f);
                } else {
                    DrawVectorEarbud(graphics, 100, 90, false);
                }

                if (app->image_case_bud_) {
                    graphics.DrawImage(app->image_case_bud_, 250.0f, 90.0f, 150.0f, 150.0f);
                }

                if (app->image_right_bud_) {
                    graphics.DrawImage(app->image_right_bud_, 450.0f, 90.0f, 100.0f, 150.0f);
                } else {
                    DrawVectorEarbud(graphics, 450, 90, true);
                }
            }

            // Заряды аккумуляторов под каждым наушником в соответствующей колонке
            RECT left_charge_rect{50, 235, 250, 255};
            RECT case_charge_rect{225, 235, 425, 255};
            RECT right_charge_rect{400, 235, 600, 255};

            draw_string(L"L: " + FormatBatteryReading(snapshot_copy.left), app->font_body_, left_charge_rect, kTextWhite);
            if (snapshot_copy.case_battery.present) {
                draw_string(L"Case: " + FormatBatteryReading(snapshot_copy.case_battery), app->font_body_, case_charge_rect, kTextWhite);
            }
            draw_string(L"R: " + FormatBatteryReading(snapshot_copy.right), app->font_body_, right_charge_rect, kTextWhite);

            // Интерактивные бузеры поиска (Ring) с индикацией
            if (app->left_ringing_.load()) {
                FillRoundedRect(graphics, app->left_ring_rect_, 14.0f, Color(255, 0xD0, 0x25, 0x25), Color(255, 255, 110, 110));
                draw_string(L"Ringing...", app->font_button_, app->left_ring_rect_, kTextWhite);
            } else {
                FillRoundedRect(graphics, app->left_ring_rect_, 14.0f, kDarkRedFill, kDarkRedStroke);
                draw_string(L"Ring", app->font_button_, app->left_ring_rect_, Color(255, 255, 110, 110));
            }

            if (app->right_ringing_.load()) {
                FillRoundedRect(graphics, app->right_ring_rect_, 14.0f, Color(255, 0xD0, 0x25, 0x25), Color(255, 255, 110, 110));
                draw_string(L"Ringing...", app->font_button_, app->right_ring_rect_, kTextWhite);
            } else {
                FillRoundedRect(graphics, app->right_ring_rect_, 14.0f, kDarkRedFill, kDarkRedStroke);
                draw_string(L"Ring", app->font_button_, app->right_ring_rect_, Color(255, 255, 110, 110));
            }

            // 4. ЛЕВАЯ КАРТОЧКА: NOISE CONTROL
            FillRoundedRect(graphics, app->anc_card_rect_, 16.0f, kCardBg, kCardBorder);
            RECT noise_control_title_rect{55, 335, 300, 355};
            draw_string(L"NOISE CONTROL", app->font_button_, noise_control_title_rect, kTextWhite, StringAlignmentNear, StringAlignmentCenter);

            // Сегменты ANC: Noise, Transparency, Off
            const std::array<std::wstring, 3> anc_labels = {L"NOISE", L"TRANS", L"OFF"};
            const std::array<bool, 3> anc_active = {
                app->selected_anc_ == AncMode::High || app->selected_anc_ == AncMode::Low || app->selected_anc_ == AncMode::Mid || app->selected_anc_ == AncMode::Adaptive,
                app->selected_anc_ == AncMode::Transparency,
                app->selected_anc_ == AncMode::Off,
            };
            for (size_t i = 0; i < app->anc_top_rects_.size(); ++i) {
                FillRoundedRect(graphics, app->anc_top_rects_[i], 10.0f, anc_active[i] ? Color(255, 0x2A, 0x2A, 0x30) : Color(255, 0x18, 0x18, 0x1A), kCardBorder);
                draw_string(anc_labels[i], app->font_small_, app->anc_top_rects_[i], anc_active[i] ? Color(255, 255, 255, 255) : Color(255, 130, 130, 135));
            }

            bool is_anc_active = (app->selected_anc_ == AncMode::High || 
                                  app->selected_anc_ == AncMode::Mid || 
                                  app->selected_anc_ == AncMode::Low || 
                                  app->selected_anc_ == AncMode::Adaptive);
            if (is_anc_active) {
                // Ползунок Dot-селектора режимов шумоподавления с текстовыми подписями уровней
                const std::array<AncMode, 4> anc_modes = {AncMode::High, AncMode::Mid, AncMode::Low, AncMode::Adaptive};
                const std::array<std::wstring, 4> anc_dot_labels = {L"HIGH", L"MID", L"LOW", L"ADAPTIVE"};
                const int line_y = 430;
                Pen slider_line_pen(Color(255, 0x3A, 0x3A, 0x3F), 2.0f);
                graphics.DrawLine(&slider_line_pen, 110.0f, static_cast<REAL>(line_y), 245.0f, static_cast<REAL>(line_y));
                
                for (size_t i = 0; i < app->anc_mode_rects_.size(); ++i) {
                    const bool active = app->selected_anc_ == anc_modes[i];
                    SolidBrush dot_brush(active ? kDotActive : Color(255, 0x4A, 0x4A, 0x4F));
                    graphics.FillEllipse(&dot_brush, static_cast<REAL>(app->anc_mode_rects_[i].left), static_cast<REAL>(app->anc_mode_rects_[i].top), 14.0f, 14.0f);

                    RECT label_rect = RECT{app->anc_mode_rects_[i].left - 30, 448, app->anc_mode_rects_[i].right + 30, 468};
                    draw_string(anc_dot_labels[i], app->font_small_, label_rect, active ? kTextWhite : kTextMuted);
                }

                // Переключатель "Personalised ANC"
                FillRoundedRect(graphics, app->anc_toggle_rect_, 10.0f, app->personalized_anc_ ? Color(255, 0x2A, 0x2A, 0x30) : Color(255, 0x18, 0x18, 0x1A), kCardBorder);
                draw_string(L"Personalised ANC", app->font_small_, app->anc_toggle_rect_, app->personalized_anc_ ? Color(255, 255, 255, 255) : Color(255, 130, 130, 135));
            }

            // 5. ПРАВАЯ КАРТОЧКА: EQUALIZER
            FillRoundedRect(graphics, app->eq_card_rect_, 16.0f, kCardBg, kCardBorder);
            RECT equalizer_title_rect{355, 335, 590, 355};
            draw_string(L"EQUALIZER", app->font_button_, equalizer_title_rect, kTextWhite, StringAlignmentNear, StringAlignmentCenter);

            // Пресеты Balanced, More Bass, More Treble, Voice
            const std::array<EqPreset, 4> presets = {EqPreset::Balanced, EqPreset::MoreBass, EqPreset::MoreTreble, EqPreset::Voice};
            const std::array<std::wstring, 4> preset_labels = {L"Balanced", L"More Bass", L"More Treble", L"Voice"};
            for (size_t i = 0; i < app->eq_preset_rects_.size(); ++i) {
                const bool active = app->selected_eq_ == presets[i];
                FillRoundedRect(graphics, app->eq_preset_rects_[i], 10.0f, active ? Color(255, 0x2A, 0x2A, 0x30) : Color(255, 0x18, 0x18, 0x1A), kCardBorder);
                draw_string(preset_labels[i], app->font_small_, app->eq_preset_rects_[i], active ? Color(255, 255, 255, 255) : Color(255, 130, 130, 135));
            }

            // Кастомная плашка "Custom Equalizer" -> Bass Enhance Card
            if (app->bass_enabled_) {
                FillRoundedRect(graphics, app->eq_custom_rect_, 10.0f, Color(255, 0x2A, 0x2A, 0x30), Color(255, 255, 255, 255));
                std::wstring bass_text = L"Bass Enhance: Level " + std::to_wstring(app->bass_level_);
                draw_string(bass_text, app->font_small_, app->eq_custom_rect_, Color(255, 255, 255, 255));
            } else {
                FillRoundedRect(graphics, app->eq_custom_rect_, 10.0f, Color(255, 0x18, 0x18, 0x1A), kCardBorder);
                draw_string(L"Bass Enhance: Off", app->font_small_, app->eq_custom_rect_, Color(255, 130, 130, 135));
            }

            // 6. BOTTOM LOW LATENCY TOGGLE PILL
            if (app->low_latency_enabled_) {
                FillRoundedRect(graphics, app->low_latency_rect_, 15.0f, Color(255, 0xEE, 0xEE, 0xF0), Color(255, 0xFF, 0xFF, 0xFF));
                draw_string(L"LOW LATENCY MODE: ON", app->font_button_, app->low_latency_rect_, Color(255, 0x00, 0x00, 0x00));
            } else {
                FillRoundedRect(graphics, app->low_latency_rect_, 15.0f, Color(255, 0x14, 0x14, 0x16), kCardBorder);
                draw_string(L"LOW LATENCY MODE: OFF", app->font_button_, app->low_latency_rect_, kTextMuted);
            }
        }

        // Копируем готовый кадр из памяти на экран в один миг
        BitBlt(hdc, 0, 0, width, height, mem_dc, 0, 0, SRCCOPY);

        // Освобождаем ресурсы
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

        // Бузеры поиска ушей
        if (PtInAnyRect(app->left_ring_rect_, point)) {
            app->TriggerRing(true);
            return 0;
        }
        if (PtInAnyRect(app->right_ring_rect_, point)) {
            app->TriggerRing(false);
            return 0;
        }

        // Режимы ANC
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

        // Индивидуальные точки выбора ANC и Personalised ANC
        bool is_anc_active = (app->selected_anc_ == AncMode::High || 
                              app->selected_anc_ == AncMode::Mid || 
                              app->selected_anc_ == AncMode::Low || 
                              app->selected_anc_ == AncMode::Adaptive);
        if (is_anc_active) {
            const std::array<AncMode, 4> anc_modes = {AncMode::High, AncMode::Mid, AncMode::Low, AncMode::Adaptive};
            for (size_t i = 0; i < app->anc_mode_rects_.size(); ++i) {
                if (PtInAnyRect(app->anc_mode_rects_[i], point)) {
                    app->selected_anc_ = anc_modes[i];
                    app->ApplySelectedAnc();
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            }

            // Переключатель "Personalised ANC"
            if (PtInAnyRect(app->anc_toggle_rect_, point)) {
                app->personalized_anc_ = !app->personalized_anc_;
                if (app->spp_client_.IsConnected()) {
                    app->spp_client_.SendPersonalizedAnc(app->personalized_anc_);
                }
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }

        // Изменение эквалайзера
        const std::array<EqPreset, 4> presets = {EqPreset::Balanced, EqPreset::MoreBass, EqPreset::MoreTreble, EqPreset::Voice};
        for (size_t i = 0; i < app->eq_preset_rects_.size(); ++i) {
            if (PtInAnyRect(app->eq_preset_rects_[i], point)) {
                app->ApplyEqPreset(presets[i]);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }

        // Усиление баса (Bass Enhance): Off -> On (Level 1) -> 2 -> 3 -> 4 -> 5 -> Off
        if (PtInAnyRect(app->eq_custom_rect_, point)) {
            if (!app->bass_enabled_) {
                app->bass_enabled_ = true;
                app->bass_level_ = 1;
            } else {
                if (app->bass_level_ < 5) {
                    app->bass_level_++;
                } else {
                    app->bass_enabled_ = false;
                    app->bass_level_ = 3;
                }
            }
            
            if (app->spp_client_.IsConnected()) {
                app->spp_client_.SendBass(app->bass_enabled_, app->bass_level_);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        // Кнопка свернуть (Minimize)
        if (PtInAnyRect(app->minimize_btn_rect_, point)) {
            ShowWindow(hwnd, SW_MINIMIZE);
            return 0;
        }

        // Кнопка закрыть (Close - прячет окно)
        if (PtInAnyRect(app->close_btn_rect_, point)) {
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }

        // Кнопка-пилюля режима низкой задержки (Low Latency Mode)
        if (PtInAnyRect(app->low_latency_rect_, point)) {
            app->low_latency_enabled_ = !app->low_latency_enabled_;
            if (app->spp_client_.IsConnected()) {
                app->spp_client_.SendLowLatency(app->low_latency_enabled_);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        return 0;
    }
    case WM_SHOWWINDOW:
        if (wparam) {
            app->QueueConnection();
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