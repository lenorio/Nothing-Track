# Nothing Track <sup>unofficial</sup>

**Nothing Track** is a lightweight, high-performance native Windows 11/10 tray application (C++ / Win32 / GDI+) designed to manage your Nothing and CMF ecosystem earbuds seamlessly from your desktop.

---

## 🎨 Highlights & Features

- **Modern Mobile-Inspired Interface**: Dark container cards (`#1C1C1C` on `#000000` canvas) with 24px DWM rounded window corners.
- **Aspect-Ratio Preserved Assets**: High-res renders for all Nothing & CMF models and colorways (Orange, Black, White, Blue, Yellow).
- **Animated Splash Screen**: Smooth 25 FPS loading spinner (`#AC3C3B`) while connecting over Bluetooth.
- **Battery Management**: Real-time battery percentages for Left, Right, and Charging Case.
- **Noise Control (ANC)**:
  - 3 Mode Switches: **Noise**, **Transparency**, **Off**
  - 4 Segmented Submode Chips: **High**, **Mid**, **Low**, **Adaptive**
- **Ultra Bass Enhancer**: Interactive smooth horizontal slider (Levels 1–5) with real-time mouse drag & drop (`WM_MOUSEMOVE`).
- **Equalizer Presets**: Balanced, More Bass, More Treble, Voice, and **Dirac Opteo**.
- **Low Latency Mode**: One-click toggle for low latency audio sync during gaming and videos.
- **Find My Earbuds**: Left / Right earbud buzzer triggering.
- **Tray Integration & Localization**: System tray icon with right-click popup context menu (`Open`, `Exit`) and `[ RU / EN ]` UI language switcher.

---

## 🎧 Supported Devices

- **CMF Buds 2** / **CMF Buds** *(Orange, Dark Grey, White, Blue)*
- **CMF Buds Pro** / **CMF Buds Pro 2** *(Orange, Dark Grey, Light Grey)*
- **Nothing Ear (a)** *(Yellow, White, Black)*
- **Nothing Ear (open)**
- **Nothing Ear (1)** / **Nothing Ear (2)** / **Nothing Ear** *(White, Black)*
- **Nothing Ear (stick)**

---

## 🛠 Building & Requirements

### System Requirements
- Windows 10 (Build 19041+) or Windows 11
- Bluetooth Adapter (BLE & SPP RFCOMM support)

### Building from Source
Requires **Visual Studio 2022** (with C++ Desktop Workload) and **CMake**:

```bash
# Generate build files
cmake -B build -S .

# Compile Release build
cmake --build build --config Release
```

The executable will be generated at `build/Release/NothingTray.exe`.

---

## 📜 Changelog

Detailed release history and protocol updates are recorded in [CHANGELOG.md](CHANGELOG.md).

---

## 👥 Credits

- **RapidZapper** - Original concept and research on EarPC / EarWeb.
- **DerrenGoneDigital** - Original logo and asset inspiration.
- **Google Deepmind / Antigravity Team** - Pair programming & AGY Agentic development.
