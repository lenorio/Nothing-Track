#include "spp_client.h"

#include <algorithm>
#include <stdexcept>
#include <cstdio>
#include <sstream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <Windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Rfcomm.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Networking.Sockets.h>
#include <winrt/Windows.Storage.Streams.h>

using namespace winrt;
using namespace Windows::Devices::Bluetooth::Rfcomm;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Networking::Sockets;
using namespace Windows::Storage::Streams;

namespace nothing_tray {
namespace {
constexpr uint16_t kAncCommandId = 61455;
constexpr uint16_t kBassCommandId = 61521;
constexpr uint16_t kStateReadCommandId1 = 49182;
constexpr uint16_t kStateReadCommandId2 = 49232;

void AppendUInt16LE(std::vector<uint8_t>& buffer, uint16_t value) {
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

std::array<uint8_t, 3> MakeAncPayload(AncMode mode) {
    switch (mode) {
    case AncMode::Transparency:
        return {0x02, 0x00, 0x00};
    case AncMode::Off:
        return {0x03, 0x00, 0x00};
    case AncMode::High:
        return {0x01, 0x01, 0x00};
    case AncMode::Mid:
        return {0x01, 0x02, 0x00};
    case AncMode::Low:
        return {0x01, 0x03, 0x00};
    case AncMode::Adaptive:
        return {0x01, 0x04, 0x00};
    default:
        return {0x03, 0x00, 0x00};
    }
}

std::array<uint8_t, 2> MakeBassPayload(bool enabled, uint8_t level) {
    return {static_cast<uint8_t>(enabled ? 0x01 : 0x00), static_cast<uint8_t>(enabled ? level * 2 : 0x00)};
}
} // namespace

static void LogDebug(const std::string& text) {
    std::string line = text + "\n";
    OutputDebugStringA(line.c_str());

    try {
        wchar_t temp_path[MAX_PATH];
        if (GetTempPathW(MAX_PATH, temp_path) > 0) {
            std::wstring log_file = std::wstring(temp_path) + L"nothing_tray.log";
            FILE* f = _wfsopen(log_file.c_str(), L"a", _SH_DENYNO);
            if (f) {
                SYSTEMTIME st;
                GetLocalTime(&st);
                fprintf(f, "[%02d:%02d:%02d.%03d] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, text.c_str());
                fflush(f);
                fclose(f);
            }
        }
    } catch (...) {}
}

SppClient::~SppClient() {
    Disconnect();
}

bool SppClient::IsConnected() const {
    return connected_.load(std::memory_order_acquire);
}

void SppClient::Disconnect() {
    read_active_.store(false, std::memory_order_release);
    try {
        if (socket_) {
            socket_.Close();
        }
    } catch (...) {}

    if (read_thread_.joinable()) {
        read_thread_.join();
    }

    std::scoped_lock lock(mutex_);
    socket_ = nullptr;
    service_ = nullptr;
    connected_.store(false, std::memory_order_release);
}

bool SppClient::EnsureConnected() {
    return IsConnected();
}

bool SppClient::Connect() {
    if (connected_.load(std::memory_order_acquire)) {
        return true;
    }

    LogDebug("SppClient: Searching for paired Bluetooth devices...");
    
    // 1. First attempt: standard RfcommDeviceService AQS selector
    try {
        const auto selector = RfcommDeviceService::GetDeviceSelector(RfcommServiceId::FromUuid(SppUuid()));
        const auto devices = DeviceInformation::FindAllAsync(selector).get();
        if (devices.Size() > 0) {
            LogDebug("SppClient: Found device via SppUuid AQS selector");
            const auto device = devices.GetAt(0);
            auto tmp_service = RfcommDeviceService::FromIdAsync(device.Id()).get();
            if (tmp_service) {
                auto tmp_socket = StreamSocket();
                tmp_socket.Control().KeepAlive(true);
                tmp_socket.ConnectAsync(
                    tmp_service.ConnectionHostName(),
                    tmp_service.ConnectionServiceName(),
                    SocketProtectionLevel::BluetoothEncryptionAllowNullAuthentication).get();

                {
                    std::scoped_lock lock(mutex_);
                    service_ = tmp_service;
                    socket_ = tmp_socket;
                    device_name_ = device.Name().c_str();
                    connected_.store(true, std::memory_order_release);
                }
                LogDebug("SppClient: Connection established via AQS!");

                read_active_.store(true, std::memory_order_release);
                read_thread_ = std::thread([this] { ReaderLoop(); });
                return true;
            }
        }
    } catch (const winrt::hresult_error& e) {
        LogDebug("SppClient: AQS selector connect attempt failed: " + winrt::to_string(e.message()) + ". Trying paired devices enumeration...");
    } catch (const std::exception& e) {
        LogDebug("SppClient: AQS selector error: " + std::string(e.what()));
    } catch (...) {
        LogDebug("SppClient: AQS selector unknown error");
    }

    // 2. Second attempt: Enumerate all paired Bluetooth devices on Windows
    try {
        LogDebug("SppClient: Enumerating all paired Bluetooth devices...");
        auto btSelector = Windows::Devices::Bluetooth::BluetoothDevice::GetDeviceSelectorFromPairingState(true);
        auto pairedDevices = DeviceInformation::FindAllAsync(btSelector).get();
        LogDebug("SppClient: Found " + std::to_string(pairedDevices.Size()) + " paired Bluetooth devices.");

        for (uint32_t i = 0; i < pairedDevices.Size(); ++i) {
            auto devInfo = pairedDevices.GetAt(i);
            std::string name = winrt::to_string(devInfo.Name());

            std::string lower_name = name;
            std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
            if (lower_name.find("nothing") == std::string::npos &&
                lower_name.find("ear") == std::string::npos &&
                lower_name.find("cmf") == std::string::npos &&
                lower_name.find("buds") == std::string::npos &&
                lower_name.find("headphone") == std::string::npos) {
                LogDebug("SppClient: Skipping non-Nothing device [" + std::to_string(i) + "]: " + name);
                continue;
            }

            LogDebug("SppClient: Checking paired device [" + std::to_string(i) + "]: " + name);

            try {
                auto btDevice = Windows::Devices::Bluetooth::BluetoothDevice::FromIdAsync(devInfo.Id()).get();
                if (!btDevice) continue;

                // Try SppUuid first with Uncached mode to force SDP record retrieval
                auto rfResult = btDevice.GetRfcommServicesForIdAsync(
                    RfcommServiceId::FromUuid(SppUuid()),
                    Windows::Devices::Bluetooth::BluetoothCacheMode::Uncached
                ).get();

                if (rfResult.Services().Size() == 0) {
                    // Try standard SerialPort UUID as fallback
                    rfResult = btDevice.GetRfcommServicesForIdAsync(
                        RfcommServiceId::SerialPort(),
                        Windows::Devices::Bluetooth::BluetoothCacheMode::Uncached
                    ).get();
                }

                if (rfResult.Services().Size() > 0) {
                    LogDebug("SppClient: Found SPP RFCOMM service on device: " + name);
                    auto tmp_service = rfResult.Services().GetAt(0);
                    auto tmp_socket = StreamSocket();
                    tmp_socket.Control().KeepAlive(true);
                    tmp_socket.ConnectAsync(
                        tmp_service.ConnectionHostName(),
                        tmp_service.ConnectionServiceName(),
                        SocketProtectionLevel::BluetoothEncryptionAllowNullAuthentication).get();

                    {
                        std::scoped_lock lock(mutex_);
                        service_ = tmp_service;
                        socket_ = tmp_socket;
                        device_name_ = devInfo.Name().c_str();
                        connected_.store(true, std::memory_order_release);
                    }
                    LogDebug("SppClient: Successfully connected to " + name + " via SPP!");

                    read_active_.store(true, std::memory_order_release);
                    read_thread_ = std::thread([this] { ReaderLoop(); });
                    return true;
                }
            } catch (const winrt::hresult_error& e) {
                LogDebug("SppClient: RFCOMM check failed for " + name + ": " + winrt::to_string(e.message()));
            } catch (const std::exception& e) {
                LogDebug("SppClient: Exception for " + name + ": " + std::string(e.what()));
            } catch (...) {}
        }
    } catch (const winrt::hresult_error& e) {
        LogDebug("SppClient: Enumeration failed winrt error: " + winrt::to_string(e.message()));
    } catch (const std::exception& e) {
        LogDebug("SppClient: Enumeration failed std::exception: " + std::string(e.what()));
    } catch (...) {}

    return false;
}

bool SppClient::ConnectToAddress(uint64_t bluetooth_address) {
    if (connected_.load(std::memory_order_acquire)) return true;
    
    try {
        LogDebug("SppClient: Direct RFCOMM Connection to MAC: " + std::to_string(bluetooth_address));
        auto btDevice = Windows::Devices::Bluetooth::BluetoothDevice::FromBluetoothAddressAsync(bluetooth_address).get();
        if (!btDevice) {
            LogDebug("SppClient: Direct device resolution failed for MAC " + std::to_string(bluetooth_address));
            return false;
        }

        // Try SppUuid with Uncached
        auto rf = btDevice.GetRfcommServicesForIdAsync(
            RfcommServiceId::FromUuid(SppUuid()),
            Windows::Devices::Bluetooth::BluetoothCacheMode::Uncached
        ).get();

        if (rf.Services().Size() == 0) {
            // Fallback to Cached
            rf = btDevice.GetRfcommServicesForIdAsync(
                RfcommServiceId::FromUuid(SppUuid()),
                Windows::Devices::Bluetooth::BluetoothCacheMode::Cached
            ).get();
        }

        if (rf.Services().Size() == 0) {
            // Fallback to SerialPort UUID
            rf = btDevice.GetRfcommServicesForIdAsync(
                RfcommServiceId::SerialPort(),
                Windows::Devices::Bluetooth::BluetoothCacheMode::Uncached
            ).get();
        }

        if (rf.Services().Size() == 0) {
            LogDebug("SppClient: Target MAC does not publish SPP profile");
            return false;
        }

        auto tmp_service = rf.Services().GetAt(0);
        auto tmp_socket = StreamSocket();
        tmp_socket.Control().KeepAlive(true);

        tmp_socket.ConnectAsync(
            tmp_service.ConnectionHostName(),
            tmp_service.ConnectionServiceName(),
            SocketProtectionLevel::BluetoothEncryptionAllowNullAuthentication).get();

        {
            std::scoped_lock lock(mutex_);
            service_ = tmp_service;
            socket_ = tmp_socket;
            connected_.store(true, std::memory_order_release);
        }
        LogDebug("SppClient: Direct MAC connected successfully!");

        read_active_.store(true, std::memory_order_release);
        read_thread_ = std::thread([this] { ReaderLoop(); });
        return true;
    } catch (const winrt::hresult_error& e) {
        LogDebug("SppClient: MAC StreamSocket connect failed: " + winrt::to_string(e.message()));
        return false;
    }
}

bool SppClient::QueryDeviceState() {
    if (!IsConnected()) return false;

    LogDebug("SppClient: Querying all device states...");
    bool ok = true;
    ok &= SendCommand(49158, {}); // Serial number (0xC006)
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ok &= SendCommand(49159, {}); // Battery
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ok &= SendCommand(49182, {}); // ANC Status
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ok &= SendCommand(49183, {}); // EQ Preset
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ok &= SendCommand(49230, {}); // Enhanced Bass Status
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ok &= SendCommand(49218, {}); // Firmware
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ok &= SendCommand(49217, {}); // Low Latency Status
    return ok;
}

uint16_t SppClient::ComputeCrc16(std::span<const uint8_t> data) {
    uint16_t crc = 0xFFFF;
    for (const uint8_t byte : data) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1) ? static_cast<uint16_t>((crc >> 1) ^ 0xA001) : static_cast<uint16_t>(crc >> 1);
        }
    }
    return crc;
}

