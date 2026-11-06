/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

// Network backend interface

#pragma once

#include "Base/Types.h"

#include <functional>
#include <memory>
#include <string>

namespace Xe::Network {

  // Network backend types
  enum class BackendType {
    None, // No networking (packets dropped)
    TAP,  // TAP/TUN virtual network device
  };

  // Packet received callback
  using PacketCallback = std::function<void(const u8 *data, u32 length)>;

  // Abstract network backend interface
  class INetworkBackend {
  public:
    virtual ~INetworkBackend() = default;

    // Initialize the backend. Returns true on success, false on failure.
    virtual bool Initialize() = 0;

    // Shutdown the backend
    virtual void Shutdown() = 0;

    // Check if the backend is initialized and ready
    virtual bool IsReady() const = 0;

    // Send a packet to the network. Returns true on success, false on failure.
    virtual bool SendPacket(const u8 *data, u32 length) = 0;

    // Set the callback for received packets
    virtual void SetPacketCallback(PacketCallback callback) = 0;

    // Get the MAC address of the backend interface (if applicable)
    virtual bool GetMACAddress(u8 *mac) const = 0;

    // Check if link is up
    virtual bool IsLinkUp() const = 0;
  };

  // Null backend, drops all packets (for when networking is disabled)
  class NullBackend : public INetworkBackend {
  public:
    bool Initialize() override {
      ready = true;
      return true;
    }
    void Shutdown() override { ready = false; }
    bool IsReady() const override { return ready; }

    bool SendPacket(const u8 * /*data*/, u32 /*length*/) override { return true; } // Silently drop

    void SetPacketCallback(PacketCallback /*callback*/) override {}

    bool GetMACAddress(u8 * /*mac*/) const override { return false; }
    bool IsLinkUp() const override { return false; }

  private:
    bool ready = false;
  };

  // Factory function to create backend based on type
  std::unique_ptr<INetworkBackend> CreateNetworkBackend(BackendType type, const std::string &config);

  // Get string name for backend type
  inline std::string BackendTypeToString(BackendType type) {
    switch (type) {
      case BackendType::None: return "none";
      case BackendType::TAP: return "tap";
      default: return "unknown";
    }
  }

  // Parse backend type from string
  inline BackendType StringToBackendType(const std::string &str) {
    if (str == "tap" || str == "TAP") return BackendType::TAP;
    return BackendType::None;
  }

} // namespace Xe::Network
