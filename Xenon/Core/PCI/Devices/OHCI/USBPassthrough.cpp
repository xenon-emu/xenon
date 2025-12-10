/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "USBPassthrough.h"
#include "Base/Logging/Log.h"
#include "Core/RAM/RAM.h"

namespace Xe {
namespace PCIDev {

#ifdef _WIN32

//=============================================================================
// WinUSBDevice Implementation
//=============================================================================

WinUSBDevice::WinUSBDevice()
  : deviceHandle(INVALID_HANDLE_VALUE)
  , winusbHandle(nullptr) {
}

WinUSBDevice::~WinUSBDevice() {
  Close();
}

bool WinUSBDevice::Open(const std::wstring& devicePath) {
  Close();

  // Open device handle
  deviceHandle = CreateFileW(
    devicePath.c_str(),
    GENERIC_READ | GENERIC_WRITE,
    FILE_SHARE_READ | FILE_SHARE_WRITE,
    nullptr,
    OPEN_EXISTING,
    FILE_FLAG_OVERLAPPED,
    nullptr
  );

  if (deviceHandle == INVALID_HANDLE_VALUE) {
    DWORD error = GetLastError();
    LOG_ERROR(OHCI, "Failed to open USB device: error {}", error);
  return false;
  }

  // Initialize WinUSB
  if (!WinUsb_Initialize(deviceHandle, &winusbHandle)) {
    DWORD error = GetLastError();
    LOG_ERROR(OHCI, "WinUsb_Initialize failed: error {}", error);
    CloseHandle(deviceHandle);
    deviceHandle = INVALID_HANDLE_VALUE;
    return false;
  }

  LOG_INFO(OHCI, "USB device opened successfully");
  return true;
}

void WinUSBDevice::Close() {
  // Release all interface handles
  for (auto& handle : interfaceHandles) {
    if (handle != nullptr) {
      WinUsb_Free(handle);
    }
  }
  interfaceHandles.clear();

  // Free WinUSB handle
  if (winusbHandle != nullptr) {
    WinUsb_Free(winusbHandle);
    winusbHandle = nullptr;
  }

  // Close device handle
  if (deviceHandle != INVALID_HANDLE_VALUE) {
    CloseHandle(deviceHandle);
    deviceHandle = INVALID_HANDLE_VALUE;
  }
}

bool WinUSBDevice::ControlTransfer(u8 requestType, u8 request, u16 value,
     u16 index, u8* buffer, u16 length,
   u32& bytesTransferred) {
  if (!IsOpen()) {
    return false;
  }

  WINUSB_SETUP_PACKET setupPacket = {};
  setupPacket.RequestType = requestType;
  setupPacket.Request = request;
  setupPacket.Value = value;
  setupPacket.Index = index;
  setupPacket.Length = length;

  ULONG transferred = 0;
  BOOL result = WinUsb_ControlTransfer(
    winusbHandle,
    setupPacket,
    buffer,
    length,
    &transferred,
    nullptr
  );

  bytesTransferred = transferred;

  if (!result) {
    DWORD error = GetLastError();
    LOG_DEBUG(OHCI, "Control transfer failed: error {}", error);
    return false;
  }

  return true;
}

bool WinUSBDevice::BulkOrInterruptTransfer(u8 pipeId, u8* buffer, u32 length,
       u32& bytesTransferred, bool isRead) {
  if (!IsOpen()) {
    return false;
  }

  // Validate parameters
  if (buffer == nullptr && length > 0) {
 LOG_ERROR(OHCI, "BulkOrInterruptTransfer: null buffer with non-zero length");
    return false;
  }

  // WinUSB requires the pipe ID to have the direction bit set correctly
  // IN endpoints: 0x81-0x8F (bit 7 set)
  // OUT endpoints: 0x01-0x0F (bit 7 clear)
  u8 actualPipeId = pipeId;
  if (isRead) {
    actualPipeId |= 0x80;  // Ensure IN direction bit is set
  } else {
    actualPipeId &= 0x7F;  // Ensure OUT direction bit is clear
  }

  ULONG transferred = 0;
  BOOL result;

  if (isRead) {
    // For zero-length reads, we still need a valid buffer pointer
    result = WinUsb_ReadPipe(
      winusbHandle,
      actualPipeId,
      buffer,
      length,
      &transferred,
    nullptr  // Synchronous transfer
    );
  } else {
    // For writes, length of 0 is valid (zero-length packet)
    result = WinUsb_WritePipe(
      winusbHandle,
      actualPipeId,
      buffer,
      length,
      &transferred,
   nullptr  // Synchronous transfer
    );
  }

  bytesTransferred = transferred;

  if (!result) {
    DWORD error = GetLastError();
    // ERROR_INVALID_PARAMETER (87) often means:
    // - Invalid pipe ID (endpoint doesn't exist)
    // - Buffer alignment issues
    // - Pipe is stalled
  if (error == ERROR_INVALID_PARAMETER) {
      LOG_WARNING(OHCI, "BulkOrInterruptTransfer: Invalid parameter - pipeId=0x{:02X} length={} isRead={}",
 actualPipeId, length, isRead);
    } else if (error == ERROR_SEM_TIMEOUT) {
      LOG_DEBUG(OHCI, "BulkOrInterruptTransfer: Timeout on pipe 0x{:02X}", actualPipeId);
    } else {
      LOG_DEBUG(OHCI, "BulkOrInterruptTransfer failed: error {} on pipe 0x{:02X}", error, actualPipeId);
    }
 return false;
  }

  return true;
}

bool WinUSBDevice::GetDeviceDescriptor(u8* buffer, u32 bufferLength,
           u32& bytesReturned) {
  if (!IsOpen()) {
    return false;
  }

  ULONG lengthTransferred = 0;
  BOOL result = WinUsb_GetDescriptor(
    winusbHandle,
    USB_DEVICE_DESCRIPTOR_TYPE,
    0,
    0,
    buffer,
    bufferLength,
    &lengthTransferred
  );

  bytesReturned = lengthTransferred;
  return result != FALSE;
}

bool WinUSBDevice::GetConfigDescriptor(u8* buffer, u32 bufferLength,
      u32& bytesReturned) {
  if (!IsOpen()) {
    return false;
  }

  ULONG lengthTransferred = 0;
  BOOL result = WinUsb_GetDescriptor(
    winusbHandle,
    USB_CONFIGURATION_DESCRIPTOR_TYPE,
    0,
  0,
    buffer,
    bufferLength,
    &lengthTransferred
  );

  bytesReturned = lengthTransferred;
  return result != FALSE;
}

bool WinUSBDevice::ClaimInterface(u8 interfaceNumber) {
  if (!IsOpen()) {
    return false;
  }

  // For WinUSB, interfaces are claimed automatically when getting the interface handle
  WINUSB_INTERFACE_HANDLE ifaceHandle = nullptr;
  BOOL result = WinUsb_GetAssociatedInterface(
  winusbHandle,
    interfaceNumber,
    &ifaceHandle
  );

  if (result && ifaceHandle != nullptr) {
    interfaceHandles.push_back(ifaceHandle);
    return true;
  }

  return false;
}

bool WinUSBDevice::ReleaseInterface(u8 interfaceNumber) {
  if (interfaceNumber < interfaceHandles.size()) {
    if (interfaceHandles[interfaceNumber] != nullptr) {
      WinUsb_Free(interfaceHandles[interfaceNumber]);
      interfaceHandles[interfaceNumber] = nullptr;
 return true;
    }
  }
  return false;
}

bool WinUSBDevice::GetPipeInfo(u8 interfaceIndex, u8 pipeIndex,
          WINUSB_PIPE_INFORMATION& pipeInfo) {
  if (!IsOpen()) {
    return false;
  }

  WINUSB_INTERFACE_HANDLE handle = winusbHandle;
  if (interfaceIndex > 0 && interfaceIndex <= interfaceHandles.size()) {
    handle = interfaceHandles[interfaceIndex - 1];
  }

  return WinUsb_QueryPipe(handle, 0, pipeIndex, &pipeInfo) != FALSE;
}

bool WinUSBDevice::AbortPipe(u8 pipeId) {
  if (!IsOpen()) {
    return false;
  }
  return WinUsb_AbortPipe(winusbHandle, pipeId) != FALSE;
}

bool WinUSBDevice::ResetPipe(u8 pipeId) {
  if (!IsOpen()) {
    return false;
  }
  return WinUsb_ResetPipe(winusbHandle, pipeId) != FALSE;
}

bool WinUSBDevice::FlushPipe(u8 pipeId) {
  if (!IsOpen()) {
    return false;
  }
  return WinUsb_FlushPipe(winusbHandle, pipeId) != FALSE;
}

bool WinUSBDevice::SetPipePolicy(u8 pipeId, u32 policyType,
               u32 valueLength, void* value) {
  if (!IsOpen()) {
  return false;
  }
  return WinUsb_SetPipePolicy(winusbHandle, pipeId, policyType,
        valueLength, value) != FALSE;
}

#endif // _WIN32

//=============================================================================
// USBPassthroughManager Implementation
//=============================================================================

USBPassthroughManager::USBPassthroughManager()
  : initialized(false) {
}

USBPassthroughManager::~USBPassthroughManager() {
  Shutdown();
}

bool USBPassthroughManager::Initialize() {
  if (initialized) {
    return true;
  }

#ifdef _WIN32
  LOG_INFO(OHCI, "USB Passthrough Manager initialized (WinUSB backend)");
  initialized = true;
  return true;
#else
  LOG_WARNING(OHCI, "USB Passthrough is not yet implemented for Linux!");
  LOG_WARNING(OHCI, "Please contribute libusb support or wait for future updates.");
  initialized = false;
  return false;
#endif
}

void USBPassthroughManager::Shutdown() {
  if (!initialized) {
    return;
  }

#ifdef _WIN32
  std::lock_guard<std::mutex> lock(deviceMutex);
  attachedDevices.clear();
#endif

  initialized = false;
  LOG_INFO(OHCI, "USB Passthrough Manager shutdown");
}

std::vector<USBDeviceInfo> USBPassthroughManager::EnumerateDevices() {
  std::vector<USBDeviceInfo> devices;

#ifdef _WIN32
  // Get device interface class GUID for USB devices
  HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(
  &GUID_DEVINTERFACE_USB_DEVICE,
 nullptr,
    nullptr,
    DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
  );

  if (deviceInfoSet == INVALID_HANDLE_VALUE) {
    LOG_ERROR(OHCI, "Failed to enumerate USB devices");
    return devices;
  }

  SP_DEVICE_INTERFACE_DATA interfaceData = {};
  interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

  for (DWORD index = 0;
       SetupDiEnumDeviceInterfaces(deviceInfoSet, nullptr,
 &GUID_DEVINTERFACE_USB_DEVICE,
        index, &interfaceData);
  index++) {
    // Get required buffer size
    DWORD requiredSize = 0;
    SetupDiGetDeviceInterfaceDetailW(deviceInfoSet, &interfaceData,
     nullptr, 0, &requiredSize, nullptr);

    if (requiredSize == 0) {
      continue;
    }

    // Allocate buffer
    std::vector<u8> detailBuffer(requiredSize);
    auto* detailData = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(detailBuffer.data());
    detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

    SP_DEVINFO_DATA devInfoData = {};
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    if (!SetupDiGetDeviceInterfaceDetailW(deviceInfoSet, &interfaceData,
          detailData, requiredSize,
           nullptr, &devInfoData)) {
      continue;
    }

    // Get device description
    wchar_t descBuffer[256] = {};
    SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, &devInfoData,
     SPDRP_DEVICEDESC, nullptr,
      reinterpret_cast<PBYTE>(descBuffer),
             sizeof(descBuffer), nullptr);

    // Get hardware ID to extract VID/PID
    wchar_t hwIdBuffer[512] = {};
    SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, &devInfoData,
       SPDRP_HARDWAREID, nullptr,
    reinterpret_cast<PBYTE>(hwIdBuffer),
              sizeof(hwIdBuffer), nullptr);

