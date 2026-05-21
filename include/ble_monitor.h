#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "app_state.h"

namespace nothing_tray {

class BleMonitor {
public:
    using UpdateCallback = std::function<void(const BatterySnapshot&)>;

    BleMonitor() = default;
    ~BleMonitor();

    BleMonitor(const BleMonitor&) = delete;
    BleMonitor& operator=(const BleMonitor&) = delete;

    void Start(UpdateCallback callback);
    void Stop();
    void SetDeviceHint(std::wstring device_hint);

private:
    void Run();
    void PublishSnapshot(const BatterySnapshot& snapshot);

    std::thread worker_{};
    std::atomic_bool stop_requested_{false};
    std::mutex snapshot_mutex_{};
    BatterySnapshot snapshot_{};
    UpdateCallback callback_{};
    std::wstring device_hint_{};
};

} // namespace nothing_tray