std::vector<uint8_t> SppClient::BuildPacket(uint16_t command_id, std::span<const uint8_t> payload, uint16_t op_id) {
    std::vector<uint8_t> packet;
    packet.reserve(3 + 2 + 1 + 2 + payload.size() + 2);
    packet.push_back(0x55);
    packet.push_back(0x60);
    packet.push_back(0x01);
    AppendUInt16LE(packet, command_id);
    packet.push_back(static_cast<uint8_t>(payload.size()));
    packet.push_back(0x00);
    packet.push_back(static_cast<uint8_t>(op_id & 0xFF));
    packet.insert(packet.end(), payload.begin(), payload.end());

    const uint16_t crc = ComputeCrc16(packet);
    AppendUInt16LE(packet, crc);
    return packet;
}

bool SppClient::SendCommand(uint16_t command_id, std::span<const uint8_t> payload) {
    std::scoped_lock lock(mutex_);
    if (!socket_) {
        LogDebug("SppClient: Command ignored, socket offline");
        return false;
    }

    const uint16_t op_id_raw = op_counter_++;
    const uint8_t op_id = static_cast<uint8_t>((op_id_raw % 250) + 1);
    const std::vector<uint8_t> packet = BuildPacket(command_id, payload, op_id);

    try {
        DataWriter writer(socket_.OutputStream());
        writer.WriteBytes(winrt::array_view<uint8_t const>(packet.data(), packet.size()));
        writer.StoreAsync().get();
        writer.FlushAsync().get();
        writer.DetachStream();
        return true;
    } catch (const winrt::hresult_error& e) {
        LogDebug("SppClient: Outbound payload transmission failed: " + winrt::to_string(e.message()));
        connected_.store(false, std::memory_order_release);
    }
    return false;
}

