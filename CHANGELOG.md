# Changelog

All notable changes to **Nothing Track** will be documented in this file.

## [1.2.0] - 2026-08-12

### 🎨 UI & Design Overhaul
- **Container Card Grid**: Redesigned main window using dark container cards (`#1C1C1C` cards on `#000000` canvas) aligned with mobile design specs.
- **Aspect Ratio Image Fitting**: Implemented proportional image rendering for earbud assets (`object-fit: contain`) so images are never stretched or distorted.
- **Rounded Window Corners**: Applied 24px rounded corners via `CreateRoundRectRgn` and Windows DWM corner attributes (`DWMWCP_ROUND`).
- **Animated Splash Screen**: Added a sleek loading overlay with a 25 FPS rotating spinner arc (`#AC3C3B`) while Bluetooth connection is being established.

### 🎛 Controls & Ultra Bass
- **Smooth Ultra Bass Slider**: Replaced square blocks with a real continuous horizontal slider track and draggable white knob thumb. Added mouse drag-and-drop (`WM_MOUSEMOVE` capture) for real-time 1–5 level adjustment.
- **Cleaner ANC Card**: Removed redundant "Personalised ANC" toggle button, unifying Adaptive ANC under the 4-level chip selector.
- **Language Switcher**: Added quick UI language toggle (`[ RU / EN ]`) in header.

### 🔌 Bluetooth & SPP Protocol Fixes
- **CMF Buds 2 Protocol**: Corrected ANC command payloads (`MakeAncPayload`) and incoming packet parsing (`ProcessIncomingPacket`) for "Off" (`0x03`), "Transparency" (`0x02`), and Noise Cancellation modes.
- **Command Response Support**: Added handling for packet ID `28688` (`0x7010`), preventing ANC state from unexpectedly reverting to Noise Cancellation.
- **Bluetooth Device Name Preservation**: Locked real paired Bluetooth device names (e.g., `CMF Buds 2`, `Nothing Ear (a)`) to prevent overwrite by generic BLE advertisement strings (`Nothing earbuds`).
- **Multi-Model & Multi-Color Asset Engine**: Connected full high-res asset sets for all Nothing & CMF models and colorways (Orange, Black, White, Blue, Yellow).

### 🖥 Tray & System Integration
- **Tray Context Menu**: Added right-click context menu (`Open`, `Exit`) on system tray icon.
- **Single Instance Control**: Ensured background thread stability and prevented window teardown when opening from tray.