    // Parse VID and PID from hardware ID (format: USB\VID_XXXX&PID_XXXX)
    u16 vid = 0, pid = 0;
    std::wstring hwId(hwIdBuffer);
    size_t vidPos = hwId.find(L"VID_");
    size_t pidPos = hwId.find(L"PID_");

    if (vidPos != std::wstring::npos && pidPos != std::wstring::npos) {
      vid = static_cast<u16>(std::wcstol(hwId.c_str() + vidPos + 4, nullptr, 16));
      pid = static_cast<u16>(std::wcstol(hwId.c_str() + pidPos + 4, nullptr, 16));
    }

    USBDeviceInfo info = {};
    info.vendorId = vid;
    info.productId = pid;

    // Convert wide string to narrow string for description
    size_t descLen = wcslen(descBuffer);
    info.description.resize(descLen);
    for (size_t i = 0; i < descLen; i++) {
      info.description[i] = static_cast<char>(descBuffer[i]);
    }

    info.isConnected = true;
    info.speed = USBSpeed::Full;
    info.deviceAddress = 0;

    devices.push_back(info);
  }

  SetupDiDestroyDeviceInfoList(deviceInfoSet);
#else
  LOG_WARNING(OHCI, "USB device enumeration not implemented for Linux");
#endif

  return devices;
}

