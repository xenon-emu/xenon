/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

// TAP/TUN network backend implementation.

#include "TAPBackend.h"

#include "Base/Global.h"
#include "Base/Logging/Log.h"
#include "Base/Thread.h"

#include <algorithm>
#include <cstring>
#include <vector>

// Every frame the backend moves, the device it opens and the errors the host reports.
// #define TAP_DEBUG

#ifdef TAP_DEBUG
  #define TAP_DEBUGP(x, ...) LOG_DEBUG(ETH, "[TAP] " x, ##__VA_ARGS__)
#else
  #define TAP_DEBUGP(x, ...)
#endif

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <cfgmgr32.h>
  #include <devguid.h>
  #include <iphlpapi.h>
  #include <setupapi.h>
  #include <windows.h>
  #include <winioctl.h>
  #include <winsock2.h>
  #pragma comment(lib, "iphlpapi.lib")
  #pragma comment(lib, "setupapi.lib")

// TAP-Windows device types. Order matters, classic TAP is preferred over DCO/WinTun.
static const char *TAP_COMPONENT_IDS[] = {"tap0901",       // OpenVPN TAP-Windows6 (PREFERRED, supports all IOCTLs)
                                          "tap0801",       // Older TAP-Windows
                                          "root\\tap0901", // Alternative format
                                          // DCO and WinTun come last: they don't support the media status IOCTL and
                                          // need different APIs, so they're only used if no classic TAP is available.
                                          // "wintun",       // WireGuard WinTun - NOT COMPATIBLE (requires WinTun API)
                                          // "ovpn-dco",     // OpenVPN DCO - NOT COMPATIBLE (requires DCO API)
                                          nullptr};

// TAP-Windows IOCTL definitions. The base is different for TAP-Windows6 vs older versions.

// TAP-Windows6 (tap0901) IOCTL codes
static constexpr DWORD TAP_WIN_IOCTL_GET_MAC = CTL_CODE(FILE_DEVICE_UNKNOWN, 1, METHOD_BUFFERED, FILE_ANY_ACCESS);
static constexpr DWORD TAP_WIN_IOCTL_GET_VERSION = CTL_CODE(FILE_DEVICE_UNKNOWN, 2, METHOD_BUFFERED, FILE_ANY_ACCESS);
static constexpr DWORD TAP_WIN_IOCTL_SET_MEDIA_STATUS =
  CTL_CODE(FILE_DEVICE_UNKNOWN, 6, METHOD_BUFFERED, FILE_ANY_ACCESS);

// Alternative IOCTL codes used by some driver versions (0x8xx range)
static constexpr DWORD TAP_WIN_IOCTL_GET_MAC_ALT =
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS);
static constexpr DWORD TAP_WIN_IOCTL_GET_VERSION_ALT =
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS);
static constexpr DWORD TAP_WIN_IOCTL_SET_MEDIA_STATUS_ALT =
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS);

// Network adapter registry paths
static constexpr const char *ADAPTER_KEY =
  "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e972-e325-11ce-bfc1-08002be10318}";
static constexpr const char *NETWORK_CONNECTIONS_KEY =
  "SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}";

#else
  // Linux/macOS
  #include <fcntl.h>
  #include <net/if.h>
  #include <poll.h>
  #include <sys/ioctl.h>
  #include <sys/socket.h>
  #include <unistd.h>

  #ifdef __linux__
    #include <linux/if_tun.h>
  #elif defined(__APPLE__)
    #include <net/if_utun.h>
    #include <sys/kern_control.h>
    #include <sys/sys_domain.h>
  #endif
#endif

namespace Xe::Network {

  TAPBackend::TAPBackend(const TAPConfig &cfg)
      : config(cfg) {}

  TAPBackend::~TAPBackend() { Shutdown(); }

