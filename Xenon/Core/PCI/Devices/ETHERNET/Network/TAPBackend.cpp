/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

//
// TAP/TUN Network Backend Implementation
//

#include "TAPBackend.h"

#include "Base/Logging/Log.h"
#include "Base/Thread.h"
#include "Base/Global.h"

#include <cstring>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <setupapi.h>
#include <devguid.h>
#include <cfgmgr32.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "setupapi.lib")

// TAP-Windows device types (various driver versions)
// NOTE: Order matters - we prefer classic TAP over DCO/WinTun
static const char* TAP_COMPONENT_IDS[] = {
  "tap0901",      // OpenVPN TAP-Windows6 (PREFERRED - supports all IOCTLs)
  "tap0801",      // Older TAP-Windows
  "root\\tap0901", // Alternative format
  // DCO and WinTun listed last - they don't support media status IOCTL
  // and require different APIs. Only use if no classic TAP is available.
  // "wintun",       // WireGuard WinTun - NOT COMPATIBLE (requires WinTun API)
  // "ovpn-dco",     // OpenVPN DCO - NOT COMPATIBLE (requires DCO API)
  nullptr
};

// TAP-Windows IOCTL definitions
// The base is different for TAP-Windows6 vs older versions
#define TAP_WIN_CONTROL_CODE(request, method) \
  CTL_CODE(FILE_DEVICE_UNKNOWN, request, method, FILE_ANY_ACCESS)

// TAP-Windows6 (tap0901) IOCTL codes - these are the standard ones
#define TAP_WIN_IOCTL_GET_MAC             TAP_WIN_CONTROL_CODE(1, METHOD_BUFFERED)
#define TAP_WIN_IOCTL_GET_VERSION         TAP_WIN_CONTROL_CODE(2, METHOD_BUFFERED)
#define TAP_WIN_IOCTL_GET_MTU             TAP_WIN_CONTROL_CODE(3, METHOD_BUFFERED)
#define TAP_WIN_IOCTL_GET_INFO            TAP_WIN_CONTROL_CODE(4, METHOD_BUFFERED)
#define TAP_WIN_IOCTL_CONFIG_POINT_TO_POINT TAP_WIN_CONTROL_CODE(5, METHOD_BUFFERED)
#define TAP_WIN_IOCTL_SET_MEDIA_STATUS    TAP_WIN_CONTROL_CODE(6, METHOD_BUFFERED)
#define TAP_WIN_IOCTL_CONFIG_DHCP_MASQ    TAP_WIN_CONTROL_CODE(7, METHOD_BUFFERED)
#define TAP_WIN_IOCTL_GET_LOG_LINE        TAP_WIN_CONTROL_CODE(8, METHOD_BUFFERED)
#define TAP_WIN_IOCTL_CONFIG_DHCP_SET_OPT TAP_WIN_CONTROL_CODE(9, METHOD_BUFFERED)
#define TAP_WIN_IOCTL_CONFIG_TUN          TAP_WIN_CONTROL_CODE(10, METHOD_BUFFERED)

// Alternative IOCTL codes used by some driver versions (0x8xx range)
#define TAP_WIN_IOCTL_GET_MAC_ALT             CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define TAP_WIN_IOCTL_GET_VERSION_ALT         CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define TAP_WIN_IOCTL_GET_MTU_ALT             CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define TAP_WIN_IOCTL_GET_INFO_ALT            CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define TAP_WIN_IOCTL_CONFIG_POINT_TO_POINT_ALT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define TAP_WIN_IOCTL_SET_MEDIA_STATUS_ALT    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define TAP_WIN_IOCTL_CONFIG_TUN_ALT          CTL_CODE(FILE_DEVICE_UNKNOWN, 0x80a, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Network adapter registry paths
#define ADAPTER_KEY "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e972-e325-11ce-bfc1-08002be10318}"
#define NETWORK_CONNECTIONS_KEY "SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}"

#else
// Linux/macOS
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <poll.h>

#ifdef __linux__
#include <linux/if_tun.h>
#elif defined(__APPLE__)
#include <sys/kern_control.h>
#include <sys/sys_domain.h>
#include <net/if_utun.h>
#endif
#endif