bool SppClient::SendAnc(AncMode mode) {
    const auto payload = MakeAncPayload(mode);
    return SendCommand(kAncCommandId, payload);
}

bool SppClient::SendBass(bool enabled, uint8_t level) {
    level = std::clamp<uint8_t>(level, 1, 5);
    const auto payload = MakeBassPayload(enabled, level);
    return SendCommand(kBassCommandId, payload);
}

void SppClient::SetUpdateCallback(SppUpdateCallback callback) {
    std::scoped_lock lock(mutex_);
    callback_ = callback;
}

bool SppClient::SendEq(uint8_t preset_val) {
    const std::array<uint8_t, 2> payload = {preset_val, 0x00};
    return SendCommand(61456, payload);
}

void SppClient::ReaderLoop() {
    LogDebug("SppClient: ReaderLoop started");
    try {
        DataReader reader(socket_.InputStream());
        reader.InputStreamOptions(InputStreamOptions::Partial);
        
        while (read_active_.load(std::memory_order_acquire)) {
            // 1. Read magic byte (0x55)
            if (reader.LoadAsync(1).get() < 1) {
                break;
            }
            uint8_t magic = reader.ReadByte();
            if (magic != 0x55) {
                continue;
            }

            // 2. Read next 7 bytes of the header
            if (reader.LoadAsync(7).get() < 7) {
                break;
            }
            uint8_t h1 = reader.ReadByte(); // 0x60
            uint8_t h2 = reader.ReadByte(); // 0x01
            
            uint8_t cmd_lo = reader.ReadByte();
            uint8_t cmd_hi = reader.ReadByte();
            uint16_t command = static_cast<uint16_t>(cmd_lo | (cmd_hi << 8));
            
            uint8_t payload_len = reader.ReadByte();
            uint8_t h5 = reader.ReadByte(); // 0x00
            uint8_t op_id = reader.ReadByte();

            // 3. Read payload and 2 CRC bytes
            uint32_t to_read = static_cast<uint32_t>(payload_len) + 2;
            if (reader.LoadAsync(to_read).get() < to_read) {
                break;
            }

            std::vector<uint8_t> payload(payload_len);
            if (payload_len > 0) {
                reader.ReadBytes(payload);
            }
            uint8_t crc_lo = reader.ReadByte();
            uint8_t crc_hi = reader.ReadByte();
            uint16_t packet_crc = static_cast<uint16_t>(crc_lo | (crc_hi << 8));

            // Process packet
            ProcessIncomingPacket(command, payload);
        }
    } catch (const winrt::hresult_error& e) {
        LogDebug("SppClient: ReaderLoop winrt exception: " + winrt::to_string(e.message()));
    } catch (const std::exception& e) {
        LogDebug("SppClient: ReaderLoop exception: " + std::string(e.what()));
    } catch (...) {
        LogDebug("SppClient: ReaderLoop unknown exception");
    }

    LogDebug("SppClient: ReaderLoop exited");
    connected_.store(false, std::memory_order_release);
    SppUpdateCallback cb;
    {
        std::scoped_lock lock(mutex_);
        cb = callback_;
    }
    if (cb) {
        SppStateUpdate update;
        update.type = SppStateUpdate::Type::ConnectionState;
        update.connected = false;
        cb(update);
    }
}