  bool TAPBackend::Initialize() {
    if (ready) { return true; }

    LOG_INFO(ETH, "TAP Backend: Initializing device '{}'", config.deviceName);

    if (!InitializePlatform()) {
      LOG_ERROR(ETH, "TAP Backend: Failed to initialize platform-specific components");
      return false;
    }

    // Start reader thread
    readerRunning = true;
    readerThread = std::thread(&TAPBackend::ReaderThreadLoop, this);

    // And the sender.
    sendSlots = std::make_unique<SendSlot[]>(kSendSlots);
    sendHead = sendCount = 0;
    senderRunning = true;
    senderThread = std::thread(&TAPBackend::SenderThreadLoop, this);

    ready = true;
    linkUp = true;

    LOG_INFO(ETH, "TAP Backend: Initialized successfully");

    if (hasMacAddress) {
      LOG_INFO(ETH, "TAP Backend: MAC Address: {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}", macAddress[0], macAddress[1],
               macAddress[2], macAddress[3], macAddress[4], macAddress[5]);
    }

    return true;
  }

  void TAPBackend::Shutdown() {
    if (!ready) { return; }

    LOG_INFO(ETH, "TAP Backend: Shutting down");

    // Stop reader thread
    readerRunning = false;
    linkUp = false;

    if (readerThread.joinable()) { readerThread.join(); }

    {
      std::lock_guard<std::mutex> lock(sendMutex);
      senderRunning = false;
    }
    sendCV.notify_all();
    if (senderThread.joinable()) { senderThread.join(); }

    ShutdownPlatform();

    ready = false;

    LOG_INFO(ETH, "TAP Backend: Shutdown complete. TX: {} packets, RX: {} packets", txPackets.load(), rxPackets.load());
  }

  bool TAPBackend::IsReady() const { return ready; }

  bool TAPBackend::GetMACAddress(u8 *mac) const {
    if (!hasMacAddress || !mac) { return false; }
    memcpy(mac, macAddress, 6);
    return true;
  }

  void TAPBackend::SetPacketCallback(PacketCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex);
    packetCallback = callback;
  }

  bool TAPBackend::SendPacket(const u8 *data, u32 length) {
    if (!ready || !sendSlots) {
      TAP_DEBUGP("TX dropped: backend not ready");
      return false;
    }
    if (!data || length == 0 || length > sizeof(SendSlot::data)) {
      TAP_DEBUGP("TX rejected: {} bytes", length);
      return false;
    }

    std::unique_lock<std::mutex> lock(sendMutex);
    if (sendCount == kSendSlots) {
      // The host isn't taking frames as fast as the guest sends them.
      TAP_DEBUGP("TX dropped: send queue full ({} frames)", kSendSlots);
      return false;
    }
    SendSlot &slot = sendSlots[(sendHead + sendCount) % kSendSlots];
    memcpy(slot.data, data, length);
    slot.length = length;
    ++sendCount;
    TAP_DEBUGP("TX queued: {} bytes, {} waiting", length, sendCount);
    const bool wake = senderWaiting;
    lock.unlock();
    if (wake) { sendCV.notify_one(); }
    return true;
  }

  // Writes the queued frames to the device in order. The slot at the head stays the sender's while it writes it:
  // SendPacket only fills slots behind it.
  void TAPBackend::SenderThreadLoop() {
    Base::SetCurrentThreadName("[Xe] TAP Sender");
    TAP_DEBUGP("Sender thread started");
    std::unique_lock<std::mutex> lock(sendMutex);
    while (senderRunning) {
      if (sendCount == 0) {
        senderWaiting = true;
        sendCV.wait_for(lock, std::chrono::milliseconds(100), [this] { return !senderRunning || sendCount != 0; });
        senderWaiting = false;
        continue;
      }
      const SendSlot &slot = sendSlots[sendHead];
      lock.unlock();
      if (WriteFrame(slot.data, slot.length)) {
        txPackets++;
        TAP_DEBUGP("TX written: {} bytes", slot.length);
      } else {
        TAP_DEBUGP("TX write failed: {} bytes", slot.length);
      }
      lock.lock();
      sendHead = (sendHead + 1) % kSendSlots;
      --sendCount;
    }
    TAP_DEBUGP("Sender thread stopped");
  }

