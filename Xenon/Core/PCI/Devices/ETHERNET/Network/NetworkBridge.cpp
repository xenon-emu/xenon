/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

// Network bridge manager

#include "NetworkBridge.h"

#include "../Ethernet.h"
#include "Base/Logging/Log.h"
#include "TAPBackend.h"

#include <mutex>

// Every frame that crosses the bridge and the backend's identity.
// #define NETBRIDGE_DEBUG

#ifdef NETBRIDGE_DEBUG
  #define NETBRIDGE_DEBUGP(x, ...) LOG_DEBUG(ETH, "[NETBRIDGE] " x, ##__VA_ARGS__)
#else
  #define NETBRIDGE_DEBUGP(x, ...)
#endif

namespace Xe::Network {

  // Factory function implementation
  std::unique_ptr<INetworkBackend> CreateNetworkBackend(BackendType type, const std::string &config) {
    switch (type) {
      case BackendType::TAP: {
        TAPConfig tapConfig = ParseTAPConfig(config);
        return std::make_unique<TAPBackend>(tapConfig);
      }
      case BackendType::None:
      default: return std::make_unique<NullBackend>();
    }
  }

  NetworkBridge::NetworkBridge() = default;

  NetworkBridge::~NetworkBridge() { Shutdown(); }

  NetworkBridge &NetworkBridge::Instance() {
    static NetworkBridge instance;
    return instance;
  }

  bool NetworkBridge::Initialize(const BridgeConfig &cfg) {
    if (active) {
      LOG_WARNING(ETH, "Network bridge already initialized");
      return true;
    }

    config = cfg;

    if (!config.enabled) {
      LOG_INFO(ETH, "Network bridging disabled");
      return true;
    }

    LOG_INFO(ETH, "Initializing network bridge with {} backend", BackendTypeToString(config.backendType));

    // Create the backend
    backend = CreateNetworkBackend(config.backendType, config.backendConfig);

    if (!backend) {
      LOG_ERROR(ETH, "Failed to create network backend");
      return false;
    }

    // Initialize the backend
    if (!backend->Initialize()) {
      LOG_ERROR(ETH, "Failed to initialize network backend");
      backend.reset();
      return false;
    }

    // Set up packet receive callback
    backend->SetPacketCallback([this](const u8 *data, u32 length) { OnPacketReceived(data, length); });

    active = true;

    LOG_INFO(ETH, "Network bridge initialized successfully");
    return true;
  }

  void NetworkBridge::Shutdown() {
    if (!active) { return; }

    LOG_INFO(ETH, "Shutting down network bridge");

    active = false;

    // Detach device
    DetachEthernetDevice();

    // Shutdown backend
    if (backend) {
      backend->Shutdown();
      backend.reset();
    }

    LOG_INFO(ETH, "Network bridge shutdown complete");
  }

  void NetworkBridge::AttachEthernetDevice(PCIDev::ETHERNET *device) {
    std::lock_guard<std::mutex> lock(deviceMutex);

    if (ethernetDevice) { LOG_WARNING(ETH, "Replacing existing Ethernet device attachment"); }

    ethernetDevice = device;

    if (device && backend && backend->IsReady()) {
      // Sync link state
      device->SetLinkUp(backend->IsLinkUp());

      // Optionally sync MAC address from backend
      u8 backendMac[6];
      if (backend->GetMACAddress(backendMac)) {
        NETBRIDGE_DEBUGP("Backend MAC: {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}", backendMac[0], backendMac[1],
                         backendMac[2], backendMac[3], backendMac[4], backendMac[5]);
      }

      LOG_INFO(ETH, "Ethernet device attached to network bridge");
    }
  }

  void NetworkBridge::DetachEthernetDevice() {
    std::lock_guard<std::mutex> lock(deviceMutex);

    if (ethernetDevice) {
      ethernetDevice = nullptr;
      LOG_INFO(ETH, "Ethernet device detached from network bridge");
    }
  }

  void NetworkBridge::OnPacketReceived(const u8 *data, u32 length) {
    if (!active || !data || length == 0) { return; }

    std::lock_guard<std::mutex> lock(deviceMutex);
    NETBRIDGE_DEBUGP("RX from the backend: {} bytes{}", length, ethernetDevice ? "" : ", no device attached");

    if (ethernetDevice) {
      // Forward packet to the guest Ethernet device
      ethernetDevice->EnqueueRxPacket(data, length);
    }
  }

} // namespace Xe::Network