bool USBPassthroughManager::AttachDevice(u16 vendorId, u16 productId) {
#ifdef _WIN32
  std::lock_guard<std::mutex> lock(deviceMutex);

  // Check if already attached
  for (const auto& dev : attachedDevices) {
    if (dev.info.vendorId == vendorId && dev.info.productId == productId) {
      LOG_WARNING(OHCI, "Device {:04X}:{:04X} already attached", vendorId, productId);
      return true;
  }
  }

  // Find device paths
  auto paths = FindDevicePaths(vendorId, productId);
  if (paths.empty()) {
    LOG_ERROR(OHCI, "Device {:04X}:{:04X} not found", vendorId, productId);
    return false;
  }

  // Try to open the first matching device
  auto device = std::make_unique<WinUSBDevice>();
  if (!device->Open(paths[0])) {
    LOG_ERROR(OHCI, "Failed to open device {:04X}:{:04X}", vendorId, productId);
    return false;
  }

  AttachedDevice attached;
  attached.device = std::move(device);
  attached.info.vendorId = vendorId;
  attached.info.productId = productId;
  attached.info.isConnected = true;
  attached.info.deviceAddress = 0;  // Will be set by SET_ADDRESS
  attached.devicePath = paths[0];

  attachedDevices.push_back(std::move(attached));

  LOG_INFO(OHCI, "Attached USB device {:04X}:{:04X}", vendorId, productId);
  return true;
#else
  LOG_ERROR(OHCI, "USB passthrough not implemented for Linux");
  return false;
#endif
}

bool USBPassthroughManager::DetachDevice(u16 vendorId, u16 productId) {
#ifdef _WIN32
  std::lock_guard<std::mutex> lock(deviceMutex);

  for (auto it = attachedDevices.begin(); it != attachedDevices.end(); ++it) {
    if (it->info.vendorId == vendorId && it->info.productId == productId) {
      it->device->Close();
      attachedDevices.erase(it);
LOG_INFO(OHCI, "Detached USB device {:04X}:{:04X}", vendorId, productId);
      return true;
    }
  }

  LOG_WARNING(OHCI, "Device {:04X}:{:04X} not found for detach", vendorId, productId);
  return false;
#else
  LOG_ERROR(OHCI, "USB passthrough not implemented for Linux");
return false;
#endif
}

bool USBPassthroughManager::IsDeviceAttached(u8 deviceAddress) const {
#ifdef _WIN32
  std::lock_guard<std::mutex> lock(deviceMutex);

  for (const auto& dev : attachedDevices) {
    if (dev.info.deviceAddress == deviceAddress) {
  return true;
    }
  }
#endif
  return false;
}

USBTransferResult USBPassthroughManager::PerformTransfer(const USBTransferRequest& request) {
  USBTransferResult result = {};
  result.conditionCode = OHCIConditionCode::DeviceNotResponding;
  result.bytesTransferred = 0;

#ifdef _WIN32
  std::lock_guard<std::mutex> lock(deviceMutex);

  // Find the device by address
  // Strategy:
  // 1. First try exact address match
  // 2. If looking for address 0, find any device still at address 0
  // 3. If only one device attached, use it (common case during enumeration)
  AttachedDevice* attachedDev = nullptr;
  
  // Try exact address match first
  for (auto& dev : attachedDevices) {
    if (dev.info.deviceAddress == request.deviceAddress) {
      attachedDev = &dev;
      LOG_DEBUG(OHCI, "Found device {:04X}:{:04X} at exact address {}",
     dev.info.vendorId, dev.info.productId, request.deviceAddress);
      break;
    }
  }
  
  // If not found and request is for address 0, find device at address 0
  if (!attachedDev && request.deviceAddress == 0) {
    for (auto& dev : attachedDevices) {
      if (dev.info.deviceAddress == 0) {
        attachedDev = &dev;
        LOG_DEBUG(OHCI, "Found device {:04X}:{:04X} at address 0 for enumeration",
       dev.info.vendorId, dev.info.productId);
        break;
      }
    }
  }
  
  // Fallback: if only one device is attached, use it
  // This handles the common case where only one USB device is connected
  if (!attachedDev && attachedDevices.size() == 1) {
    attachedDev = &attachedDevices[0];
    LOG_DEBUG(OHCI, "Using single attached device {:04X}:{:04X} (addr {}) for request to addr {}",
              attachedDev->info.vendorId, attachedDev->info.productId,
   attachedDev->info.deviceAddress, request.deviceAddress);
  }

  if (!attachedDev || !attachedDev->device || !attachedDev->device->IsOpen()) {
    LOG_WARNING(OHCI, "No device found for address {} (attached: {})", 
     request.deviceAddress, attachedDevices.size());
    for (const auto& dev : attachedDevices) {
      LOG_DEBUG(OHCI, "  - Device {:04X}:{:04X} at address {}", 
    dev.info.vendorId, dev.info.productId, dev.info.deviceAddress);
    }
    return result;
  }

u8 endpointNum = request.endpointNumber & 0x0F;

  // CRITICAL: Endpoint 0 is ALWAYS the control endpoint
  // Control transfers have 3 phases that come as separate TDs:
  // 1. SETUP phase (directionPID = 0) - contains 8-byte setup packet
  // 2. DATA phase (directionPID = 1 or 2) - optional data transfer
  // 3. STATUS phase (zero-length, opposite direction of data)
  //
  // WinUSB handles all 3 phases in a single WinUsb_ControlTransfer call,
  // so we need to cache the result and serve it across multiple TD phases.
  
bool isControlEndpoint = (endpointNum == 0);

  if (request.isSetup && isControlEndpoint) {
    // SETUP phase - execute the full control transfer via WinUSB
    result = HandleControlSetup(*attachedDev, request);
  } else if (isControlEndpoint) {
    // DATA or STATUS phase on endpoint 0
    if (request.maxLength == 0) {
      // Zero-length transfer = STATUS phase
      result = HandleControlStatus(*attachedDev, request);
    } else {
      // Non-zero length = DATA phase
 result = HandleControlData(*attachedDev, request);
    }
  } else {
    // Bulk or interrupt transfer on endpoint 1-15
    WinUSBDevice* device = attachedDev->device.get();
    u8 pipeId = endpointNum;
    bool isRead = (request.direction == USBDirection::In);
    
    if (isRead) {
      pipeId |= 0x80;  // IN endpoint
    }

    u32 bufferSize = request.maxLength;
    if (bufferSize == 0) {
      bufferSize = 64;
    }
    std::vector<u8> buffer(bufferSize);

    if (!isRead && !request.data.empty()) {
      memcpy(buffer.data(), request.data.data(),
   std::min<size_t>(buffer.size(), request.data.size()));
    }

    u32 transferLength = isRead ? bufferSize : static_cast<u32>(request.data.size());
    u32 bytesTransferred = 0;

    LOG_DEBUG(OHCI, "Bulk/Int transfer: endpoint={} pipeId=0x{:02X} len={} isRead={}",
        endpointNum, pipeId, transferLength, isRead);

    if (device->BulkOrInterruptTransfer(pipeId, buffer.data(),
       transferLength, bytesTransferred, isRead)) {
  result.conditionCode = OHCIConditionCode::NoError;
      result.bytesTransferred = bytesTransferred;

      if (isRead && bytesTransferred > 0) {
   result.data.assign(buffer.begin(), buffer.begin() + bytesTransferred);
      }
    } else {
  result.conditionCode = OHCIConditionCode::Stall;
    }
  }
#else
  LOG_WARNING(OHCI, "USB passthrough not implemented for Linux");
  result.conditionCode = OHCIConditionCode::DeviceNotResponding;
#endif

  return result;
}

