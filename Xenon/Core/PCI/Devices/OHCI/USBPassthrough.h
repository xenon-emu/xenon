/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"

#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <map>

#ifdef _WIN32
#include <Windows.h>
#include <winusb.h>
#include <SetupAPI.h>
#include <initguid.h>
#include <usbiodef.h>
#pragma comment(lib, "winusb.lib")
#pragma comment(lib, "setupapi.lib")
#else
// Linux stubs - to be implemented
#endif

// Forward declaration
class RAM;

namespace Xe {
namespace PCIDev {

//
// OHCI Endpoint Descriptor (ED) - 16 bytes
//
#pragma pack(push, 1)
struct OHCIEndpointDescriptor {
  union {
    u32 control;
    struct {
#ifdef __LITTLE_ENDIAN__
      u32 functionAddress : 7;    // Device address
    u32 endpointNumber : 4;     // Endpoint number
      u32 direction : 2;          // 0=TD, 1=OUT, 2=IN, 3=TD
    u32 speed : 1;     // 0=Full speed, 1=Low speed
      u32 skip : 1;         // Skip this ED
      u32 format : 1;   // 0=General TD, 1=Isochronous TD
      u32 maxPacketSize : 11;     // Maximum packet size
      u32 reserved : 5;
#else
      u32 reserved : 5;
      u32 maxPacketSize : 11;
      u32 format : 1;
      u32 skip : 1;
      u32 speed : 1;
      u32 direction : 2;
      u32 endpointNumber : 4;
      u32 functionAddress : 7;
#endif
    };
  };
u32 tailPointer;    // TD queue tail pointer (physical address)
  union {
    u32 headPointer;  // TD queue head pointer (physical address)
 struct {
#ifdef __LITTLE_ENDIAN__
      u32 halted : 1;    // Halted flag
      u32 toggleCarry : 1;      // Data toggle carry
      u32 headPointerAddr : 30; // Actual head pointer (aligned)
#else
      u32 headPointerAddr : 30;
      u32 toggleCarry : 1;
      u32 halted : 1;
#endif
    };
  };
  u32 nextED;         // Next ED in list (physical address)
};
#pragma pack(pop)

//
// OHCI General Transfer Descriptor (TD) - 16 bytes
//
#pragma pack(push, 1)
struct OHCIGeneralTD {
  union {
    u32 control;
    struct {
#ifdef __LITTLE_ENDIAN__
      u32 reserved0 : 18;
      u32 bufferRounding : 1;   // Allow short packets
      u32 directionPID : 2;   // 0=SETUP, 1=OUT, 2=IN, 3=Reserved
      u32 delayInterrupt : 3;   // Delay interrupt (frames)
      u32 dataToggle : 2;       // Data toggle
      u32 errorCount : 2;     // Error counter
      u32 conditionCode : 4;    // Condition code
#else
      u32 conditionCode : 4;
      u32 errorCount : 2;
      u32 dataToggle : 2;
      u32 delayInterrupt : 3;
      u32 directionPID : 2;
      u32 bufferRounding : 1;
   u32 reserved0 : 18;
#endif
    };
  };
  u32 currentBufferPointer;   // Current buffer pointer (physical address)
  u32 nextTD;       // Next TD (physical address)
  u32 bufferEnd;     // Buffer end (physical address)
};
#pragma pack(pop)

//
// OHCI Isochronous Transfer Descriptor (ITD) - 32 bytes
//
#pragma pack(push, 1)
struct OHCIIsochronousTD {
  union {
    u32 control;
    struct {
#ifdef __LITTLE_ENDIAN__
      u32 startingFrame : 16;   // Starting frame number
      u32 reserved0 : 5;
      u32 delayInterrupt : 3; // Delay interrupt
      u32 frameCount : 3;       // Frame count (0-7 = 1-8 frames)
      u32 reserved1 : 1;
      u32 conditionCode : 4;    // Condition code
#else
u32 conditionCode : 4;
      u32 reserved1 : 1;
      u32 frameCount : 3;
      u32 delayInterrupt : 3;
      u32 reserved0 : 5;
      u32 startingFrame : 16;
#endif
    };
  };
  u32 bufferPage0;            // Buffer page 0 (physical address, page aligned)
  u32 nextTD; // Next ITD (physical address)
  u32 bufferEnd;     // Buffer end (physical address)
  u16 offsetPSW[8];      // Offset/PacketStatusWord for each frame
};
#pragma pack(pop)

//
// OHCI Host Controller Communications Area (HCCA) - 256 bytes
//
#pragma pack(push, 1)
struct OHCI_HCCA {
  u32 interruptTable[32];     // Interrupt ED table
  u16 frameNumber;        // Current frame number
  u16 pad1;       // Pad for alignment
  u32 doneHead;               // Done queue head
  u8 reserved[116];    // Reserved for HC use
};
#pragma pack(pop)

// OHCI Condition Codes
enum class OHCIConditionCode : u32 {
  NoError = 0x0,
  CRC = 0x1,
  BitStuffing = 0x2,
  DataToggleMismatch = 0x3,
  Stall = 0x4,
  DeviceNotResponding = 0x5,
  PIDCheckFailure = 0x6,
  UnexpectedPID = 0x7,
  DataOverrun = 0x8,
  DataUnderrun = 0x9,
  Reserved1 = 0xA,
  Reserved2 = 0xB,
  BufferOverrun = 0xC,
  BufferUnderrun = 0xD,
  NotAccessed = 0xE,
  NotAccessed2 = 0xF
};

// USB Transfer direction
enum class USBDirection : u32 {
  Setup = 0,
  Out = 1,
  In = 2,
  FromTD = 3
};

// USB device speed
enum class USBSpeed {
  Full = 0,
  Low = 1,
  High = 2  // Not used in OHCI, but for completeness
};

//
// USB Device Info
//
struct USBDeviceInfo {
  u16 vendorId;
  u16 productId;
  std::string description;
  std::string manufacturer;
  std::string serialNumber;
  u8 deviceAddress;
  USBSpeed speed;
  bool isConnected;
};

//
// USB Transfer Request
//
struct USBTransferRequest {
  u8 deviceAddress;
  u8 endpointNumber;
  USBDirection direction;
  std::vector<u8> data;
  u32 maxLength;
  bool isSetup;
  u8 setupPacket[8];
};

//
// USB Transfer Result
//
struct USBTransferResult {
  OHCIConditionCode conditionCode;
  u32 bytesTransferred;
  std::vector<u8> data;
};

//
// Pending Control Transfer State
// WinUSB handles all 3 phases (SETUP, DATA, STATUS) in a single call,
// but OHCI sends them as separate TDs. We need to cache the result.
//
struct PendingControlTransfer {
  bool hasData;                  // True if we have cached data from WinUSB
  std::vector<u8> data;        // Cached response data from SETUP phase
  u32 dataOffset;          // Current read offset into cached data
  u8 setupPacket[8];             // The setup packet for reference
  u16 wLength;        // Expected data length from setup packet
  USBDirection dataDirection;  // Direction of data phase (In or Out)
  bool setupCompleted;         // True after SETUP phase is done
  bool dataCompleted;  // True after DATA phase is done
};

#ifdef _WIN32

//
// WinUSB Device Handle
//
class WinUSBDevice {
public:
  WinUSBDevice();
  ~WinUSBDevice();