namespace Xe {
namespace Network {

TAPBackend::TAPBackend(const TAPConfig& cfg) : config(cfg) {
}

TAPBackend::~TAPBackend() {
  Shutdown();
}

bool TAPBackend::Initialize() {
  if (ready) {
    return true;
  }
  
  LOG_INFO(ETH, "TAP Backend: Initializing device '{}'", config.deviceName);
  
  if (!InitializePlatform()) {
    LOG_ERROR(ETH, "TAP Backend: Failed to initialize platform-specific components");
    return false;
  }
  
  // Start reader thread
  readerRunning = true;
  readerThread = std::thread(&TAPBackend::ReaderThreadLoop, this);
  
  ready = true;
  linkUp = true;
  
  LOG_INFO(ETH, "TAP Backend: Initialized successfully");
  
  if (hasMacAddress) {
    LOG_INFO(ETH, "TAP Backend: MAC Address: {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
      macAddress[0], macAddress[1], macAddress[2],
      macAddress[3], macAddress[4], macAddress[5]);
  }
  
  return true;
}

void TAPBackend::Shutdown() {
  if (!ready) {
    return;
  }
  
  LOG_INFO(ETH, "TAP Backend: Shutting down");
  
  // Stop reader thread
  readerRunning = false;
  linkUp = false;
  
  if (readerThread.joinable()) {
    readerThread.join();
  }
  
  ShutdownPlatform();
  
  ready = false;
  
  LOG_INFO(ETH, "TAP Backend: Shutdown complete. TX: {} packets, RX: {} packets",
    stats.txPackets, stats.rxPackets);
}

bool TAPBackend::IsReady() const {
  return ready;
}

std::string TAPBackend::GetName() const {
  return "TAP:" + config.deviceName;
}

bool TAPBackend::GetMACAddress(u8* mac) const {
  if (!hasMacAddress || !mac) {
    return false;
  }
  memcpy(mac, macAddress, 6);
  return true;
}

bool TAPBackend::SetMACAddress(const u8* mac) {
  if (!mac) {
    return false;
  }
  memcpy(macAddress, mac, 6);
  hasMacAddress = true;
  return true;
}

void TAPBackend::SetPacketCallback(PacketCallback callback) {
  std::lock_guard<std::mutex> lock(callbackMutex);
  packetCallback = callback;
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
  
  for (DWORD i = 0; ; i++) {
    char subkeyName[256];
    DWORD subkeyLen = sizeof(subkeyName);
    
    if (RegEnumKeyExA(adaptersKey, i, subkeyName, &subkeyLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
      break;
    }
    
    HKEY adapterKey;
    if (RegOpenKeyExA(adaptersKey, subkeyName, 0, KEY_READ, &adapterKey) != ERROR_SUCCESS) {
      continue;
    }
    
    char componentId[256] = {};
    DWORD componentIdLen = sizeof(componentId);
    DWORD type;
    
    if (RegQueryValueExA(adapterKey, "ComponentId", nullptr, &type,
                         reinterpret_cast<LPBYTE>(componentId), &componentIdLen) == ERROR_SUCCESS) {
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
                             reinterpret_cast<LPBYTE>(netCfgInstanceId), &netCfgLen) == ERROR_SUCCESS) {
          TAPDeviceInfo info;
          info.guid = netCfgInstanceId;
          info.componentId = componentId;
          
          // Try to get friendly name from Network Connections
          std::string connectionKey = std::string(NETWORK_CONNECTIONS_KEY) + "\\" + netCfgInstanceId + "\\Connection";
          HKEY connKey;
          if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, connectionKey.c_str(), 0, KEY_READ, &connKey) == ERROR_SUCCESS) {
            char friendlyName[256] = {};
            DWORD nameLen = sizeof(friendlyName);
            if (RegQueryValueExA(connKey, "Name", nullptr, &type,
                                 reinterpret_cast<LPBYTE>(friendlyName), &nameLen) == ERROR_SUCCESS) {
              info.name = friendlyName;
            }
            RegCloseKey(connKey);
          }
          
          devices.push_back(info);
          LOG_INFO(ETH, "TAP Backend: Found device: GUID={}, Name='{}', Driver={}",
                   info.guid, info.name.empty() ? "(unnamed)" : info.name, info.componentId);
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
             tapDevices[0].name.empty() ? selectedGuid : tapDevices[0].name,
             tapDevices[0].componentId);
  } else {
    // Try to find by GUID or name
    for (const auto& dev : tapDevices) {
      if (_stricmp(dev.guid.c_str(), config.deviceName.c_str()) == 0 ||
          _stricmp(dev.name.c_str(), config.deviceName.c_str()) == 0) {
        selectedGuid = dev.guid;
        selectedComponentId = dev.componentId;
        break;
      }
    }
    
    if (selectedGuid.empty()) {
      LOG_ERROR(ETH, "TAP Backend: Device '{}' not found. Available devices:", config.deviceName);
      for (const auto& dev : tapDevices) {
        LOG_ERROR(ETH, "  - {} ({})", dev.guid, dev.name.empty() ? "unnamed" : dev.name);
      }
      return false;
    }
  }
  