#ifdef _WIN32
USBTransferResult USBPassthroughManager::HandleControlSetup(AttachedDevice& dev, const USBTransferRequest& request) {
  USBTransferResult result = {};
  result.conditionCode = OHCIConditionCode::DeviceNotResponding;
  result.bytesTransferred = 0;

  WinUSBDevice* device = dev.device.get();
  
  // Parse setup packet
  u8 bmRequestType = request.setupPacket[0];
  u8 bRequest = request.setupPacket[1];
  u16 wValue = request.setupPacket[2] | (request.setupPacket[3] << 8);
  u16 wIndex = request.setupPacket[4] | (request.setupPacket[5] << 8);
  u16 wLength = request.setupPacket[6] | (request.setupPacket[7] << 8);

  // Determine data direction from bmRequestType
  bool isDeviceToHost = (bmRequestType & 0x80) != 0;

  // USB Standard Request codes
  constexpr u8 kUsbRequestGetStatus = 0x00;
  constexpr u8 kUsbRequestClearFeature = 0x01;
  constexpr u8 kUsbRequestSetFeature = 0x03;
  constexpr u8 kUsbRequestSetAddress = 0x05;
  constexpr u8 kUsbRequestGetDescriptor = 0x06;
  constexpr u8 kUsbRequestSetDescriptor = 0x07;
  constexpr u8 kUsbRequestGetConfiguration = 0x08;
  constexpr u8 kUsbRequestSetConfiguration = 0x09;
  constexpr u8 kUsbRequestGetInterface = 0x0A;
  constexpr u8 kUsbRequestSetInterface = 0x0B;
  constexpr u8 kUsbRequestSynchFrame = 0x0C;
  
  // Descriptor types (high byte of wValue for GET_DESCRIPTOR)
  u8 descriptorType = (wValue >> 8) & 0xFF;
  u8 descriptorIndex = wValue & 0xFF;

  // Log the request
  const char* reqName = "Unknown";
  switch (bRequest) {
    case kUsbRequestGetStatus: reqName = "GET_STATUS"; break;
    case kUsbRequestClearFeature: reqName = "CLEAR_FEATURE"; break;
    case kUsbRequestSetFeature: reqName = "SET_FEATURE"; break;
    case kUsbRequestSetAddress: reqName = "SET_ADDRESS"; break;
    case kUsbRequestGetDescriptor: reqName = "GET_DESCRIPTOR"; break;
    case kUsbRequestSetDescriptor: reqName = "SET_DESCRIPTOR"; break;
    case kUsbRequestGetConfiguration: reqName = "GET_CONFIGURATION"; break;
    case kUsbRequestSetConfiguration: reqName = "SET_CONFIGURATION"; break;
 case kUsbRequestGetInterface: reqName = "GET_INTERFACE"; break;
    case kUsbRequestSetInterface: reqName = "SET_INTERFACE"; break;
    case kUsbRequestSynchFrame: reqName = "SYNCH_FRAME"; break;
  }
  
  LOG_INFO(OHCI, "Control SETUP: {} (0x{:02X}) bmReqType=0x{:02X} wVal=0x{:04X} wIdx=0x{:04X} wLen={}",
      reqName, bRequest, bmRequestType, wValue, wIndex, wLength);

  // Reset pending state
  dev.pendingControlTransfer = {};
  dev.hasPendingControl = true;
  memcpy(dev.pendingControlTransfer.setupPacket, request.setupPacket, 8);
  dev.pendingControlTransfer.wLength = wLength;
  dev.pendingControlTransfer.dataDirection = isDeviceToHost ? USBDirection::In : USBDirection::Out;

  // =========================================================================
  // Special handling for SET_ADDRESS
  // WinUSB doesn't allow us to change the device address - Windows manages it
  // We just track what address the guest OS assigns to the device
  // =========================================================================
  if (bRequest == kUsbRequestSetAddress && bmRequestType == 0x00) {
    u8 newAddress = wValue & 0x7F;
    LOG_INFO(OHCI, "SET_ADDRESS: Device {:04X}:{:04X} assigned address {} (was {})",
             dev.info.vendorId, dev.info.productId, newAddress, dev.info.deviceAddress);
 
dev.info.deviceAddress = newAddress;
    
    result.conditionCode = OHCIConditionCode::NoError;
    result.bytesTransferred = 0;
    dev.pendingControlTransfer.setupCompleted = true;
    dev.pendingControlTransfer.dataCompleted = true;
    
    return result;
  }
  
  // =========================================================================
  // Special handling for SET_CONFIGURATION
  // =========================================================================
  if (bRequest == kUsbRequestSetConfiguration && bmRequestType == 0x00) {
    LOG_INFO(OHCI, "SET_CONFIGURATION: config={} (acknowledged)", wValue & 0xFF);
    
    result.conditionCode = OHCIConditionCode::NoError;
    result.bytesTransferred = 0;
    dev.pendingControlTransfer.setupCompleted = true;
    dev.pendingControlTransfer.dataCompleted = true;
    
    return result;
  }

  // =========================================================================
  // Special handling for SET_INTERFACE  
  // =========================================================================
  if (bRequest == kUsbRequestSetInterface && bmRequestType == 0x01) {
    LOG_INFO(OHCI, "SET_INTERFACE: interface={} altSetting={} (acknowledged)", wIndex, wValue);
    
    result.conditionCode = OHCIConditionCode::NoError;
    result.bytesTransferred = 0;
    dev.pendingControlTransfer.setupCompleted = true;
    dev.pendingControlTransfer.dataCompleted = true;
    
    return result;
  }

  // =========================================================================
  // Special handling for GET_CONFIGURATION
  // Return configuration value 1 (device is configured)
  // =========================================================================
  if (bRequest == kUsbRequestGetConfiguration && bmRequestType == 0x80) {
    LOG_INFO(OHCI, "GET_CONFIGURATION: returning config=1");
    
    dev.pendingControlTransfer.hasData = true;
  dev.pendingControlTransfer.data = {1};  // Configuration 1
    dev.pendingControlTransfer.dataOffset = 0;
    dev.pendingControlTransfer.setupCompleted = true;
    
    result.conditionCode = OHCIConditionCode::NoError;
    result.bytesTransferred = 0;
    
    return result;
  }

  // =========================================================================
  // Special handling for CLEAR_FEATURE (endpoint halt)
  // =========================================================================
  if (bRequest == kUsbRequestClearFeature && (bmRequestType == 0x00 || bmRequestType == 0x02)) {
    LOG_INFO(OHCI, "CLEAR_FEATURE: feature={} index={} (acknowledged)", wValue, wIndex);
    
    // If clearing endpoint halt, reset the pipe
if (wValue == 0 && bmRequestType == 0x02) {  // ENDPOINT_HALT
      u8 endpointAddr = wIndex & 0xFF;
      device->ResetPipe(endpointAddr);
    }
    
    result.conditionCode = OHCIConditionCode::NoError;
    result.bytesTransferred = 0;
    dev.pendingControlTransfer.setupCompleted = true;
    dev.pendingControlTransfer.dataCompleted = true;
    
    return result;
  }
  
  // Log descriptor requests for debugging
  if (bRequest == kUsbRequestGetDescriptor) {
    const char* descName = "Unknown";
  switch (descriptorType) {
      case 1: descName = "Device"; break;
   case 2: descName = "Configuration"; break;
      case 3: descName = "String"; break;
      case 4: descName = "Interface"; break;
      case 5: descName = "Endpoint"; break;
      case 6: descName = "DeviceQualifier"; break;
      case 7: descName = "OtherSpeedConfig"; break;
      case 0x21: descName = "HID"; break;
      case 0x22: descName = "Report"; break;
    }
    LOG_INFO(OHCI, "GET_DESCRIPTOR: Type={} ({}) Index={} Length={}",
  descName, descriptorType, descriptorIndex, wLength);
  }

  // If wLength == 0, this is a no-data control transfer
  if (wLength == 0) {
    u32 bytesTransferred = 0;
    if (device->ControlTransfer(bmRequestType, bRequest, wValue, wIndex,
 nullptr, 0, bytesTransferred)) {
   result.conditionCode = OHCIConditionCode::NoError;
      result.bytesTransferred = 0;
      dev.pendingControlTransfer.setupCompleted = true;
      dev.pendingControlTransfer.dataCompleted = true;
      LOG_DEBUG(OHCI, "Control no-data transfer completed");
    } else {
      result.conditionCode = OHCIConditionCode::Stall;
      LOG_WARNING(OHCI, "Control no-data transfer failed");
    }
    return result;
  }

  // For transfers with data, execute the full control transfer now
  std::vector<u8> buffer(wLength);
  
  if (isDeviceToHost) {
    // IN transfer: execute now, cache result for DATA IN phase
    u32 bytesTransferred = 0;
    if (device->ControlTransfer(bmRequestType, bRequest, wValue, wIndex,
    buffer.data(), wLength, bytesTransferred)) {
      dev.pendingControlTransfer.hasData = true;
   dev.pendingControlTransfer.data.assign(buffer.begin(), buffer.begin() + bytesTransferred);
      dev.pendingControlTransfer.dataOffset = 0;
      dev.pendingControlTransfer.setupCompleted = true;
      result.conditionCode = OHCIConditionCode::NoError;
      result.bytesTransferred = 0;
      
    LOG_DEBUG(OHCI, "Control IN completed, cached {} bytes", bytesTransferred);
      
      // Log descriptor data for debugging
      if (bRequest == kUsbRequestGetDescriptor && bytesTransferred > 0) {
        std::string hexDump;
        for (u32 i = 0; i < std::min(bytesTransferred, 32u); i++) {
          char hex[4];
          snprintf(hex, sizeof(hex), "%02X ", buffer[i]);
          hexDump += hex;
     }
  LOG_DEBUG(OHCI, "Data: {}", hexDump);
      }
    } else {
    result.conditionCode = OHCIConditionCode::Stall;
    LOG_WARNING(OHCI, "Control IN transfer failed for {} (0x{:02X})", reqName, bRequest);
    }
  } else {
    // OUT transfer: wait for DATA OUT phase
    dev.pendingControlTransfer.setupCompleted = true;
    result.conditionCode = OHCIConditionCode::NoError;
    result.bytesTransferred = 0;
    LOG_DEBUG(OHCI, "Control OUT SETUP complete, waiting for DATA OUT");
  }

  return result;
}

