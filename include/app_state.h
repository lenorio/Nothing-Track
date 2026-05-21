#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include <winrt/base.h>

namespace nothing_tray {

constexpr uint16_t kNothingCompanyId = 3275; // Идентификатор Nothing Technology Ltd

inline winrt::guid FastPairUuid() {
    return winrt::guid{0x0000fe2c, 0x0000, 0x1000, {0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb}};
}

inline winrt::guid SppUuid() {
    return winrt::guid{0xaeac4a03, 0xdff5, 0x498f, {0x84, 0x3a, 0x34, 0x48, 0x7c, 0xf1, 0x33, 0xeb}};
}

struct BatteryReading {
    bool present{false};
    bool charging{false};
    std::optional<uint8_t> percent{};
};

inline BatteryReading ParseBatteryByte(uint8_t value) {
    if ((value & 0x7F) == 0x7F) {
        return {};
    }

    BatteryReading reading;
    reading.present = true;
    reading.charging = (value & 0x80) != 0;
    reading.percent = static_cast<uint8_t>(value & 0x7F);
    return reading;
}

inline std::wstring FormatBatteryReading(const BatteryReading& reading) {
    if (!reading.present || !reading.percent.has_value()) {
        return L"—";
    }

    std::wstring text = std::to_wstring(*reading.percent) + L"%";
    if (reading.charging) {
        text += L" ⚡";
    }

    return text;
}

struct BatterySnapshot {
    std::wstring device_name{L"Nothing earbuds"};
    BatteryReading left{};
    BatteryReading right{};
    BatteryReading case_battery{};
    std::chrono::steady_clock::time_point last_seen{};
    bool has_data{false};
    uint64_t bluetooth_address{0}; // MAC-адрес для прямого сопряжения по SPP

    std::wstring Tooltip() const {
        if (!has_data) {
            return device_name + L"\nL: — | R: —\nCase: —";
        }

        return device_name + L"\nL: " + FormatBatteryReading(left) + L" | R: " + FormatBatteryReading(right) +
               L"\nCase: " + FormatBatteryReading(case_battery);
     }
};

inline bool operator==(const BatteryReading& lhs, const BatteryReading& rhs) {
    return lhs.present == rhs.present &&
           lhs.charging == rhs.charging &&
           lhs.percent == rhs.percent;
}

inline bool operator!=(const BatteryReading& lhs, const BatteryReading& rhs) {
    return !(lhs == rhs);
}

inline bool operator==(const BatterySnapshot& lhs, const BatterySnapshot& rhs) {
    return lhs.device_name == rhs.device_name &&
           lhs.left == rhs.left &&
           lhs.right == rhs.right &&
           lhs.case_battery == rhs.case_battery &&
           lhs.has_data == rhs.has_data &&
           lhs.bluetooth_address == rhs.bluetooth_address;
}

inline bool operator!=(const BatterySnapshot& lhs, const BatterySnapshot& rhs) {
    return !(lhs == rhs);
}

enum class AncMode : uint8_t {
    Transparency = 0x05,
    Off = 0x07,
    High = 0x03,
    Low = 0x01,
    Mid = 0x02,
    Adaptive = 0x04,
};

struct SppStateUpdate {
    enum class Type {
        Battery,
        AncMode,
        EqPreset,
        BassState,
        FirmwareVersion,
        DeviceModel,
        LowLatency,
    };

    Type type;
    BatteryReading left{};
    BatteryReading right{};
    BatteryReading case_battery{};
    AncMode anc_mode{AncMode::Off};
    uint8_t eq_preset{0};
    bool bass_enabled{false};
    uint8_t bass_level{0};
    bool low_latency_enabled{false};
    std::wstring text_value{};
};

using SppUpdateCallback = std::function<void(const SppStateUpdate& update)>;

} // namespace nothing_tray