  // Determine if this is a classic TAP or WinTun/DCO driver
  // Convert to lowercase for comparison
  std::string componentIdLower = selectedComponentId;
  std::transform(componentIdLower.begin(), componentIdLower.end(), componentIdLower.begin(), ::tolower);
  
  bool isClassicTap = (componentIdLower.find("tap") != std::string::npos);
  bool isWinTun = (componentIdLower.find("wintun") != std::string::npos);
  bool isDCO = (componentIdLower.find("ovpn-dco") != std::string::npos || 
                componentIdLower.find("dco") != std::string::npos);
  
  LOG_INFO(ETH, "TAP Backend: Component ID: '{}', isClassicTap={}, isWinTun={}, isDCO={}", 
           selectedComponentId, isClassicTap, isWinTun, isDCO);
  
  // Try multiple path formats to open the device
  std::vector<std::string> pathFormats = {
    "\\\\.\\Global\\" + selectedGuid + ".tap",
    "\\\\.\\Global\\" + selectedGuid,
    "\\\\.\\tap0901",
    "\\\\.\\tap",
  };
  
  for (const auto& devicePath : pathFormats) {
    LOG_DEBUG(ETH, "TAP Backend: Trying path: {}", devicePath);
    
    tapHandle = CreateFileA(
      devicePath.c_str(),
      GENERIC_READ | GENERIC_WRITE,
      0,
      nullptr,
      OPEN_EXISTING,
      FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED,
      nullptr
    );
    
    if (tapHandle != INVALID_HANDLE_VALUE) {
      LOG_INFO(ETH, "TAP Backend: Successfully opened device with path: {}", devicePath);
      break;
    }
    
    DWORD error = GetLastError();
    LOG_DEBUG(ETH, "TAP Backend: Path '{}' failed with error {}", devicePath, error);
  }
  
  if (tapHandle == INVALID_HANDLE_VALUE) {
    DWORD error = GetLastError();
    LOG_ERROR(ETH, "TAP Backend: Failed to open TAP device. Last error: {}", error);
    
    switch (error) {
    case ERROR_FILE_NOT_FOUND:
      LOG_ERROR(ETH, "TAP Backend: Device not found. The TAP driver may not be properly installed.");
      break;
    case ERROR_ACCESS_DENIED:
      LOG_ERROR(ETH, "TAP Backend: Access denied. Try running as Administrator.");
      break;
    case ERROR_SHARING_VIOLATION:
      LOG_ERROR(ETH, "TAP Backend: Device is in use by another application.");
      break;
    case ERROR_GEN_FAILURE:
      LOG_ERROR(ETH, "TAP Backend: General failure. The TAP adapter may be disabled.");
      break;
    default:
      LOG_ERROR(ETH, "TAP Backend: Unknown error {}.", error);
      break;
    }
    
    return false;
  }
  
  DWORD bytesReturned;
  
  // Get TAP driver version
  ULONG version[3] = {};
  if (DeviceIoControl(tapHandle, TAP_WIN_IOCTL_GET_VERSION, nullptr, 0,
                      version, sizeof(version), &bytesReturned, nullptr)) {
    LOG_INFO(ETH, "TAP Backend: Driver version {}.{}.{}", version[0], version[1], version[2]);
  } else if (DeviceIoControl(tapHandle, TAP_WIN_IOCTL_GET_VERSION_ALT, nullptr, 0,
                             version, sizeof(version), &bytesReturned, nullptr)) {
    LOG_INFO(ETH, "TAP Backend: Driver version {}.{}.{} (alt IOCTL)", version[0], version[1], version[2]);
  }
  