void SppClient::ProcessIncomingPacket(uint16_t command, std::span<const uint8_t> payload) {
    LogDebug("SppClient: Received command response: " + std::to_string(command) + ", payload size: " + std::to_string(payload.size()));
    
    if (command == 57345 || command == 16391) {
        // Battery status
        if (payload.size() >= 1) {
            uint8_t connectedDevices = payload[0];
            BatteryReading left{}, right{}, case_battery{};
            for (uint8_t i = 0; i < connectedDevices; ++i) {
                if (1 + (i * 2) + 1 < payload.size()) {
                    uint8_t deviceId = payload[1 + (i * 2)];
                    uint8_t rawVal = payload[2 + (i * 2)];
                    BatteryReading rd = ParseBatteryByte(rawVal);
                    
                    if (deviceId == 0x02) {
                        left = rd;
                    } else if (deviceId == 0x03) {
                        right = rd;
                    } else if (deviceId == 0x04) {
                        case_battery = rd;
                    }
                }
            }
            
            SppUpdateCallback cb;
            {
                std::scoped_lock lock(mutex_);
                cb = callback_;
            }
            if (cb) {
                SppStateUpdate update;
                update.type = SppStateUpdate::Type::Battery;
                update.left = left;
                update.right = right;
                update.case_battery = case_battery;
                cb(update);
            }
        }
    } else if (command == 57347 || command == 16414 || command == 28688) {
        // ANC status
        if (!payload.empty()) {
            uint8_t type = payload[0];
            uint8_t level = (payload.size() >= 2) ? payload[1] : 0x00;
            AncMode mode = AncMode::High;
            bool valid = true;
            if (type == 0x02 || type == 0x05) {
                mode = AncMode::Transparency;
            } else if (type == 0x03 || type == 0x07 || type == 0x00) {
                mode = AncMode::Off;
            } else if (type == 0x01) {
                if (level == 0x01) mode = AncMode::High;
                else if (level == 0x02) mode = AncMode::Mid;
                else if (level == 0x03) mode = AncMode::Low;
                else if (level == 0x04) mode = AncMode::Adaptive;
                else mode = AncMode::High;
            } else if (type == 0x04) {
                mode = AncMode::Adaptive;
            } else {
                valid = false;
            }
            if (valid) {
                SppUpdateCallback cb;
                {
                    std::scoped_lock lock(mutex_);
                    cb = callback_;
                }
                if (cb) {
                    SppStateUpdate update;
                    update.type = SppStateUpdate::Type::AncMode;
                    update.anc_mode = mode;
                    cb(update);
                }
            }
        }
    } else if (command == 16415 || command == 16464) {
        // EQ status
        if (payload.size() >= 1) {
            uint8_t eqPreset = payload[0];
            SppUpdateCallback cb;
            {
                std::scoped_lock lock(mutex_);
                cb = callback_;
            }
            if (cb) {
                SppStateUpdate update;
                update.type = SppStateUpdate::Type::EqPreset;
                update.eq_preset = eqPreset;
                cb(update);
            }
        }
    } else if (command == 16462) {
        // Enhanced Bass status
        if (payload.size() >= 2) {
            bool enabled = payload[0] != 0;
            uint8_t level = payload[1] / 2;
            SppUpdateCallback cb;
            {
                std::scoped_lock lock(mutex_);
                cb = callback_;
            }
            if (cb) {
                SppStateUpdate update;
                update.type = SppStateUpdate::Type::BassState;
                update.bass_enabled = enabled;
                update.bass_level = level;
                cb(update);
            }
        }
    } else if (command == 16450) {
        // Firmware version
        std::wstring fw_str;
        for (uint8_t b : payload) {
            if (b >= 32 && b <= 126) {
                fw_str.push_back(static_cast<wchar_t>(b));
            }
        }
        SppUpdateCallback cb;
        {
            std::scoped_lock lock(mutex_);
            cb = callback_;
        }
        if (cb) {
            SppStateUpdate update;
            update.type = SppStateUpdate::Type::FirmwareVersion;
            update.text_value = fw_str;
            cb(update);
        }
    } else if (command == 16390) {
        // Serial number response
        std::string csv_str(payload.begin(), payload.end());
        std::vector<std::string> lines;
        std::stringstream ss(csv_str);
        std::string line;
        while (std::getline(ss, line, '\n')) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines.push_back(line);
        }

        std::string found_serial = "";
        for (const auto& l : lines) {
            std::vector<std::string> parts;
            std::stringstream line_ss(l);
            std::string part;
            while (std::getline(line_ss, part, ',')) {
                parts.push_back(part);
            }
            if (parts.size() >= 3) {
                std::string type_str = parts[1];
                type_str.erase(std::remove_if(type_str.begin(), type_str.end(), [](unsigned char c) {
                    return !std::isdigit(c);
                }), type_str.end());

                if (type_str == "4") {
                    std::string serial = parts[2];
                    serial.erase(std::remove_if(serial.begin(), serial.end(), [](unsigned char c) {
                        return std::isspace(c) || !std::isprint(c);
                    }), serial.end());

                    if (!serial.empty()) {
                        found_serial = serial;
                        break;
                    }
                }
            }
        }

        if (!found_serial.empty()) {
            std::wstring wserial(found_serial.begin(), found_serial.end());
            LogDebug("SppClient: Parsed serial number: " + found_serial);
            SppUpdateCallback cb;
            {
                std::scoped_lock lock(mutex_);
                cb = callback_;
            }
            if (cb) {
                SppStateUpdate update;
                update.type = SppStateUpdate::Type::DeviceModel;
                update.text_value = wserial;
                cb(update);
            }
        }
    } else if (command == 16449) {
        // Low Latency Mode
        if (payload.size() >= 1) {
            bool enabled = (payload[0] == 0x01);
            SppUpdateCallback cb;
            {
                std::scoped_lock lock(mutex_);
                cb = callback_;
            }
            if (cb) {
                SppStateUpdate update;
                update.type = SppStateUpdate::Type::LowLatency;
                update.low_latency_enabled = enabled;
                cb(update);
            }
        }
    }
}