USBTransferResult USBPassthroughManager::HandleControlData(AttachedDevice& dev, const USBTransferRequest& request) {
  USBTransferResult result = {};
  result.conditionCode = OHCIConditionCode::DeviceNotResponding;
  result.bytesTransferred = 0;

  if (!dev.hasPendingControl) {
    LOG_WARNING(OHCI, "DATA phase without pending SETUP");
    result.conditionCode = OHCIConditionCode::NoError;
    result.bytesTransferred = 0;
  return result;
  }

  PendingControlTransfer& pending = dev.pendingControlTransfer;

  if (!pending.setupCompleted) {
    LOG_WARNING(OHCI, "DATA phase before SETUP completed");
    result.conditionCode = OHCIConditionCode::NoError;
    result.bytesTransferred = 0;
    return result;
  }

  if (request.direction == USBDirection::In) {
    // DATA IN phase - return cached data from SETUP phase
    if (pending.hasData && !pending.data.empty()) {
      u32 available = static_cast<u32>(pending.data.size()) - pending.dataOffset;
      u32 toTransfer = std::min(request.maxLength, available);
      
      if (toTransfer > 0) {
        result.data.assign(pending.data.begin() + pending.dataOffset,
            pending.data.begin() + pending.dataOffset + toTransfer);
        pending.dataOffset += toTransfer;
      }
      
   result.conditionCode = OHCIConditionCode::NoError;
      result.bytesTransferred = toTransfer;

      LOG_DEBUG(OHCI, "Control DATA IN: returned {} bytes (offset {}/{})",
    toTransfer, pending.dataOffset, pending.data.size());
      
    if (pending.dataOffset >= pending.data.size()) {
      pending.dataCompleted = true;
 }
    } else {
      LOG_WARNING(OHCI, "Control DATA IN but no cached data");
      result.conditionCode = OHCIConditionCode::NoError;
      result.bytesTransferred = 0;
      pending.dataCompleted = true;
    }
  } else {
    // DATA OUT phase - execute control transfer with provided data
    WinUSBDevice* device = dev.device.get();
    
  u8 bmRequestType = pending.setupPacket[0];
    u8 bRequest = pending.setupPacket[1];
    u16 wValue = pending.setupPacket[2] | (pending.setupPacket[3] << 8);
    u16 wIndex = pending.setupPacket[4] | (pending.setupPacket[5] << 8);
    u16 wLength = pending.wLength;

    std::vector<u8> buffer(wLength);
    u32 dataLen = std::min(static_cast<u32>(request.data.size()), static_cast<u32>(wLength));
    if (!request.data.empty()) {
      memcpy(buffer.data(), request.data.data(), dataLen);
    }

    u32 bytesTransferred = 0;
    if (device->ControlTransfer(bmRequestType, bRequest, wValue, wIndex,
         buffer.data(), static_cast<u16>(dataLen), bytesTransferred)) {
result.conditionCode = OHCIConditionCode::NoError;
      result.bytesTransferred = bytesTransferred;
      pending.dataCompleted = true;
   LOG_DEBUG(OHCI, "Control DATA OUT: sent {} bytes", bytesTransferred);
    } else {
      result.conditionCode = OHCIConditionCode::Stall;
      LOG_DEBUG(OHCI, "Control DATA OUT failed");
    }
  }

  return result;
}