#ifdef _WIN32
  // Windows implementation

  // Structure to hold TAP device info
  struct TAPDeviceInfo {
    std::string guid;
    std::string name;
    std::string componentId;
  };

  // Find all TAP devices and their info
  static std::vector<TAPDeviceInfo> FindAllTAPDevices() {
    std::vector<TAPDeviceInfo> devices;

    HKEY adaptersKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, ADAPTER_KEY, 0, KEY_READ, &adaptersKey) != ERROR_SUCCESS) {
      LOG_ERROR(ETH, "TAP Backend: Cannot open network adapters registry key");
      return devices;
    }

    for (DWORD i = 0;; i++) {
      char subkeyName[256];
      DWORD subkeyLen = sizeof(subkeyName);

      if (RegEnumKeyExA(adaptersKey, i, subkeyName, &subkeyLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
        break;
      }

      HKEY adapterKey;
      if (RegOpenKeyExA(adaptersKey, subkeyName, 0, KEY_READ, &adapterKey) != ERROR_SUCCESS) { continue; }

      char componentId[256] = {};
      DWORD componentIdLen = sizeof(componentId);
      DWORD type;

      if (RegQueryValueExA(adapterKey, "ComponentId", nullptr, &type, reinterpret_cast<LPBYTE>(componentId),
                           &componentIdLen)
          == ERROR_SUCCESS) {
        // Check if this is a TAP device
        bool isTap = false;
        for (int j = 0; TAP_COMPONENT_IDS[j] != nullptr; j++) {
          if (_stricmp(componentId, TAP_COMPONENT_IDS[j]) == 0) {
            isTap = true;
            break;
          }
        }

        if (isTap) {
          char netCfgInstanceId[256] = {};
          DWORD netCfgLen = sizeof(netCfgInstanceId);

          if (RegQueryValueExA(adapterKey, "NetCfgInstanceId", nullptr, &type,
                               reinterpret_cast<LPBYTE>(netCfgInstanceId), &netCfgLen)
              == ERROR_SUCCESS) {
            TAPDeviceInfo info;
            info.guid = netCfgInstanceId;
            info.componentId = componentId;

            // Try to get friendly name from Network Connections
            std::string connectionKey = std::string(NETWORK_CONNECTIONS_KEY) + "\\" + netCfgInstanceId + "\\Connection";
            HKEY connKey;
            if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, connectionKey.c_str(), 0, KEY_READ, &connKey) == ERROR_SUCCESS) {
              char friendlyName[256] = {};
              DWORD nameLen = sizeof(friendlyName);
              if (RegQueryValueExA(connKey, "Name", nullptr, &type, reinterpret_cast<LPBYTE>(friendlyName), &nameLen)
                  == ERROR_SUCCESS) {
                info.name = friendlyName;
              }
              RegCloseKey(connKey);
            }

            devices.push_back(info);
            LOG_INFO(ETH, "TAP Backend: Found device: GUID={}, Name='{}', Driver={}", info.guid,
                     info.name.empty() ? "(unnamed)" : info.name, info.componentId);
          }
        }
      }

      RegCloseKey(adapterKey);
    }

    RegCloseKey(adaptersKey);
    return devices;
  }

  bool TAPBackend::InitializePlatform() {
    // Find all TAP devices first
    auto tapDevices = FindAllTAPDevices();

    if (tapDevices.empty()) {
      LOG_ERROR(ETH, "TAP Backend: No compatible TAP devices found on system.");
      LOG_ERROR(ETH, "TAP Backend: Note: WinTun and OpenVPN DCO are NOT compatible.");
      LOG_ERROR(ETH, "TAP Backend: You need the classic TAP-Windows driver.");
      LOG_ERROR(ETH, "TAP Backend: Download from: https://build.openvpn.net/downloads/releases/");
      LOG_ERROR(ETH, "TAP Backend: Look for: tap-windows-9.24.x-xxxx-Win10.exe");
      return false;
    }

    // Select device
    std::string selectedGuid;
    std::string selectedComponentId;

    if (config.deviceName.empty() || config.deviceName == "auto") {
      // Auto-select first available
      selectedGuid = tapDevices[0].guid;
      selectedComponentId = tapDevices[0].componentId;
      config.deviceName = selectedGuid;
      LOG_INFO(ETH, "TAP Backend: Auto-selected device '{}' ({})",
               tapDevices[0].name.empty() ? selectedGuid : tapDevices[0].name, tapDevices[0].componentId);
    } else {
      // Try to find by GUID or name
      for (const auto &dev : tapDevices) {
        if (_stricmp(dev.guid.c_str(), config.deviceName.c_str()) == 0
            || _stricmp(dev.name.c_str(), config.deviceName.c_str()) == 0) {
          selectedGuid = dev.guid;
          selectedComponentId = dev.componentId;
          break;
        }
      }

      if (selectedGuid.empty()) {
        LOG_ERROR(ETH, "TAP Backend: Device '{}' not found. Available devices:", config.deviceName);
        for (const auto &dev : tapDevices) {
          LOG_ERROR(ETH, "  - {} ({})", dev.guid, dev.name.empty() ? "unnamed" : dev.name);
        }
        return false;
      }
    }

    // Work out whether this is a classic TAP or a WinTun/DCO driver.
    std::string componentIdLower = selectedComponentId;
    std::transform(componentIdLower.begin(), componentIdLower.end(), componentIdLower.begin(), ::tolower);

    bool isClassicTap = (componentIdLower.find("tap") != std::string::npos);
    bool isWinTun = (componentIdLower.find("wintun") != std::string::npos);
    bool isDCO =
      (componentIdLower.find("ovpn-dco") != std::string::npos || componentIdLower.find("dco") != std::string::npos);

    LOG_INFO(ETH, "TAP Backend: Component ID: '{}', isClassicTap={}, isWinTun={}, isDCO={}", selectedComponentId,
             isClassicTap, isWinTun, isDCO);

    // Try multiple path formats to open the device
    std::vector<std::string> pathFormats = {
      "\\\\.\\Global\\" + selectedGuid + ".tap",
      "\\\\.\\Global\\" + selectedGuid,
      "\\\\.\\tap0901",
      "\\\\.\\tap",
    };

    for (const auto &devicePath : pathFormats) {
      TAP_DEBUGP("Trying path: {}", devicePath);

      tapHandle = CreateFileA(devicePath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED, nullptr);

      if (tapHandle != INVALID_HANDLE_VALUE) {
        LOG_INFO(ETH, "TAP Backend: Successfully opened device with path: {}", devicePath);
        break;
      }

      TAP_DEBUGP("Path '{}' failed with error {}", devicePath, GetLastError());
    }

    if (tapHandle == INVALID_HANDLE_VALUE) {
      DWORD error = GetLastError();
      LOG_ERROR(ETH, "TAP Backend: Failed to open TAP device. Last error: {}", error);

      switch (error) {
        case ERROR_FILE_NOT_FOUND:
          LOG_ERROR(ETH, "TAP Backend: Device not found. The TAP driver may not be properly installed.");
          break;
        case ERROR_ACCESS_DENIED: LOG_ERROR(ETH, "TAP Backend: Access denied. Try running as Administrator."); break;
        case ERROR_SHARING_VIOLATION: LOG_ERROR(ETH, "TAP Backend: Device is in use by another application."); break;
        case ERROR_GEN_FAILURE: LOG_ERROR(ETH, "TAP Backend: General failure. The TAP adapter may be disabled."); break;
        default: LOG_ERROR(ETH, "TAP Backend: Unknown error {}.", error); break;
      }

      return false;
    }

    DWORD bytesReturned;

    // Get TAP driver version
    ULONG version[3] = {};
    if (DeviceIoControl(tapHandle, TAP_WIN_IOCTL_GET_VERSION, nullptr, 0, version, sizeof(version), &bytesReturned,
                        nullptr)) {
      LOG_INFO(ETH, "TAP Backend: Driver version {}.{}.{}", version[0], version[1], version[2]);
    } else if (DeviceIoControl(tapHandle, TAP_WIN_IOCTL_GET_VERSION_ALT, nullptr, 0, version, sizeof(version),
                               &bytesReturned, nullptr)) {
      LOG_INFO(ETH, "TAP Backend: Driver version {}.{}.{} (alt IOCTL)", version[0], version[1], version[2]);
    }

    // Get MAC address
    bool gotMac = false;
    if (DeviceIoControl(tapHandle, TAP_WIN_IOCTL_GET_MAC, nullptr, 0, macAddress, sizeof(macAddress), &bytesReturned,
                        nullptr)) {
      hasMacAddress = true;
      gotMac = true;
    } else if (DeviceIoControl(tapHandle, TAP_WIN_IOCTL_GET_MAC_ALT, nullptr, 0, macAddress, sizeof(macAddress),
                               &bytesReturned, nullptr)) {
      hasMacAddress = true;
      gotMac = true;
    }

    if (!gotMac) {
      // Fall back to GetAdaptersInfo
      ULONG adapterInfoSize = 0;
      GetAdaptersInfo(nullptr, &adapterInfoSize);

      if (adapterInfoSize > 0) {
        std::vector<u8> buffer(adapterInfoSize);
        PIP_ADAPTER_INFO adapterInfo = reinterpret_cast<PIP_ADAPTER_INFO>(buffer.data());

        if (GetAdaptersInfo(adapterInfo, &adapterInfoSize) == NO_ERROR) {
          for (PIP_ADAPTER_INFO adapter = adapterInfo; adapter != nullptr; adapter = adapter->Next) {
            if (strstr(adapter->AdapterName, selectedGuid.c_str()) != nullptr) {
              if (adapter->AddressLength == 6) {
                memcpy(macAddress, adapter->Address, 6);
                hasMacAddress = true;
                gotMac = true;
                LOG_INFO(ETH, "TAP Backend: Got MAC from adapter info");
                break;
              }
            }
          }
        }
      }
    }

    if (!gotMac) {
      LOG_INFO(ETH, "TAP Backend: Using generated MAC address");
      macAddress[0] = 0x02;
      macAddress[1] = 0x00;
      macAddress[2] = 0x00;
      macAddress[3] = static_cast<u8>(GetCurrentProcessId() & 0xFF);
      macAddress[4] = static_cast<u8>((GetCurrentProcessId() >> 8) & 0xFF);
      macAddress[5] = static_cast<u8>(GetTickCount() & 0xFF);
      hasMacAddress = true;
    }

    // Set the media status to connected. Try every IOCTL variation, whatever driver type was detected.
    {
      ULONG mediaStatus = TRUE;
      bool mediaSet = false;
      DWORD lastErr = 0;

      DWORD ioctlCodes[] = {
        TAP_WIN_IOCTL_SET_MEDIA_STATUS,
        TAP_WIN_IOCTL_SET_MEDIA_STATUS_ALT,
        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x06, METHOD_BUFFERED, FILE_ANY_ACCESS),
        CTL_CODE(34, 6, METHOD_BUFFERED, FILE_ANY_ACCESS),
      };

      for (DWORD ioctl : ioctlCodes) {
        DWORD returned = 0;
        if (DeviceIoControl(tapHandle, ioctl, &mediaStatus, sizeof(mediaStatus), &mediaStatus, sizeof(mediaStatus),
                            &returned, nullptr)) {
          mediaSet = true;
          LOG_INFO(ETH, "TAP Backend: Media status CONNECTED (IOCTL=0x{:08X})", ioctl);
          break;
        }
        lastErr = GetLastError();
        TAP_DEBUGP("IOCTL 0x{:08X} failed, error {}", ioctl, lastErr);
      }

      if (!mediaSet) {
        LOG_WARNING(ETH, "TAP Backend: Could not set media status (error {})", lastErr);
        if (isWinTun || isDCO) {
          LOG_INFO(ETH, "TAP Backend: WinTun/DCO work without media status IOCTL");
        } else {
          LOG_WARNING(ETH, "TAP Backend: Adapter may show 'Disconnected'");
          LOG_WARNING(ETH, "TAP Backend: Try TAP-Windows 9.21.2 from OpenVPN");
        }
      }
    }

    // The read slots and the sender's write, each with its own manual reset event.
    readSlots = std::make_unique<IoSlot[]>(kReadSlots);
    bool eventsMade = true;
    for (u32 i = 0; i != kReadSlots; ++i) {
      readSlots[i].overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
      eventsMade &= readSlots[i].overlapped.hEvent != nullptr;
    }
    memset(&writeOverlapped, 0, sizeof(writeOverlapped));
    writeOverlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    eventsMade &= writeOverlapped.hEvent != nullptr;
    if (!eventsMade) {
      LOG_ERROR(ETH, "TAP Backend: Failed to create overlapped events");
      ShutdownPlatform();
      return false;
    }

    return true;
  }

  // Runs after the reader and sender threads have stopped (no reads or writes in flight).
  void TAPBackend::ShutdownPlatform() {
    if (tapHandle != INVALID_HANDLE_VALUE) {
      // Set media status to disconnected
      ULONG mediaStatus = FALSE;
      DWORD bytesReturned;
      DeviceIoControl(tapHandle, TAP_WIN_IOCTL_SET_MEDIA_STATUS, &mediaStatus, sizeof(mediaStatus), &mediaStatus,
                      sizeof(mediaStatus), &bytesReturned, nullptr);

      CloseHandle(tapHandle);
      tapHandle = INVALID_HANDLE_VALUE;
    }

    if (readSlots) {
      for (u32 i = 0; i != kReadSlots; ++i) {
        if (readSlots[i].overlapped.hEvent) { CloseHandle(readSlots[i].overlapped.hEvent); }
      }
      readSlots.reset();
    }
    if (writeOverlapped.hEvent) {
      CloseHandle(writeOverlapped.hEvent);
      writeOverlapped.hEvent = nullptr;
    }
  }

  bool TAPBackend::WriteFrame(const u8 *data, u32 length) {
    if (tapHandle == INVALID_HANDLE_VALUE) { return false; }
    DWORD bytesWritten = 0;
    ResetEvent(writeOverlapped.hEvent);
    if (!WriteFile(tapHandle, data, length, &bytesWritten, &writeOverlapped)) {
      const DWORD error = GetLastError();
      if (error != ERROR_IO_PENDING) {
        TAP_DEBUGP("WriteFile failed, error {}", error);
        return false;
      }
      if (WaitForSingleObject(writeOverlapped.hEvent, 1000) != WAIT_OBJECT_0) {
        // The driver may still be reading data: wait for the cancellation before the slot is reused.
        TAP_DEBUGP("WriteFile did not complete within a second, cancelling it");
        CancelIoEx(tapHandle, &writeOverlapped);
        GetOverlappedResult(tapHandle, &writeOverlapped, &bytesWritten, TRUE);
        return false;
      }
      GetOverlappedResult(tapHandle, &writeOverlapped, &bytesWritten, FALSE);
    }
    return true;
  }

  // kReadSlots reads stay in flight. They are consumed in the order issued, and each is issued again (to the back of
  // the driver's queue) once its frame has been handed on.
  void TAPBackend::ReaderThreadLoop() {
    Base::SetCurrentThreadName("[Xe] TAP Reader");

    const auto issue = [&](IoSlot &slot) {
      ResetEvent(slot.overlapped.hEvent);
      DWORD bytes = 0;
      if (ReadFile(tapHandle, slot.buffer, sizeof(slot.buffer), &bytes, &slot.overlapped)
          || GetLastError() == ERROR_IO_PENDING) {
        slot.pending = true; // also when it completed at once, the event is set, the result is taken below
        return true;
      }
      TAP_DEBUGP("ReadFile failed, error {}", GetLastError());
      return false;
    };

    TAP_DEBUGP("Reader thread started");
    u32 next = 0; // the oldest read
    while (readerRunning && XeRunning) {
      if (tapHandle == INVALID_HANDLE_VALUE || !readSlots) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }
      // Keep every slot in flight.
      bool failed = false;
      for (u32 i = 0; i != kReadSlots; ++i) {
        IoSlot &slot = readSlots[(next + i) % kReadSlots];
        if (!slot.pending && !issue(slot)) {
          failed = true;
          break;
        }
      }
      IoSlot &slot = readSlots[next];
      if (!slot.pending) {
        if (failed) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
        continue;
      }

      // Wait for the oldest read (then check whether we should exit, keeping it pending).
      const DWORD result = WaitForSingleObject(slot.overlapped.hEvent, 100);
      if (result == WAIT_TIMEOUT) { continue; }
      DWORD bytesRead = 0;
      const bool ok = result == WAIT_OBJECT_0 && GetOverlappedResult(tapHandle, &slot.overlapped, &bytesRead, FALSE);
      slot.pending = false;
      next = (next + 1) % kReadSlots;
      if (!ok) {
        TAP_DEBUGP("Read completed with an error (wait result {})", result);
        continue;
      }

      if (bytesRead > 0) {
        rxPackets++;
        TAP_DEBUGP("RX: {} bytes", bytesRead);

        // Call the callback
        std::lock_guard<std::mutex> lock(callbackMutex);
        if (packetCallback) { packetCallback(slot.buffer, bytesRead); }
      }
    }

    if (readSlots) {
      for (u32 i = 0; i != kReadSlots; ++i) {
        IoSlot &slot = readSlots[i];
        if (!slot.pending) { continue; }
        DWORD bytesRead = 0;
        CancelIoEx(tapHandle, &slot.overlapped);
        GetOverlappedResult(tapHandle, &slot.overlapped, &bytesRead, TRUE);
        slot.pending = false;
      }
    }
    TAP_DEBUGP("Reader thread stopped");
  }