  bool Open(const std::wstring& devicePath);
  void Close();
  bool IsOpen() const { return winusbHandle != nullptr; }

  // Control transfers
  bool ControlTransfer(u8 requestType, u8 request, u16 value, u16 index,
       u8* buffer, u16 length, u32& bytesTransferred);

  // Bulk/Interrupt transfers
  bool BulkOrInterruptTransfer(u8 pipeId, u8* buffer, u32 length,
 u32& bytesTransferred, bool isRead);

  // Get device descriptor
  bool GetDeviceDescriptor(u8* buffer, u32 bufferLength, u32& bytesReturned);

  // Get configuration descriptor
  bool GetConfigDescriptor(u8* buffer, u32 bufferLength, u32& bytesReturned);

  // Claim/release interface
  bool ClaimInterface(u8 interfaceNumber);
  bool ReleaseInterface(u8 interfaceNumber);

  // Get pipe information
  bool GetPipeInfo(u8 interfaceIndex, u8 pipeIndex,
           WINUSB_PIPE_INFORMATION& pipeInfo);

  // Abort pipe
  bool AbortPipe(u8 pipeId);

  // Reset pipe
  bool ResetPipe(u8 pipeId);

  // Flush pipe
  bool FlushPipe(u8 pipeId);

  // Set pipe policy
  bool SetPipePolicy(u8 pipeId, u32 policyType, u32 valueLength, void* value);

private:
  HANDLE deviceHandle;
  WINUSB_INTERFACE_HANDLE winusbHandle;
  std::vector<WINUSB_INTERFACE_HANDLE> interfaceHandles;
};

#else

//
// Linux USB Device Handle (stub - not implemented)
//
class LinuxUSBDevice {
public:
  LinuxUSBDevice() {
    // TODO: Implement using libusb
  }

  ~LinuxUSBDevice() = default;

  bool Open(const std::string& devicePath) {
    // TODO: Implement
    return false;
  }

  void Close() {
    // TODO: Implement
  }

  bool IsOpen() const { return false; }

  bool ControlTransfer(u8 requestType, u8 request, u16 value, u16 index,
        u8* buffer, u16 length, u32& bytesTransferred) {
    // TODO: Implement
    return false;
  }

  bool BulkOrInterruptTransfer(u8 pipeId, u8* buffer, u32 length,
         u32& bytesTransferred, bool isRead) {
    // TODO: Implement
    return false;
  }

