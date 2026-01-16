/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

//
// Network Backend Interface
// Abstract interface for different network backends (TAP, pcap, etc.)
//

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Base/Types.h"

namespace Xe {
namespace Network {

// Network backend types
enum class BackendType {
  None,       // No networking (packets dropped)
  TAP,        // TAP/TUN virtual network device
  PCAP,       // Packet capture (libpcap/npcap)
  Socket      // Raw socket (requires admin/root)
};

// Packet received callback
using PacketCallback = std::function<void(const u8* data, u32 length)>;

// Network backend statistics
struct BackendStats {
  u64 txPackets = 0;
  u64 rxPackets = 0;
  u64 txBytes = 0;
  u64 rxBytes = 0;
  u64 txErrors = 0;
  u64 rxErrors = 0;
  u64 txDropped = 0;
  u64 rxDropped = 0;
};

// Abstract network backend interface
class INetworkBackend {
public:
  virtual ~INetworkBackend() = default;
  
  // Initialize the backend
  // Returns true on success, false on failure
  virtual bool Initialize() = 0;
  
  // Shutdown the backend
  virtual void Shutdown() = 0;
  
  // Check if the backend is initialized and ready
  virtual bool IsReady() const = 0;
  
  // Send a packet to the network
  // Returns true on success, false on failure
  virtual bool SendPacket(const u8* data, u32 length) = 0;
  
  // Set the callback for received packets
  virtual void SetPacketCallback(PacketCallback callback) = 0;
  
  // Get the backend type
  virtual BackendType GetType() const = 0;
  
  // Get the backend name (for logging)
  virtual std::string GetName() const = 0;
  
  // Get the MAC address of the backend interface (if applicable)
  virtual bool GetMACAddress(u8* mac) const = 0;
  
  // Set the MAC address (if supported)
  virtual bool SetMACAddress(const u8* mac) = 0;
  
  // Get statistics
  virtual const BackendStats& GetStats() const = 0;
  
  // Check if link is up
  virtual bool IsLinkUp() const = 0;
};

// Null backend - drops all packets (for when networking is disabled)
class NullBackend : public INetworkBackend {
public:
  NullBackend() = default;
  ~NullBackend() override = default;
  
  bool Initialize() override { ready = true; return true; }
  void Shutdown() override { ready = false; }
  bool IsReady() const override { return ready; }
  
  bool SendPacket(const u8* /*data*/, u32 /*length*/) override { 
    stats.txDropped++;
    return true; // Silently drop
  }
  
  void SetPacketCallback(PacketCallback callback) override { 
    this->callback = callback;
  }
  
  BackendType GetType() const override { return BackendType::None; }
  std::string GetName() const override { return "Null"; }
  
  bool GetMACAddress(u8* /*mac*/) const override { return false; }
  bool SetMACAddress(const u8* /*mac*/) override { return false; }
  
  const BackendStats& GetStats() const override { return stats; }
  bool IsLinkUp() const override { return false; }

private:
  bool ready = false;
  PacketCallback callback;
  BackendStats stats;
};

// Factory function to create backend based on type
std::unique_ptr<INetworkBackend> CreateNetworkBackend(BackendType type, const std::string& config);

// Get string name for backend type
inline std::string BackendTypeToString(BackendType type) {
  switch (type) {
  case BackendType::None: return "none";
  case BackendType::TAP: return "tap";
  case BackendType::PCAP: return "pcap";
  case BackendType::Socket: return "socket";
  default: return "unknown";
  }
}

// Parse backend type from string
inline BackendType StringToBackendType(const std::string& str) {
  if (str == "tap" || str == "TAP") return BackendType::TAP;
  if (str == "pcap" || str == "PCAP") return BackendType::PCAP;
  if (str == "socket" || str == "Socket") return BackendType::Socket;
  return BackendType::None;
}

} // namespace Network
} // namespace Xe
