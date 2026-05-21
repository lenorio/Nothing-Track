#include "tray_app.h"

#include <algorithm>
#include <array>
#include <cmath>
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
    WNDCLASSW host_class{};
    host_class.lpfnWndProc = HostWndProc;
    host_class.hInstance = instance_;
    host_class.lpszClassName = kHostClassName;
    host_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&host_class);

    WNDCLASSW control_class{};
    control_class.lpfnWndProc = ControlWndProc;
    control_class.hInstance = instance_;
    control_class.lpszClassName = kControlClassName;
    control_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    control_class.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    RegisterClassW(&control_class);
}

void TrayApp::CreateHostWindow() {
    host_window_ = CreateWindowExW(0, kHostClassName, kTrayTitle, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance_, this);
}

void TrayApp::CreateControlWindow() {
    if (control_window_) return;

    // Убран флаг WS_EX_TOPMOST, окно больше не липнет поверх других окон!
    control_window_ = CreateWindowExW(WS_EX_TOOLWINDOW, kControlClassName, L"Nothing / CMF Controls",
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
    {
        std::scoped_lock lock(state_mutex_);
        snapshot_ = snapshot;
    }
    PostUiMessage(kBleSnapshotMessage);
}

void TrayApp::OnBleSnapshot(const BatterySnapshot& snapshot) {
    ApplySnapshot(snapshot);
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

void TrayApp::RefreshControlWindow() {
    if (!control_window_) return;
    InvalidateRect(control_window_, nullptr, TRUE);
}

void TrayApp::UpdateControlStatusText() {
    if (control_window_) InvalidateRect(control_window_, nullptr, TRUE);
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
    spp_client_.SendAnc(SelectedAncMode());
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
    switch (preset) {
    case EqPreset::Balanced:
        bass_enabled_ = true;
        bass_level_ = 3;
        break;
    case EqPreset::MoreBass:
        bass_enabled_ = true;
        bass_level_ = 5;
        break;
    case EqPreset::MoreTreble:
        bass_enabled_ = true;
        bass_level_ = 1;
        break;
    case EqPreset::Voice:
        bass_enabled_ = false;
        bass_level_ = 2;
        break;
    }
    ApplySelectedBass();
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

void TrayApp::LoadUiResources() {
    if (font_heading_) return;

    const std::filesystem::path workspace = FindWorkspaceRoot();
    workspace_root_ = workspace.wstring();
    font_root_ = (workspace / L"fonts").wstring();
    asset_root_ = (workspace / L"assets").wstring();

    const std::wstring heading_path = (std::filesystem::path(font_root_) / L"ndot_55.otf").wstring();
    const std::wstring body_path = (std::filesystem::path(font_root_) / L"space_grotesk_regular.otf").wstring();
    const std::wstring button_path = (std::filesystem::path(font_root_) / L"manrope_regular.otf").wstring();

    AddFontResourceExW(heading_path.c_str(), FR_PRIVATE, nullptr);
    AddFontResourceExW(body_path.c_str(), FR_PRIVATE, nullptr);
    AddFontResourceExW(button_path.c_str(), FR_PRIVATE, nullptr);

    HDC screen = GetDC(nullptr);
    const int dpi = GetDeviceCaps(screen, LOGPIXELSY);
    ReleaseDC(nullptr, screen);

    auto make_font = [&](const wchar_t* family, int px, int weight) -> HFONT {
        const int height = -MulDiv(px, dpi, 96);
        HFONT font = CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 VARIABLE_PITCH, family);
        if (!font) {
            font = CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               VARIABLE_PITCH, L"Segoe UI");
        }
        return font;
    };

    // Загрузка фирменных шрифтов Nothing
    font_heading_ = make_font(L"ndot 55", 28, FW_NORMAL);
    font_body_ = make_font(L"Space Grotesk", 16, FW_NORMAL);
    font_button_ = make_font(L"Manrope", 14, FW_SEMIBOLD);
    font_small_ = make_font(L"Manrope", 12, FW_NORMAL);

    brush_canvas_ = CreateSolidBrush(RGB(0, 0, 0));
    brush_surface_ = CreateSolidBrush(RGB(0x14, 0x14, 0x16));
    brush_surface_elevated_ = CreateSolidBrush(RGB(0x1B, 0x1D, 0x1F));
    brush_surface_card_ = CreateSolidBrush(RGB(0x12, 0x12, 0x12));

    const std::wstring left_bud = (std::filesystem::path(asset_root_) / L"donphan_white_left.webp").wstring();
    const std::wstring right_bud = (std::filesystem::path(asset_root_) / L"donphan_white_right.wstring").wstring();

    image_left_bud_ = Image::FromFile(left_bud.c_str(), FALSE);
    if (image_left_bud_ && image_left_bud_->GetLastStatus() != Ok) {
        delete image_left_bud_;
        image_left_bud_ = nullptr;
    }
    image_right_bud_ = Image::FromFile(right_bud.c_str(), FALSE);
    if (image_right_bud_ && image_right_bud_->GetLastStatus() != Ok) {
        delete image_right_bud_;
        image_right_bud_ = nullptr;
    }
}

void TrayApp::ReleaseUiResources() {
    if (font_heading_) DeleteObject(font_heading_);
    if (font_body_) DeleteObject(font_body_);
    if (font_button_) DeleteObject(font_button_);
    if (font_small_) DeleteObject(font_small_);
    if (brush_canvas_) DeleteObject(brush_canvas_);
    if (brush_surface_) DeleteObject(brush_surface_);
    if (brush_surface_elevated_) DeleteObject(brush_surface_elevated_);
    if (brush_surface_card_) DeleteObject(brush_surface_card_);

    if (!font_root_.empty()) {
        const std::wstring heading_path = (std::filesystem::path(font_root_) / L"ndot_55.otf").wstring();
        const std::wstring body_path = (std::filesystem::path(font_root_) / L"space_grotesk_regular.otf").wstring();
        const std::wstring button_path = (std::filesystem::path(font_root_) / L"manrope_regular.otf").wstring();
        RemoveFontResourceExW(heading_path.c_str(), FR_PRIVATE, nullptr);
        RemoveFontResourceExW(body_path.c_str(), FR_PRIVATE, nullptr);
        RemoveFontResourceExW(button_path.c_str(), FR_PRIVATE, nullptr);
    }

    if (image_left_bud_) delete image_left_bud_;
    if (image_right_bud_) delete image_right_bud_;
}

void TrayApp::LayoutControlWindow() {
    if (!control_window_) return;

    // СТРОГАЯ СЕТКА КООРДИНАТ КАРТОЧЕК
    header_rect_ = RECT{0, 0, 650, 80};
    device_area_rect_ = RECT{0, 80, 650, 320};
    
    left_bud_rect_ = RECT{120, 100, 220, 250};
    right_bud_rect_ = RECT{400, 100, 500, 250};
    
    left_ring_rect_ = RECT{120, 260, 220, 300};
    right_ring_rect_ = RECT{400, 260, 500, 300};
    
    // Левая карточка (NOISE CONTROL): X=40, Y=320 to X=300, Y=530
    anc_card_rect_ = RECT{40, 320, 300, 530};
    
    // Правая карточка (EQUALIZER): X=340, Y=320 to X=600, Y=530
    eq_card_rect_ = RECT{340, 320, 600, 530};
    
    // Кнопки ANC режимов
    const int anc_button_y = 370;
    const int anc_button_width = 75;
    const int anc_button_spacing = 6;
    anc_top_rects_[0] = RECT{50, anc_button_y, 50 + anc_button_width, anc_button_y + 35};
    anc_top_rects_[1] = RECT{50 + anc_button_width + anc_button_spacing, anc_button_y, 50 + (anc_button_width * 2) + anc_button_spacing, anc_button_y + 35};
    anc_top_rects_[2] = RECT{50 + (anc_button_width * 2) + (anc_button_spacing * 2), anc_button_y, 290, anc_button_y + 35};
    
    // Ползунок степени шумоподавления (Dots)
    const int anc_line_y = 430;
    anc_mode_rects_[0] = RECT{70, anc_line_y - 6, 85, anc_line_y + 9};
    anc_mode_rects_[1] = RECT{120, anc_line_y - 6, 135, anc_line_y + 9};
    anc_mode_rects_[2] = RECT{170, anc_line_y - 6, 185, anc_line_y + 9};
    anc_mode_rects_[3] = RECT{220, anc_line_y - 6, 235, anc_line_y + 9};
    anc_toggle_rect_ = RECT{50, 475, 290, 515};
    
    // Кнопки эквалайзера 2х2
    const int eq_button_y = 370;
    const int eq_button_width = 110;
    const int eq_button_height = 40;
    const int eq_button_spacing = 8;
    eq_preset_rects_[0] = RECT{355, eq_button_y, 355 + eq_button_width, eq_button_y + eq_button_height};
    eq_preset_rects_[1] = RECT{355 + eq_button_width + eq_button_spacing, eq_button_y, 585, eq_button_y + eq_button_height};
    eq_preset_rects_[2] = RECT{355, eq_button_y + eq_button_height + eq_button_spacing, 355 + eq_button_width, eq_button_y + (eq_button_height * 2) + eq_button_spacing};
    eq_preset_rects_[3] = RECT{355 + eq_button_width + eq_button_spacing, eq_button_y + eq_button_height + eq_button_spacing, 585, eq_button_y + (eq_button_height * 2) + eq_button_spacing};
    eq_custom_rect_ = RECT{355, 475, 585, 515};
}

void TrayApp::TriggerRing() {
    MessageBeep(MB_ICONASTERISK);
    if (spp_client_.IsConnected()) {
        // Сигнал поиска на левый наушник
        spp_client_.SendBass(true, 5); 
    }
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
        if (LOWORD(lparam) == WM_LBUTTONDBLCLK || LOWORD(lparam) == WM_RBUTTONUP) {
            app->ShowControlWindow();
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
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_NCHITTEST: {
        const LRESULT hit = DefWindowProcW(hwnd, message, wparam, lparam);
        if (hit == HTCLIENT) {
            POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            ScreenToClient(hwnd, &point);
            if (PtInAnyRect(app->anc_card_rect_, point) || PtInAnyRect(app->eq_card_rect_, point) ||
                PtInAnyRect(app->left_ring_rect_, point) || PtInAnyRect(app->right_ring_rect_, point)) {
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

        // Внутренний буфер GDI+ для гладкой отрисовки без мерцаний
        Graphics graphics(hdc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        graphics.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        
        // 1. Чёрный холст
        SolidBrush canvas_brush(kCanvas);
        graphics.FillRectangle(&canvas_brush, 0.0f, 0.0f, static_cast<REAL>(client.right), static_cast<REAL>(client.bottom));

        BatterySnapshot snapshot_copy;
        {
            std::scoped_lock lock(app->state_mutex_);
            snapshot_copy = app->snapshot_;
        }

        // 2. РЕНДЕРИНГ ЗАГОЛОВКА по центру (шрифт ndot_55)
        std::wstring device_name = snapshot_copy.device_name.empty() ? L"CMF Buds 2" : snapshot_copy.device_name;
        DrawCenteredText(hdc, app->font_heading_, device_name, RECT{0, 24, 650, 70}, RGB(0xF4, 0xF4, 0xF6));

        // 3. ОТРИСОВКА НАУШНИКОВ (если картинок WebP нет — рендерим идеальный вектор)
        if (app->image_left_bud_) {
            graphics.DrawImage(app->image_left_bud_, 120.0f, 100.0f, 100.0f, 150.0f);
        } else {
            DrawVectorEarbud(graphics, 120, 100, false);
        }
        
        if (app->image_right_bud_) {
            graphics.DrawImage(app->image_right_bud_, 400.0f, 100.0f, 100.0f, 150.0f);
        } else {
            DrawVectorEarbud(graphics, 400, 100, true);
        }

        // Заряды аккумуляторов под наушниками
        std::wstring left_charge = snapshot_copy.left.present && snapshot_copy.left.percent.has_value()
            ? std::to_wstring(*snapshot_copy.left.percent) + L"% L" : L"— L";
        std::wstring right_charge = snapshot_copy.right.present && snapshot_copy.right.percent.has_value()
            ? std::to_wstring(*snapshot_copy.right.percent) + L"% R" : L"— R";
            
        DrawCenteredText(hdc, app->font_body_, left_charge, RECT{120, 222, 220, 245}, RGB(0xF4, 0xF4, 0xF6));
        DrawCenteredText(hdc, app->font_body_, right_charge, RECT{400, 222, 500, 245}, RGB(0xF4, 0xF4, 0xF6));

        // Силуэт кнопки "Ring" (Тёмно-красный пилл)
        FillRoundedRect(graphics, app->left_ring_rect_, 14.0f, kDarkRedFill, kDarkRedStroke);
        DrawCenteredText(hdc, app->font_button_, L"Ring", app->left_ring_rect_, RGB(255, 110, 110));

        FillRoundedRect(graphics, app->right_ring_rect_, 14.0f, kDarkRedFill, kDarkRedStroke);
        DrawCenteredText(hdc, app->font_button_, L"Ring", app->right_ring_rect_, RGB(255, 110, 110));

        // 4. ЛЕВАЯ КАРТОЧКА: NOISE CONTROL
        FillRoundedRect(graphics, app->anc_card_rect_, 16.0f, kCardBg, kCardBorder);
        DrawTextLine(hdc, app->font_button_, L"NOISE CONTROL", 60, 335, RGB(0xF4, 0xF4, 0xF6));

        // Сегменты ANC: Noise, Transparency, Off
        const std::array<std::wstring, 3> anc_labels = {L"Noise", L"Trans", L"Off"};
        const std::array<bool, 3> anc_active = {
            app->selected_anc_ == AncMode::High || app->selected_anc_ == AncMode::Low || app->selected_anc_ == AncMode::Mid || app->selected_anc_ == AncMode::Adaptive,
            app->selected_anc_ == AncMode::Transparency,
            app->selected_anc_ == AncMode::Off,
        };
        for (size_t i = 0; i < app->anc_top_rects_.size(); ++i) {
            FillRoundedRect(graphics, app->anc_top_rects_[i], 10.0f, anc_active[i] ? Color(255, 0x2A, 0x2A, 0x30) : Color(255, 0x18, 0x18, 0x1A), kCardBorder);
            DrawCenteredText(hdc, app->font_small_, anc_labels[i], app->anc_top_rects_[i], anc_active[i] ? RGB(255, 255, 255) : RGB(130, 130, 135));
        }

        // Ползунок Dot-селектора режимов шумоподавления
        const std::array<AncMode, 4> anc_modes = {AncMode::High, AncMode::Mid, AncMode::Low, AncMode::Adaptive};
        const int line_y = 430;
        Pen slider_line_pen(Color(255, 0x3A, 0x3A, 0x3F), 2.0f);
        graphics.DrawLine(&slider_line_pen, 70.0f, static_cast<REAL>(line_y), 235.0f, static_cast<REAL>(line_y));
        
        for (size_t i = 0; i < app->anc_mode_rects_.size(); ++i) {
            const bool active = app->selected_anc_ == anc_modes[i];
            SolidBrush dot_brush(active ? kDotActive : Color(255, 0x4A, 0x4A, 0x4F));
            graphics.FillEllipse(&dot_brush, static_cast<REAL>(app->anc_mode_rects_[i].left), static_cast<REAL>(app->anc_mode_rects_[i].top), 14.0f, 14.0f);
        }

        // Переключатель "Personalised ANC"
        FillRoundedRect(graphics, app->anc_toggle_rect_, 10.0f, app->personalized_anc_ ? Color(255, 0x2A, 0x2A, 0x30) : Color(255, 0x18, 0x18, 0x1A), kCardBorder);
        DrawCenteredText(hdc, app->font_small_, L"Personalised ANC", app->anc_toggle_rect_, app->personalized_anc_ ? RGB(255, 255, 255) : RGB(130, 130, 135));

        // 5. ПРАВАЯ КАРТОЧКА: EQUALIZER
        FillRoundedRect(graphics, app->eq_card_rect_, 16.0f, kCardBg, kCardBorder);
        DrawTextLine(hdc, app->font_button_, L"EQUALIZER", 360, 335, RGB(0xF4, 0xF4, 0xF6));

        // Пресеты Balanced, More Bass, More Treble, Voice
        const std::array<EqPreset, 4> presets = {EqPreset::Balanced, EqPreset::MoreBass, EqPreset::MoreTreble, EqPreset::Voice};
        const std::array<std::wstring, 4> preset_labels = {L"Balanced", L"More Bass", L"More Treble", L"Voice"};
        for (size_t i = 0; i < app->eq_preset_rects_.size(); ++i) {
            const bool active = app->selected_eq_ == presets[i];
            FillRoundedRect(graphics, app->eq_preset_rects_[i], 10.0f, active ? Color(255, 0x2A, 0x2A, 0x30) : Color(255, 0x18, 0x18, 0x1A), kCardBorder);
            DrawCenteredText(hdc, app->font_small_, preset_labels[i], app->eq_preset_rects_[i], active ? RGB(255, 255, 255) : RGB(130, 130, 135));
        }

        // Кастомная плашка "Custom Equalizer"
        FillRoundedRect(graphics, app->eq_custom_rect_, 10.0f, Color(255, 0x18, 0x18, 0x1A), kCardBorder);
        DrawCenteredText(hdc, app->font_small_, L"Custom Profile", app->eq_custom_rect_, RGB(130, 130, 135));

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        const int x = GET_X_LPARAM(lparam);
        const int y = GET_Y_LPARAM(lparam);
        POINT point{x, y};

        // Бузеры поиска ушей
        if (PtInAnyRect(app->left_ring_rect_, point) || PtInAnyRect(app->right_ring_rect_, point)) {
            app->TriggerRing();
            return 0;
        }

        // Режимы ANC
        if (PtInAnyRect(app->anc_top_rects_[0], point)) {
            if (app->selected_anc_ != AncMode::High && app->selected_anc_ != AncMode::Low &&
                app->selected_anc_ != AncMode::Mid && app->selected_anc_ != AncMode::Adaptive) {
                app->selected_anc_ = AncMode::High;
            }
            app->ApplySelectedAnc();
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }
        if (PtInAnyRect(app->anc_top_rects_[1], point)) {
            app->selected_anc_ = AncMode::Transparency;
            app->ApplySelectedAnc();
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }
        if (PtInAnyRect(app->anc_top_rects_[2], point)) {
            app->selected_anc_ = AncMode::Off;
            app->ApplySelectedAnc();
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }

        // Индивидуальные точки выбора ANC
        const std::array<AncMode, 4> anc_modes = {AncMode::High, AncMode::Mid, AncMode::Low, AncMode::Adaptive};
        for (size_t i = 0; i < app->anc_mode_rects_.size(); ++i) {
            if (PtInAnyRect(app->anc_mode_rects_[i], point)) {
                app->selected_anc_ = anc_modes[i];
                app->ApplySelectedAnc();
                InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            }
        }

        if (PtInAnyRect(app->anc_toggle_rect_, point)) {
            app->personalized_anc_ = !app->personalized_anc_;
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }

        // Изменение эквалайзера
        const std::array<EqPreset, 4> presets = {EqPreset::Balanced, EqPreset::MoreBass, EqPreset::MoreTreble, EqPreset::Voice};
        for (size_t i = 0; i < app->eq_preset_rects_.size(); ++i) {
            if (PtInAnyRect(app->eq_preset_rects_[i], point)) {
                app->ApplyEqPreset(presets[i]);
                InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            }
        }
        return 0;
    }
    case WM_SHOWWINDOW:
        if (wparam) {
            app->QueueConnection();
        }
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wparam) == WA_INACTIVE) {
            app->DestroyControlWindow();
            return 0;
        }
        return 0;
    case WM_KILLFOCUS:
        app->DestroyControlWindow();
        return 0;
    case WM_CLOSE:
        app->DestroyControlWindow();
        return 0;
    case WM_DESTROY:
        app->ReleaseUiResources();
        return 0;
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

} // namespace nothing_tray