std::wstring SppClient::GetDeviceName() const {
    std::scoped_lock lock(mutex_);
    return device_name_;
}

bool SppClient::SendPersonalizedAnc(bool enabled) {
    const std::array<uint8_t, 1> payload = {static_cast<uint8_t>(enabled ? 0x01 : 0x00)};
    return SendCommand(61457, payload);
}

bool SppClient::SendFindMyBuds(bool left, bool active) {
    LogDebug("SppClient: SendFindMyBuds side=" + std::string(left ? "Left" : "Right") + " active=" + std::to_string(active));
    uint8_t state_val = active ? 0x01 : 0x00;
    uint8_t side_1 = left ? 0x01 : 0x02;
    uint8_t side_2 = left ? 0x02 : 0x03;

    std::array<uint8_t, 3> payload1 = {0x01, side_1, state_val};
    std::array<uint8_t, 2> payload2 = {side_1, state_val};
    std::array<uint8_t, 2> payload3 = {side_2, state_val};

    bool ok = SendCommand(61442, payload1);
    ok |= SendCommand(61442, payload2);
    ok |= SendCommand(61442, payload3);
    ok |= SendCommand(61443, payload1);
    return ok;
}

bool SppClient::SendLowLatency(bool enabled) {
    const std::array<uint8_t, 2> payload = {
        static_cast<uint8_t>(enabled ? 0x01 : 0x02),
        0x00
    };
    return SendCommand(61504, payload);
}

} // namespace nothing_tray