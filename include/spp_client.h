#pragma once
 
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>
 
#include "app_state.h"
 
#include <winrt/Windows.Devices.Bluetooth.Rfcomm.h>
#include <winrt/Windows.Networking.Sockets.h>
 
namespace nothing_tray {
 
class SppClient {
public:
    SppClient() = default;
    ~SppClient();
 
    SppClient(const SppClient&) = delete;
    SppClient& operator=(const SppClient&) = delete;
 
    bool Connect();
    bool ConnectToAddress(uint64_t bluetooth_address); // Прямое подключение по MAC
    void Disconnect();
    bool IsConnected() const;
    bool QueryDeviceState();
 
    void SetUpdateCallback(SppUpdateCallback callback);
    bool SendAnc(AncMode mode);
    bool SendBass(bool enabled, uint8_t level);
    bool SendEq(uint8_t preset_val);
    bool SendPersonalizedAnc(bool enabled);
    bool SendFindMyBuds(bool left, bool active);
    bool SendLowLatency(bool enabled);
 
private:
    bool EnsureConnected();
    bool SendCommand(uint16_t command_id, std::span<const uint8_t> payload);
    static std::vector<uint8_t> BuildPacket(uint16_t command_id, std::span<const uint8_t> payload, uint16_t op_id);
    static uint16_t ComputeCrc16(std::span<const uint8_t> data);
    
    void ReaderLoop();
    void ProcessIncomingPacket(uint16_t command, std::span<const uint8_t> payload);
 
    mutable std::mutex mutex_{};
    winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommDeviceService service_{nullptr};
    winrt::Windows::Networking::Sockets::StreamSocket socket_{nullptr};
    std::atomic_bool connected_{false};
    uint8_t op_counter_{0};
    
    std::thread read_thread_{};
    std::atomic_bool read_active_{false};
    SppUpdateCallback callback_{nullptr};
};
 
} // namespace nothing_tray