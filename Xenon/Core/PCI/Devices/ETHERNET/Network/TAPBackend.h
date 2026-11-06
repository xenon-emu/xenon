/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

// TAP/TUN network backend

#pragma once

#include "NetworkBackend.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#ifdef _WIN32
  #include <windows.h>
#endif

namespace Xe::Network {

  // TAP device configuration
  struct TAPConfig {
    std::string deviceName;     // Name of the TAP device
    bool persistentMode = true; // Keep device alive after close (Linux)
  };

  class TAPBackend : public INetworkBackend {
  public:
    explicit TAPBackend(const TAPConfig &config);
    ~TAPBackend() override;

    // INetworkBackend interface
    bool Initialize() override;
    void Shutdown() override;
    bool IsReady() const override;
    bool SendPacket(const u8 *data, u32 length) override;
    void SetPacketCallback(PacketCallback callback) override;
    bool GetMACAddress(u8 *mac) const override;
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

    // Frames passed each way, reported when the backend shuts down
    std::atomic<u64> txPackets{0};
    std::atomic<u64> rxPackets{0};

    // MAC address (set during init or manually)
    u8 macAddress[6] = {0};
    bool hasMacAddress = false;

#ifdef _WIN32
    // Windows TAP-Windows or WinTap handle
    HANDLE tapHandle = INVALID_HANDLE_VALUE;
    // Overlapped reads kept in flight. A slot owns its buffer and OVERLAPPED until the driver completes it. Reads are
    // consumed in the order they were issued (the driver completes them in that order).
    static constexpr u32 kReadSlots = 16;
    struct IoSlot {
      OVERLAPPED overlapped = {};
      u8 buffer[2048] = {};
      bool pending = false;
    };
    std::unique_ptr<IoSlot[]> readSlots;
    // The sender thread's write (one at a time, in order).
    OVERLAPPED writeOverlapped = {};
#endif

    // Transmit queue: SendPacket copies the frame in and returns. The sender thread hands the frames to the host in
    // order, so the guest never waits for the host's network stack.
    static constexpr u32 kSendSlots = 256;
    struct SendSlot {
      u8 data[2048];
      u32 length;
    };
    std::unique_ptr<SendSlot[]> sendSlots;
    u32 sendHead = 0;
    u32 sendCount = 0;
    bool senderWaiting = false;
    std::mutex sendMutex;
    std::condition_variable sendCV;
    std::thread senderThread;
    std::atomic<bool> senderRunning{false};
    void SenderThreadLoop();
    // Writes one frame to the device.
    bool WriteFrame(const u8 *data, u32 length);

#ifndef _WIN32
    // Linux/macOS file descriptor
    int tapFd = -1;
#endif
  };

  // Parses the TAP config from a string, format "deviceName[:anything]". Whatever follows the colon (an address, say)
  // is ignored, the host side of the adapter is configured outside Xenon. Examples: "tap0", "{GUID}".
  TAPConfig ParseTAPConfig(const std::string &configStr);

} // namespace Xe::Network