  // Get MAC address
  bool gotMac = false;
  if (DeviceIoControl(tapHandle, TAP_WIN_IOCTL_GET_MAC, nullptr, 0, 
                      macAddress, sizeof(macAddress), &bytesReturned, nullptr)) {
    hasMacAddress = true;
    gotMac = true;
  } else if (DeviceIoControl(tapHandle, TAP_WIN_IOCTL_GET_MAC_ALT, nullptr, 0,
                             macAddress, sizeof(macAddress), &bytesReturned, nullptr)) {
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
  
  // **CRITICAL**: Set media status to connected
  // Try ALL IOCTL variations regardless of detected driver type
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
      if (DeviceIoControl(tapHandle, ioctl, 
                          &mediaStatus, sizeof(mediaStatus), 
                          &mediaStatus, sizeof(mediaStatus), &returned, nullptr)) {
        mediaSet = true;
        LOG_INFO(ETH, "TAP Backend: Media status CONNECTED (IOCTL=0x{:08X})", ioctl);
        break;
      }
      lastErr = GetLastError();
      LOG_DEBUG(ETH, "TAP Backend: IOCTL 0x{:08X} failed, error {}", ioctl, lastErr);
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
  
  // Initialize overlapped structures
  memset(&readOverlapped, 0, sizeof(readOverlapped));
  memset(&writeOverlapped, 0, sizeof(writeOverlapped));
  readOverlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
  writeOverlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
  
  if (!readOverlapped.hEvent || !writeOverlapped.hEvent) {
    LOG_ERROR(ETH, "TAP Backend: Failed to create overlapped events");
    CloseHandle(tapHandle);
    tapHandle = INVALID_HANDLE_VALUE;
    return false;
  }
  
  return true;
}

void TAPBackend::ShutdownPlatform() {
  if (tapHandle != INVALID_HANDLE_VALUE) {
    // Set media status to disconnected
    ULONG mediaStatus = FALSE;
    DWORD bytesReturned;
    DeviceIoControl(tapHandle, TAP_WIN_IOCTL_SET_MEDIA_STATUS,
                    &mediaStatus, sizeof(mediaStatus),
                    &mediaStatus, sizeof(mediaStatus), &bytesReturned, nullptr);
    
    CloseHandle(tapHandle);
    tapHandle = INVALID_HANDLE_VALUE;
  }
  
  if (readOverlapped.hEvent) {
    CloseHandle(readOverlapped.hEvent);
    readOverlapped.hEvent = nullptr;
  }
  
  if (writeOverlapped.hEvent) {
    CloseHandle(writeOverlapped.hEvent);
    writeOverlapped.hEvent = nullptr;
  }
}

bool TAPBackend::SendPacket(const u8* data, u32 length) {
  if (!ready || tapHandle == INVALID_HANDLE_VALUE) {
    stats.txDropped++;
    return false;
  }
  
  if (!data || length == 0 || length > 2048) {
    stats.txErrors++;
    return false;
  }
  
  DWORD bytesWritten = 0;
  ResetEvent(writeOverlapped.hEvent);
  
  if (!WriteFile(tapHandle, data, length, &bytesWritten, &writeOverlapped)) {
    if (GetLastError() == ERROR_IO_PENDING) {
      // Wait for completion (with timeout)
      DWORD result = WaitForSingleObject(writeOverlapped.hEvent, 1000);
      if (result == WAIT_OBJECT_0) {
        GetOverlappedResult(tapHandle, &writeOverlapped, &bytesWritten, FALSE);
      } else {
        CancelIo(tapHandle);
        stats.txErrors++;
        return false;
      }
    } else {
      stats.txErrors++;
      return false;
    }
  }
  
  stats.txPackets++;
  stats.txBytes += bytesWritten;
  return true;
}

void TAPBackend::ReaderThreadLoop() {
  Base::SetCurrentThreadName("[Xe] TAP Reader");
  
  std::vector<u8> buffer(2048);
  
  while (readerRunning && XeRunning) {
    if (tapHandle == INVALID_HANDLE_VALUE) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }
    
    DWORD bytesRead = 0;
    ResetEvent(readOverlapped.hEvent);
    
    if (!ReadFile(tapHandle, buffer.data(), static_cast<DWORD>(buffer.size()), 
                  &bytesRead, &readOverlapped)) {
      if (GetLastError() == ERROR_IO_PENDING) {
        // Wait for data or timeout
        DWORD result = WaitForSingleObject(readOverlapped.hEvent, 100);
        if (result == WAIT_OBJECT_0) {
          GetOverlappedResult(tapHandle, &readOverlapped, &bytesRead, FALSE);
        } else if (result == WAIT_TIMEOUT) {
          continue; // No data, check if we should exit
        } else {
          stats.rxErrors++;
          continue;
        }
      } else {
        stats.rxErrors++;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        continue;
      }
    }
    
    if (bytesRead > 0) {
      stats.rxPackets++;
      stats.rxBytes += bytesRead;
      
      // Call the callback
      std::lock_guard<std::mutex> lock(callbackMutex);
      if (packetCallback) {
        packetCallback(buffer.data(), bytesRead);
      }
    }
  }
}

