/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

//
// Network Bridge Manager
// Connects the emulated Ethernet device to the host network
//

#pragma once

#include "NetworkBackend.h"

#include <memory>
#include <string>
#include <atomic>
#include <mutex>

namespace Xe {

// Forward declarations
namespace PCIDev {
class ETHERNET;
}

namespace Network {

// Bridge configuration
struct BridgeConfig {
  bool enabled = false;                    // Enable network bridging
  BackendType backendType = BackendType::None; // Backend type to use
  std::string backendConfig;               // Backend-specific configuration string
};

class NetworkBridge {
public:
  NetworkBridge();
  ~NetworkBridge();
  
  // Initialize the bridge with the specified configuration
  bool Initialize(const BridgeConfig& config);
  
  // Shutdown the bridge
  void Shutdown();
  
  // Check if the bridge is active
  bool IsActive() const { return active; }
  
  // Attach the Ethernet device to the bridge
  void AttachEthernetDevice(PCIDev::ETHERNET* device);
  
  // Detach the Ethernet device
  void DetachEthernetDevice();
  
  // Get the current backend (for diagnostics)
  INetworkBackend* GetBackend() const { return backend.get(); }
  
  // Get configuration
  const BridgeConfig& GetConfig() const { return config; }
  
  // Static instance for global access
  static NetworkBridge& Instance();
  
private:
  // Packet received callback from backend
  void OnPacketReceived(const u8* data, u32 length);
  
  // Send packet from guest to host
  bool SendPacketToHost(const u8* data, u32 length);
  
  // Configuration
  BridgeConfig config;
  
  // Network backend
  std::unique_ptr<INetworkBackend> backend;
  
  // Attached Ethernet device
  PCIDev::ETHERNET* ethernetDevice = nullptr;
  std::mutex deviceMutex;
  
  // State
  std::atomic<bool> active{false};
};

// Global bridge instance access
inline NetworkBridge& GetNetworkBridge() {
  return NetworkBridge::Instance();
}

} // namespace Network
} // namespace Xe