USBTransferResult USBPassthroughManager::HandleControlStatus(AttachedDevice& dev, const USBTransferRequest& request) {
  USBTransferResult result = {};
  
  if (dev.hasPendingControl) {
    LOG_DEBUG(OHCI, "Control STATUS phase: setupDone={} dataDone={}",
         dev.pendingControlTransfer.setupCompleted, 
      dev.pendingControlTransfer.dataCompleted);
    
    // Clear pending state
    dev.hasPendingControl = false;
    dev.pendingControlTransfer = {};
  } else {
    LOG_DEBUG(OHCI, "Control STATUS phase (no pending state)");
  }

  result.conditionCode = OHCIConditionCode::NoError;
  result.bytesTransferred = 0;

  return result;
}
#endif // _WIN32

std::vector<USBDeviceInfo> USBPassthroughManager::GetAttachedDevices() const {
  std::vector<USBDeviceInfo> devices;

#ifdef _WIN32
  std::lock_guard<std::mutex> lock(deviceMutex);

  for (const auto& dev : attachedDevices) {
    devices.push_back(dev.info);
  }
#endif

  return devices;
}

void USBPassthroughManager::SetDeviceAddress(u16 vendorId, u16 productId, u8 address) {
#ifdef _WIN32
  std::lock_guard<std::mutex> lock(deviceMutex);

  for (auto& dev : attachedDevices) {
    if (dev.info.vendorId == vendorId && dev.info.productId == productId) {
      dev.info.deviceAddress = address;
      LOG_INFO(OHCI, "Set device {:04X}:{:04X} address to {}",
 vendorId, productId, address);
  return;
    }
  }
#endif
}

#ifdef _WIN32
std::vector<std::wstring> USBPassthroughManager::FindDevicePaths(u16 vendorId, u16 productId) {
  std::vector<std::wstring> paths;

  HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(
    &GUID_DEVINTERFACE_USB_DEVICE,
    nullptr,
    nullptr,
    DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
  );

  if (deviceInfoSet == INVALID_HANDLE_VALUE) {
 return paths;
  }

  SP_DEVICE_INTERFACE_DATA interfaceData = {};
  interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

  for (DWORD index = 0;
       SetupDiEnumDeviceInterfaces(deviceInfoSet, nullptr,
       &GUID_DEVINTERFACE_USB_DEVICE,
    index, &interfaceData);
       index++) {
    DWORD requiredSize = 0;
    SetupDiGetDeviceInterfaceDetailW(deviceInfoSet, &interfaceData,
   nullptr, 0, &requiredSize, nullptr);

    if (requiredSize == 0) {
      continue;
    }

std::vector<u8> detailBuffer(requiredSize);
    auto* detailData = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(detailBuffer.data());
  detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

    SP_DEVINFO_DATA devInfoData = {};
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    if (!SetupDiGetDeviceInterfaceDetailW(deviceInfoSet, &interfaceData,
     detailData, requiredSize,
    nullptr, &devInfoData)) {
 continue;
    }

    // Get hardware ID
    wchar_t hwIdBuffer[512] = {};
    SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, &devInfoData,
   SPDRP_HARDWAREID, nullptr,
       reinterpret_cast<PBYTE>(hwIdBuffer),
  sizeof(hwIdBuffer), nullptr);

    // Parse VID and PID
    std::wstring hwId(hwIdBuffer);
    size_t vidPos = hwId.find(L"VID_");
  size_t pidPos = hwId.find(L"PID_");

    if (vidPos != std::wstring::npos && pidPos != std::wstring::npos) {
      u16 vid = static_cast<u16>(std::wcstol(hwId.c_str() + vidPos + 4, nullptr, 16));
      u16 pid = static_cast<u16>(std::wcstol(hwId.c_str() + pidPos + 4, nullptr, 16));

      if (vid == vendorId && pid == productId) {
     paths.push_back(detailData->DevicePath);
      }
 }
  }

  SetupDiDestroyDeviceInfoList(deviceInfoSet);
  return paths;
}
#endif

//=============================================================================
// OHCITransferProcessor Implementation
//=============================================================================

OHCITransferProcessor::OHCITransferProcessor()
  : ramPtr(nullptr)
  , passthroughManager(nullptr)
  , doneHead(0) {
}

OHCITransferProcessor::~OHCITransferProcessor() = default;

void OHCITransferProcessor::SetRAM(RAM* ram) {
  ramPtr = ram;
}

void OHCITransferProcessor::SetPassthroughManager(USBPassthroughManager* manager) {
  passthroughManager = manager;
}

u8* OHCITransferProcessor::GetRAMPointer(u32 address) {
  if (!ramPtr || address == 0) {
    return nullptr;
  }
  return ramPtr->GetPointerToAddress(address);
}

void OHCITransferProcessor::ProcessControlList(u32 headED) {
  if (headED == 0 || !ramPtr) {
  return;
  }

  u32 currentED = headED;
  u32 maxEDs = 256;

  while (currentED != 0 && maxEDs-- > 0) {
    OHCIEndpointDescriptor ed = ReadED(currentED);

    if (!ed.skip) {
      if (ProcessED(ed, currentED)) {
        WriteED(currentED, ed);
      }
    }

    currentED = ed.nextED & ~0xF;
  }
}

void OHCITransferProcessor::ProcessBulkList(u32 headED) {
  if (headED == 0 || !ramPtr) {
    return;
  }

  u32 currentED = headED;
  u32 maxEDs = 256;

  while (currentED != 0 && maxEDs-- > 0) {
    OHCIEndpointDescriptor ed = ReadED(currentED);

    if (!ed.skip) {
      if (ProcessED(ed, currentED)) {
      WriteED(currentED, ed);
      }
    }

    currentED = ed.nextED & ~0xF;
  }
}