  bool GetDeviceDescriptor(u8* buffer, u32 bufferLength, u32& bytesReturned) {
    // TODO: Implement
    return false;
  }

  bool GetConfigDescriptor(u8* buffer, u32 bufferLength, u32& bytesReturned) {
    // TODO: Implement
    return false;
  }
};

#endif // _WIN32

//
// USB Passthrough Manager
// Handles enumeration and management of passthrough USB devices
//
class USBPassthroughManager {
public:
  USBPassthroughManager();
  ~USBPassthroughManager();

  // Initialize the USB passthrough system
  bool Initialize();

  // Shutdown the USB passthrough system
  void Shutdown();

  // Enumerate available USB devices
  std::vector<USBDeviceInfo> EnumerateDevices();

  // Attach a USB device for passthrough
  bool AttachDevice(u16 vendorId, u16 productId);

  // Detach a USB device
  bool DetachDevice(u16 vendorId, u16 productId);

  // Check if a device is attached
  bool IsDeviceAttached(u8 deviceAddress) const;

  // Perform a USB transfer
  USBTransferResult PerformTransfer(const USBTransferRequest& request);

  // Get attached device info
  std::vector<USBDeviceInfo> GetAttachedDevices() const;

  // Set device address (called after SET_ADDRESS request)
  void SetDeviceAddress(u16 vendorId, u16 productId, u8 address);

private:
#ifdef _WIN32
  struct AttachedDevice {
    std::unique_ptr<WinUSBDevice> device;
    USBDeviceInfo info;
    std::wstring devicePath;

    // Single pending control transfer state per device
    // Only one control transfer can be active at a time on endpoint 0
    PendingControlTransfer pendingControlTransfer;
    bool hasPendingControl = false;
  };

  std::vector<AttachedDevice> attachedDevices;
  mutable std::mutex deviceMutex;

  // Helper to find device paths
  std::vector<std::wstring> FindDevicePaths(u16 vendorId, u16 productId);

  // Handle control transfer phases
  USBTransferResult HandleControlSetup(AttachedDevice& dev, const USBTransferRequest& request);
  USBTransferResult HandleControlData(AttachedDevice& dev, const USBTransferRequest& request);
  USBTransferResult HandleControlStatus(AttachedDevice& dev, const USBTransferRequest& request);
#else
  // Linux implementation would use libusb
  // TODO: Implement Linux support
#endif

  bool initialized;
};

//
// ED/TD Processor
// Handles processing of OHCI Endpoint Descriptors and Transfer Descriptors
//
class OHCITransferProcessor {
public:
  OHCITransferProcessor();
  ~OHCITransferProcessor();

  // Set RAM pointer for direct memory access
  void SetRAM(RAM* ram);

  // Set USB passthrough manager
  void SetPassthroughManager(USBPassthroughManager* manager);

  // Process a control ED list
  void ProcessControlList(u32 headED);

  // Process a bulk ED list
  void ProcessBulkList(u32 headED);

  // Process periodic (interrupt) ED list
  void ProcessPeriodicList(u32 edAddress);

  // Process isochronous transfers
  void ProcessIsochronousList(u32 edAddress);

  // Get and clear the done queue
  u32 GetDoneHead();
  void ClearDoneHead();

  // Add TD to done queue
  void AddToDoneQueue(u32 tdAddress);

private:
  // Process a single ED
  bool ProcessED(OHCIEndpointDescriptor& ed, u32 edAddress);

  // Process a general TD
  OHCIConditionCode ProcessGeneralTD(OHCIGeneralTD& td, u32 tdAddress,
    const OHCIEndpointDescriptor& ed);

  // Process an isochronous TD
  OHCIConditionCode ProcessIsochronousTD(OHCIIsochronousTD& td, u32 tdAddress,
    const OHCIEndpointDescriptor& ed);

  // Get pointer to RAM address (returns nullptr if invalid)
  u8* GetRAMPointer(u32 address);

  // Read ED from memory
  OHCIEndpointDescriptor ReadED(u32 address);

  // Write ED to memory
  void WriteED(u32 address, const OHCIEndpointDescriptor& ed);

  // Read general TD from memory
  OHCIGeneralTD ReadGeneralTD(u32 address);

  // Write general TD to memory
  void WriteGeneralTD(u32 address, const OHCIGeneralTD& td);

  // Read isochronous TD from memory
  OHCIIsochronousTD ReadIsochronousTD(u32 address);

  // Write isochronous TD to memory
  void WriteIsochronousTD(u32 address, const OHCIIsochronousTD& td);

  // Read buffer data from memory
  std::vector<u8> ReadBuffer(u32 start, u32 end);

  // Write buffer data to memory
  void WriteBuffer(u32 start, const std::vector<u8>& data);

  RAM* ramPtr;
  USBPassthroughManager* passthroughManager;

  u32 doneHead;
  std::mutex doneQueueMutex;
};

} // namespace PCIDev
} // namespace Xe
