/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

//
// TAP/TUN Network Backend Implementation
// Provides virtual network device connectivity
//

#pragma once

#include "NetworkBackend.h"

#include <atomic>
#include <mutex>
#include <thread>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Xe {
namespace Network {

// TAP device configuration
struct TAPConfig {
  std::string deviceName;     // Name of the TAP device (e.g., "tap0" on Linux, adapter GUID on Windows)
  std::string ipAddress;      // Optional IP address to configure
  std::string netmask;        // Optional netmask
  u32 mtu = 1500;             // Maximum transmission unit
  bool persistentMode = true; // Keep device alive after close (Linux)
};

class TAPBackend : public INetworkBackend {
public:
  explicit TAPBackend(const TAPConfig& config);
  ~TAPBackend() override;
  
  // INetworkBackend interface
  bool Initialize() override;
  void Shutdown() override;
  bool IsReady() const override;
  bool SendPacket(const u8* data, u32 length) override;
  void SetPacketCallback(PacketCallback callback) override;
  BackendType GetType() const override { return BackendType::TAP; }
  std::string GetName() const override;
  bool GetMACAddress(u8* mac) const override;
  bool SetMACAddress(const u8* mac) override;
  const BackendStats& GetStats() const override { return stats; }
  bool IsLinkUp() const override { return linkUp; }

private:
  // Platform-specific initialization
  bool InitializePlatform();
  void ShutdownPlatform();
  
  // Reader thread
  void ReaderThreadLoop();
  
  // Configuration
  TAPConfig config;
  
  // State
  std::atomic<bool> ready{false};
  std::atomic<bool> linkUp{false};
  
  // Callback
  PacketCallback packetCallback;
  std::mutex callbackMutex;
  
  // Reader thread
  std::thread readerThread;
  std::atomic<bool> readerRunning{false};
  
  // Statistics
  BackendStats stats;
  
  // MAC address (set during init or manually)
  u8 macAddress[6] = {0};
  bool hasMacAddress = false;
  
#ifdef _WIN32
  // Windows TAP-Windows or WinTap handle
  HANDLE tapHandle = INVALID_HANDLE_VALUE;
  OVERLAPPED readOverlapped = {};
  OVERLAPPED writeOverlapped = {};
#else
  // Linux/macOS file descriptor
  int tapFd = -1;
#endif
};

// Helper function to parse TAP config from string
// Format: "deviceName[:ipAddress/netmask]"
// Examples: "tap0", "tap0:192.168.1.100/24", "{GUID}"
TAPConfig ParseTAPConfig(const std::string& configStr);

// List available TAP devices on the system
std::vector<std::string> ListTAPDevices();

} // namespace Network
} // namespace Xe