std::vector<std::string> ListTAPDevices() {
  std::vector<std::string> devices;
  auto tapDevices = FindAllTAPDevices();
  
  for (const auto& dev : tapDevices) {
    devices.push_back(dev.guid);
  }
  
  return devices;
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
  // macOS uses utun devices
  // Note: utun is point-to-point, not TAP. For true TAP, need tuntaposx kext
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
  
  if (connect(tapFd, reinterpret_cast<struct sockaddr*>(&sc), sizeof(sc)) < 0) {
    LOG_ERROR(ETH, "TAP Backend: Failed to connect to utun: {}", strerror(errno));
    close(tapFd);
    tapFd = -1;
    return false;
  }
  
  // Get the device name
  char ifname[IFNAMSIZ];
  socklen_t ifnamelen = sizeof(ifname);
  if (getsockopt(tapFd, SYSPROTO_CONTROL, UTUN_OPT_IFNAME, ifname, &ifnamelen) >= 0) {
    config.deviceName = ifname;
  }
  
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
    if (config.persistentMode) {
      ioctl(tapFd, TUNSETPERSIST, 0);
    }
#endif
    close(tapFd);
    tapFd = -1;
  }
}

bool TAPBackend::SendPacket(const u8* data, u32 length) {
  if (!ready || tapFd < 0) {
    stats.txDropped++;
    return false;
  }
  
  if (!data || length == 0 || length > 2048) {
    stats.txErrors++;
    return false;
  }
  
  ssize_t written = write(tapFd, data, length);
  
  if (written < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      stats.txDropped++;
    } else {
      stats.txErrors++;
    }
    return false;
  }
  
  stats.txPackets++;
  stats.txBytes += written;
  return true;
}

void TAPBackend::ReaderThreadLoop() {
  Base::SetCurrentThreadName("[Xe] TAP Reader");
  
  std::vector<u8> buffer(2048);
  struct pollfd pfd = {};
  pfd.fd = tapFd;
  pfd.events = POLLIN;
  
  while (readerRunning && XeRunning) {
    if (tapFd < 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }
    
    // Poll with timeout
    int ret = poll(&pfd, 1, 100);
    
    if (ret < 0) {
      if (errno != EINTR) {
        stats.rxErrors++;
      }
      continue;
    }
    
    if (ret == 0) {
      // Timeout, no data
      continue;
    }
    
    if (pfd.revents & POLLIN) {
      ssize_t bytesRead = read(tapFd, buffer.data(), buffer.size());
      
      if (bytesRead > 0) {
        stats.rxPackets++;
        stats.rxBytes += bytesRead;
        
        // Call the callback
        std::lock_guard<std::mutex> lock(callbackMutex);
        if (packetCallback) {
          packetCallback(buffer.data(), static_cast<u32>(bytesRead));
        }
      } else if (bytesRead < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        stats.rxErrors++;
      }
    }
    
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
      LOG_ERROR(ETH, "TAP Backend: Poll error on TAP device");
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
}

std::vector<std::string> ListTAPDevices() {
  std::vector<std::string> devices;
  
#ifdef __linux__
  // List TAP devices by checking /sys/class/net/*/tun_flags
  // Or just suggest common names
  devices.push_back("tap0");
  devices.push_back("tap1");
#elif defined(__APPLE__)
  devices.push_back("utun0");
  devices.push_back("utun1");
#endif
  
  return devices;
}

#endif // _WIN32

// Common implementations

TAPConfig ParseTAPConfig(const std::string& configStr) {
  TAPConfig config;
  
  if (configStr.empty()) {
    config.deviceName = "auto";
    return config;
  }
  
  // Parse format: "deviceName[:ipAddress/netmask]"
  size_t colonPos = configStr.find(':');
  
  if (colonPos == std::string::npos) {
    config.deviceName = configStr;
  } else {
    config.deviceName = configStr.substr(0, colonPos);
    std::string ipPart = configStr.substr(colonPos + 1);
    
    size_t slashPos = ipPart.find('/');
    if (slashPos != std::string::npos) {
      config.ipAddress = ipPart.substr(0, slashPos);
      config.netmask = ipPart.substr(slashPos + 1);
    } else {
      config.ipAddress = ipPart;
    }
  }
  
  return config;
}

} // namespace Network
} // namespace Xe