#else
  // Linux/macOS implementation

  bool TAPBackend::InitializePlatform() {
  #ifdef __linux__
    // Open the TUN/TAP clone device
    tapFd = open("/dev/net/tun", O_RDWR);
    if (tapFd < 0) {
      LOG_ERROR(ETH, "TAP Backend: Failed to open /dev/net/tun: {}", strerror(errno));
      return false;
    }

    // Configure the interface
    struct ifreq ifr = {};
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

    if (!config.deviceName.empty() && config.deviceName != "auto") {
      strncpy(ifr.ifr_name, config.deviceName.c_str(), IFNAMSIZ - 1);
    }

    if (ioctl(tapFd, TUNSETIFF, &ifr) < 0) {
      LOG_ERROR(ETH, "TAP Backend: Failed to configure TAP device: {}", strerror(errno));
      close(tapFd);
      tapFd = -1;
      return false;
    }

    config.deviceName = ifr.ifr_name;

    // Set persistent mode if requested
    if (config.persistentMode) {
      if (ioctl(tapFd, TUNSETPERSIST, 1) < 0) {
        LOG_WARNING(ETH, "TAP Backend: Failed to set persistent mode: {}", strerror(errno));
      }
    }

    // Get MAC address
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock >= 0) {
      memset(&ifr, 0, sizeof(ifr));
      strncpy(ifr.ifr_name, config.deviceName.c_str(), IFNAMSIZ - 1);

      if (ioctl(sock, SIOCGIFHWADDR, &ifr) >= 0) {
        memcpy(macAddress, ifr.ifr_hwaddr.sa_data, 6);
        hasMacAddress = true;
      }
      close(sock);
    }

    // Set non-blocking mode
    int flags = fcntl(tapFd, F_GETFL, 0);
    fcntl(tapFd, F_SETFL, flags | O_NONBLOCK);

    return true;

  #elif defined(__APPLE__)
    // macOS uses utun devices. utun is point-to-point, not TAP, a true TAP needs the tuntaposx kext.
    tapFd = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
    if (tapFd < 0) {
      LOG_ERROR(ETH, "TAP Backend: Failed to create control socket: {}", strerror(errno));
      return false;
    }

    struct ctl_info ctlInfo = {};
    strncpy(ctlInfo.ctl_name, UTUN_CONTROL_NAME, sizeof(ctlInfo.ctl_name));

    if (ioctl(tapFd, CTLIOCGINFO, &ctlInfo) < 0) {
      LOG_ERROR(ETH, "TAP Backend: Failed to get utun control info: {}", strerror(errno));
      close(tapFd);
      tapFd = -1;
      return false;
    }

    struct sockaddr_ctl sc = {};
    sc.sc_id = ctlInfo.ctl_id;
    sc.sc_len = sizeof(sc);
    sc.sc_family = AF_SYSTEM;
    sc.ss_sysaddr = AF_SYS_CONTROL;
    sc.sc_unit = 0; // Auto-assign unit number

    if (connect(tapFd, reinterpret_cast<struct sockaddr *>(&sc), sizeof(sc)) < 0) {
      LOG_ERROR(ETH, "TAP Backend: Failed to connect to utun: {}", strerror(errno));
      close(tapFd);
      tapFd = -1;
      return false;
    }

    // Get the device name
    char ifname[IFNAMSIZ];
    socklen_t ifnamelen = sizeof(ifname);
    if (getsockopt(tapFd, SYSPROTO_CONTROL, UTUN_OPT_IFNAME, ifname, &ifnamelen) >= 0) { config.deviceName = ifname; }

    // Set non-blocking
    int flags = fcntl(tapFd, F_GETFL, 0);
    fcntl(tapFd, F_SETFL, flags | O_NONBLOCK);

    return true;
  #else
    LOG_ERROR(ETH, "TAP Backend: Platform not supported");
    return false;
  #endif
  }

  void TAPBackend::ShutdownPlatform() {
    if (tapFd >= 0) {
  #ifdef __linux__
      // Clear persistent mode
      if (config.persistentMode) { ioctl(tapFd, TUNSETPERSIST, 0); }
  #endif
      close(tapFd);
      tapFd = -1;
    }
  }

  bool TAPBackend::WriteFrame(const u8 *data, u32 length) {
    if (tapFd < 0) { return false; }

    if (write(tapFd, data, length) < 0) {
      TAP_DEBUGP("write failed: {}", strerror(errno));
      return false;
    }
    return true;
  }

  void TAPBackend::ReaderThreadLoop() {
    Base::SetCurrentThreadName("[Xe] TAP Reader");

    std::vector<u8> buffer(2048);
    struct pollfd pfd = {};
    pfd.fd = tapFd;
    pfd.events = POLLIN;

    TAP_DEBUGP("Reader thread started");
    while (readerRunning && XeRunning) {
      if (tapFd < 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }

      // Poll with timeout
      int ret = poll(&pfd, 1, 100);

      if (ret < 0 && errno != EINTR) { TAP_DEBUGP("poll failed: {}", strerror(errno)); }
      // Error, or timeout with no data
      if (ret <= 0) { continue; }

      if (pfd.revents & POLLIN) {
        ssize_t bytesRead = read(tapFd, buffer.data(), buffer.size());

        if (bytesRead > 0) {
          rxPackets++;
          TAP_DEBUGP("RX: {} bytes", bytesRead);

          // Call the callback
          std::lock_guard<std::mutex> lock(callbackMutex);
          if (packetCallback) { packetCallback(buffer.data(), static_cast<u32>(bytesRead)); }
        } else if (bytesRead < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
          TAP_DEBUGP("read failed: {}", strerror(errno));
        }
      }

      if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        LOG_ERROR(ETH, "TAP Backend: Poll error on TAP device");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }
    TAP_DEBUGP("Reader thread stopped");
  }

#endif // _WIN32

  // Common implementations

  TAPConfig ParseTAPConfig(const std::string &configStr) {
    TAPConfig config;

    if (configStr.empty()) {
      config.deviceName = "auto";
      return config;
    }

    // Parse format: "deviceName[:anything]"
    config.deviceName = configStr.substr(0, configStr.find(':'));

    return config;
  }

} // namespace Xe::Network