void OHCITransferProcessor::ProcessPeriodicList(u32 edAddress) {
  if (edAddress == 0 || !ramPtr) {
    return;
  }

  u32 currentED = edAddress;
u32 maxEDs = 256;

  while (currentED != 0 && maxEDs-- > 0) {
    OHCIEndpointDescriptor ed = ReadED(currentED);

    if (!ed.skip && !ed.format) {
      if (ProcessED(ed, currentED)) {
  WriteED(currentED, ed);
      }
 }

    currentED = ed.nextED & ~0xF;
  }
}

void OHCITransferProcessor::ProcessIsochronousList(u32 edAddress) {
  if (edAddress == 0 || !ramPtr) {
    return;
  }

  u32 currentED = edAddress;
  u32 maxEDs = 256;

  while (currentED != 0 && maxEDs-- > 0) {
    OHCIEndpointDescriptor ed = ReadED(currentED);

    if (!ed.skip && ed.format) {  // Isochronous TD format
  u32 headPtr = (ed.headPointer & ~0xF);
      u32 tailPtr = ed.tailPointer;

   if (headPtr != tailPtr && !ed.halted) {
        OHCIIsochronousTD td = ReadIsochronousTD(headPtr);
     OHCIConditionCode cc = ProcessIsochronousTD(td, headPtr, ed);

     if (cc != OHCIConditionCode::NotAccessed) {
    // Update TD condition code
  td.conditionCode = static_cast<u32>(cc);
    WriteIsochronousTD(headPtr, td);

// Move head pointer to next TD
   ed.headPointer = (td.nextTD & ~0xF) | (ed.headPointer & 0xF);
        WriteED(currentED, ed);

     // Add to done queue
   AddToDoneQueue(headPtr);
     }
   }
    }

    currentED = ed.nextED & ~0xF;
  }
}

bool OHCITransferProcessor::ProcessED(OHCIEndpointDescriptor& ed, u32 edAddress) {
  // Get head and tail pointers
  u32 headPtr = (ed.headPointer & ~0xF);
  u32 tailPtr = ed.tailPointer;

  // Check if queue is empty or halted
  if (headPtr == tailPtr || ed.halted) {
    return false;
  }

  bool modified = false;
  u32 maxTDs = 64;  // Prevent infinite loops

  while (headPtr != tailPtr && !ed.halted && maxTDs-- > 0) {
    OHCIGeneralTD td = ReadGeneralTD(headPtr);
    OHCIConditionCode cc = ProcessGeneralTD(td, headPtr, ed);

    if (cc == OHCIConditionCode::NotAccessed) {
      // TD not ready, skip for now
      break;
    }

    // Update TD condition code
 td.conditionCode = static_cast<u32>(cc);
    WriteGeneralTD(headPtr, td);

    // Check for errors that should halt the endpoint
    if (cc != OHCIConditionCode::NoError &&
        cc != OHCIConditionCode::DataUnderrun) {
      ed.halted = 1;
      modified = true;
    }

    // Add to done queue
    AddToDoneQueue(headPtr);

    // Move head pointer to next TD
    u32 nextTD = td.nextTD & ~0xF;
    ed.headPointer = nextTD | (ed.headPointer & 0x3);  // Preserve halted and toggleCarry
    headPtr = nextTD;
  modified = true;

  // Update data toggle
    ed.toggleCarry = (td.dataToggle >> 1) ^ 1;
  }

  return modified;
}

OHCIConditionCode OHCITransferProcessor::ProcessGeneralTD(OHCIGeneralTD& td,
      u32 tdAddress,
   const OHCIEndpointDescriptor& ed) {
  // Check if TD has already been processed
  if (td.conditionCode != static_cast<u32>(OHCIConditionCode::NotAccessed) &&
      td.conditionCode != static_cast<u32>(OHCIConditionCode::NotAccessed2)) {
    return static_cast<OHCIConditionCode>(td.conditionCode);
  }

  // Determine transfer direction
  USBDirection direction;
  if (td.directionPID == 0) {
    direction = USBDirection::Setup;
  } else if (td.directionPID == 1) {
    direction = USBDirection::Out;
  } else if (td.directionPID == 2) {
    direction = USBDirection::In;
  } else {
    // Get direction from ED
    direction = static_cast<USBDirection>(ed.direction);
  }

  // Calculate buffer size
  u32 bufferStart = td.currentBufferPointer;
  u32 bufferEnd = td.bufferEnd;
  u32 bufferSize = 0;

  if (bufferStart != 0) {
    if ((bufferStart & 0xFFFFF000) == (bufferEnd & 0xFFFFF000)) {
      // Same page
 bufferSize = bufferEnd - bufferStart + 1;
    } else {
      // Crosses page boundary
      bufferSize = (0x1000 - (bufferStart & 0xFFF)) + (bufferEnd & 0xFFF) + 1;
    }
  }

  // Check if we have a passthrough manager
  if (!passthroughManager) {
    LOG_DEBUG(OHCI, "No passthrough manager, completing TD with error");
    return OHCIConditionCode::DeviceNotResponding;
  }

  // Prepare transfer request
  USBTransferRequest request = {};
  request.deviceAddress = ed.functionAddress;
  request.endpointNumber = ed.endpointNumber;
  request.direction = direction;
  request.maxLength = bufferSize;

  // IMPORTANT: Endpoint 0 is ALWAYS the control endpoint
  // Control transfers have 3 phases: SETUP, DATA (optional), STATUS
  // - SETUP phase: directionPID == 0, contains 8-byte setup packet
  // - DATA phase: directionPID == 1 (OUT) or 2 (IN)
  // - STATUS phase: typically zero-length, opposite direction of data phase
  bool isControlEndpoint = (ed.endpointNumber == 0);

  if (direction == USBDirection::Setup) {
    // SETUP phase - this is always a control transfer
request.isSetup = true;
    if (bufferSize >= 8 && bufferStart != 0) {
      auto setupData = ReadBuffer(bufferStart, bufferStart + 7);
      if (setupData.size() >= 8) {
        memcpy(request.setupPacket, setupData.data(), 8);
      }
    }
    LOG_DEBUG(OHCI, "Control SETUP phase: addr={} ep={} bufSize={}",
         ed.functionAddress, ed.endpointNumber, bufferSize);
  } else if (isControlEndpoint) {
    // DATA or STATUS phase on endpoint 0
    // These don't have a setup packet, but must still go through control path
    request.isSetup = false;
 
    if (direction == USBDirection::Out && bufferSize > 0) {
    request.data = ReadBuffer(bufferStart, bufferEnd);
  }
    
    LOG_DEBUG(OHCI, "Control {} phase on EP0: addr={} bufSize={}",
       direction == USBDirection::In ? "DATA IN" : 
            (bufferSize == 0 ? "STATUS" : "DATA OUT"),
  ed.functionAddress, bufferSize);
  } else {
    // Non-control endpoint (bulk or interrupt) - endpoints 1-15
    request.isSetup = false;

  if (direction == USBDirection::Out && bufferSize > 0) {
      request.data = ReadBuffer(bufferStart, bufferEnd);
    }

    LOG_DEBUG(OHCI, "Bulk/Int transfer: addr={} ep={} dir={} bufSize={}",
     ed.functionAddress, ed.endpointNumber,
    direction == USBDirection::In ? "IN" : "OUT", bufferSize);
  }

  // Perform the transfer
  USBTransferResult result = passthroughManager->PerformTransfer(request);

// For IN transfers, write data to buffer
  if (direction == USBDirection::In && result.conditionCode == OHCIConditionCode::NoError) {
    if (!result.data.empty() && bufferStart != 0) {
      WriteBuffer(bufferStart, result.data);
    }

    // Update current buffer pointer
    if (result.bytesTransferred > 0) {
      td.currentBufferPointer = bufferStart + result.bytesTransferred;
      if (td.currentBufferPointer > bufferEnd) {
        td.currentBufferPointer = 0;  // Transfer complete
      }
    }
  }

  // Handle short packet
  if (result.bytesTransferred < bufferSize && !td.bufferRounding) {
    if (result.conditionCode == OHCIConditionCode::NoError) {
      result.conditionCode = OHCIConditionCode::DataUnderrun;
    }
  }

  return result.conditionCode;
}

