#include "ble_monitor.h"

#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>

#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

using namespace winrt;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Storage::Streams;

namespace nothing_tray {
namespace {
constexpr auto kExpireAfter = std::chrono::seconds(45);
constexpr auto kPollInterval = std::chrono::seconds(5);

std::vector<uint8_t> ReadBufferBytes(const IBuffer& buffer) {
    DataReader reader = DataReader::FromBuffer(buffer);
    std::vector<uint8_t> bytes(reader.UnconsumedBufferLength());
    if (!bytes.empty()) {
        reader.ReadBytes(winrt::array_view<uint8_t>(bytes.data(), bytes.size()));
    }
    return bytes;
}
} // namespace

BleMonitor::~BleMonitor() {
    Stop();
}

void BleMonitor::Start(UpdateCallback callback) {
    Stop();
    callback_ = std::move(callback);
    stop_requested_.store(false, std::memory_order_release);
    worker_ = std::thread([this] { Run(); });
}

void BleMonitor::Stop() {
    stop_requested_.store(true, std::memory_order_release);
    if (worker_.joinable()) {
        worker_.join();
    }
}

void BleMonitor::SetDeviceHint(std::wstring device_hint) {
    std::scoped_lock lock(snapshot_mutex_);
    device_hint_ = std::move(device_hint);
}

void BleMonitor::PublishSnapshot(const BatterySnapshot& snapshot) {
    if (callback_) {
        callback_(snapshot);
    }
}

void BleMonitor::Run() {
    init_apartment(apartment_type::multi_threaded);

    BluetoothLEAdvertisementWatcher watcher;
    watcher.ScanningMode(BluetoothLEScanningMode::Passive);

    watcher.Received([this](const BluetoothLEAdvertisementWatcher&, const BluetoothLEAdvertisementReceivedEventArgs& args) {
        const auto advertisement = args.Advertisement();

        bool relevant = false;
        for (const auto& manufacturer : advertisement.ManufacturerData()) {
            if (manufacturer.CompanyId() == kNothingCompanyId) {
                relevant = true;
                break;
            }
        }

        bool has_fastpair = false;
        BluetoothLEAdvertisementDataSection fastpair_section{};
        for (const auto& section : advertisement.DataSections()) {
            const auto type = section.DataType();
            if (type == 0x16) { 
                const auto buf = section.Data();
                auto bytes = ReadBufferBytes(buf);
                if (bytes.size() >= 2) {
                    uint16_t uuid16 = static_cast<uint16_t>(bytes[0]) | (static_cast<uint16_t>(bytes[1]) << 8);
                    if (uuid16 == 0xFE2C) {
                        has_fastpair = true;
                        fastpair_section = section;
                        break;
                    }
                }
            }
        }

        if (has_fastpair) {
            relevant = true;
        }

        if (!relevant) {
            return;
        }

        BatterySnapshot updated;
        {
            std::scoped_lock lock(snapshot_mutex_);
            updated = snapshot_;
        }

        // Захватываем MAC-адрес устройства из BLE-пакета!
        updated.bluetooth_address = args.BluetoothAddress();

        const auto local_name = advertisement.LocalName();
        if (!local_name.empty()) {
            updated.device_name = std::wstring(local_name.c_str());
        }

        if (has_fastpair) {
            const auto payload_buf = fastpair_section.Data();
            const auto bytes = ReadBufferBytes(payload_buf);
            if (bytes.size() >= 5) {
                updated.left = ParseBatteryByte(bytes[bytes.size() - 3]);
                updated.right = ParseBatteryByte(bytes[bytes.size() - 2]);
                updated.case_battery = ParseBatteryByte(bytes[bytes.size() - 1]);
                updated.last_seen = std::chrono::steady_clock::now();
                updated.has_data = true;
            }
        }

        if (updated.has_data) {
            {
                std::scoped_lock lock(snapshot_mutex_);
                snapshot_ = updated;
            }
            PublishSnapshot(updated);
        }
    });

    watcher.Start();

    while (!stop_requested_.load(std::memory_order_acquire)) {
        bool expired = false;
        BatterySnapshot expired_snapshot;
        {
            std::scoped_lock lock(snapshot_mutex_);
            if (snapshot_.has_data && (std::chrono::steady_clock::now() - snapshot_.last_seen > kExpireAfter)) {
                snapshot_.has_data = false;
                snapshot_.left = {};
                snapshot_.right = {};
                snapshot_.case_battery = {};
                expired = true;
                expired_snapshot = snapshot_;
            }
        }

        if (expired) {
            PublishSnapshot(expired_snapshot);
        }

        std::this_thread::sleep_for(kPollInterval);
    }

    watcher.Stop();
}

} // namespace nothing_tray