OHCIConditionCode OHCITransferProcessor::ProcessIsochronousTD(OHCIIsochronousTD& td,
          u32 tdAddress,
   const OHCIEndpointDescriptor& ed) {
  // Isochronous transfers are more complex and timing-sensitive
  // For now, return not accessed to skip
  LOG_DEBUG(OHCI, "Isochronous TD processing not fully implemented");
  return OHCIConditionCode::NotAccessed;
}

u32 OHCITransferProcessor::GetDoneHead() {
  std::lock_guard<std::mutex> lock(doneQueueMutex);
  return doneHead;
}

void OHCITransferProcessor::ClearDoneHead() {
  std::lock_guard<std::mutex> lock(doneQueueMutex);
  doneHead = 0;
}

void OHCITransferProcessor::AddToDoneQueue(u32 tdAddress) {
  std::lock_guard<std::mutex> lock(doneQueueMutex);

  // Read the TD to update its next pointer
  OHCIGeneralTD td = ReadGeneralTD(tdAddress);
  td.nextTD = doneHead;
  WriteGeneralTD(tdAddress, td);

  doneHead = tdAddress;
}

OHCIEndpointDescriptor OHCITransferProcessor::ReadED(u32 address) {
  OHCIEndpointDescriptor ed = {};
  u8* ptr = GetRAMPointer(address);
  if (ptr) { memcpy(&ed, ptr, sizeof(OHCIEndpointDescriptor)); }
  return ed;
}

void OHCITransferProcessor::WriteED(u32 address, const OHCIEndpointDescriptor& ed) {
  u8* ptr = GetRAMPointer(address);
  if (ptr) { memcpy(ptr, &ed, sizeof(OHCIEndpointDescriptor)); }
}

OHCIGeneralTD OHCITransferProcessor::ReadGeneralTD(u32 address) {
  OHCIGeneralTD td = {};
  u8* ptr = GetRAMPointer(address);
  if (ptr) { memcpy(&td, ptr, sizeof(OHCIGeneralTD)); }
  return td;
}

void OHCITransferProcessor::WriteGeneralTD(u32 address, const OHCIGeneralTD& td) {
  u8* ptr = GetRAMPointer(address);
  if (ptr) { memcpy(ptr, &td, sizeof(OHCIGeneralTD)); }
}

OHCIIsochronousTD OHCITransferProcessor::ReadIsochronousTD(u32 address) {
OHCIIsochronousTD td = {};
  u8* ptr = GetRAMPointer(address);
  if (ptr) { memcpy(&td, ptr, sizeof(OHCIIsochronousTD)); }
  return td;
}

void OHCITransferProcessor::WriteIsochronousTD(u32 address, const OHCIIsochronousTD& td) {
  u8* ptr = GetRAMPointer(address);
  if (ptr) { memcpy(ptr, &td, sizeof(OHCIIsochronousTD)); }
}

std::vector<u8> OHCITransferProcessor::ReadBuffer(u32 start, u32 end) {
  std::vector<u8> buffer;

  if (!ramPtr || start == 0 || start > end) {
    return buffer;
  }

  // Check if buffer crosses page boundary
  if ((start & 0xFFFFF000) == (end & 0xFFFFF000)) {
    // Same page, simple case
    u32 size = end - start + 1;
    u8* ptr = GetRAMPointer(start);
    if (ptr) {
      buffer.resize(size);
      memcpy(buffer.data(), ptr, size);
    }
  } else {
    // Crosses page boundary
    u32 firstPageEnd = (start | 0xFFF);
    u32 firstSize = firstPageEnd - start + 1;
    u32 secondStart = (end & 0xFFFFF000);
    u32 secondSize = (end & 0xFFF) + 1;

    buffer.resize(firstSize + secondSize);

    u8* ptr1 = GetRAMPointer(start);
    if (ptr1) {
      memcpy(buffer.data(), ptr1, firstSize);
    }

    u8* ptr2 = GetRAMPointer(secondStart);
    if (ptr2) {
    memcpy(buffer.data() + firstSize, ptr2, secondSize);
    }
  }

  return buffer;
}

void OHCITransferProcessor::WriteBuffer(u32 start, const std::vector<u8>& data) {
  if (!ramPtr || start == 0 || data.empty()) {
    return;
  }

  u32 end = start + static_cast<u32>(data.size()) - 1;

  // Check if buffer crosses page boundary
  if ((start & 0xFFFFF000) == (end & 0xFFFFF000)) {
    // Same page
    u8* ptr = GetRAMPointer(start);
    if (ptr) {
      memcpy(ptr, data.data(), data.size());
    }
  } else {
    // Crosses page boundary
    u32 firstPageEnd = (start | 0xFFF);
    u32 firstSize = firstPageEnd - start + 1;
    u32 secondStart = (end & 0xFFFFF000);
    u32 secondSize = static_cast<u32>(data.size()) - firstSize;

    u8* ptr1 = GetRAMPointer(start);
    if (ptr1) {
      memcpy(ptr1, data.data(), firstSize);
 }

    u8* ptr2 = GetRAMPointer(secondStart);
    if (ptr2) {
      memcpy(ptr2, data.data() + firstSize, secondSize);
    }
  }
}

} // namespace PCIDev
} // namespace Xe
