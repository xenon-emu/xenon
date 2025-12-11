/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <memory>
#include <thread>
#include <atomic>
#include <array>

#include "Core/PCI/PCIDevice.h"
#include "USBPassthrough.h"

// Forward declaration
class RAM;
class PCIBridge;

#define OHCI_DEV_SIZE 0x1000
#define OHCI_MAX_PORTS 9

namespace Xe {
namespace PCIDev {

// OHCI Operational State
enum class OHCIOperationalState : u32 {
  Reset = 0,
  Resume = 1,
  Operational = 2,
  Suspend = 3
};

// OHCI Register offsets
enum class OHCIRegister : u32 {
  HcRevision = 0x00,
  HcControl = 0x04,
  HcCommandStatus = 0x08,
  HcInterruptStatus = 0x0C,
  HcInterruptEnable = 0x10,
  HcInterruptDisable = 0x14,
  HcHCCA = 0x18,
  HcPeriodCurrentED = 0x1C,
  HcControlHeadED = 0x20,
  HcControlCurrentED = 0x24,
  HcBulkHeadED = 0x28,
  HcBulkCurrentED = 0x2C,
  HcDoneHead = 0x30,
  HcFmInterval = 0x34,
  HcFmRemaining = 0x38,
  HcFmNumber = 0x3C,
  HcPeriodicStart = 0x40,
  HcLSThreshold = 0x44,
  HcRhDescriptorA = 0x48,
  HcRhDescriptorB = 0x4C,
  HcRhStatus = 0x50,
  HcRhPortStatusBase = 0x54
};

// OHCI Interrupt bits
enum class OHCIInterrupt : u32 {
  SchedulingOverrun = (1 << 0),
  WritebackDoneHead = (1 << 1),
  StartOfFrame = (1 << 2),
  ResumeDetected = (1 << 3),
  UnrecoverableError = (1 << 4),
  FrameNumberOverflow = (1 << 5),
  RootHubStatusChange = (1 << 6),
  OwnershipChange = (1 << 30),
  MasterInterruptEnable = (1 << 31)
};

// OHCI Port Status bits
enum class OHCIPortStatus : u32 {
  // Status bits (read)
  CCS  = (1 << 0),   // CurrentConnectStatus - Device connected
  PES  = (1 << 1),   // PortEnableStatus - Port enabled
  PSS  = (1 << 2),   // PortSuspendStatus - Port suspended
  POCI = (1 << 3),   // PortOverCurrentIndicator
  PRS  = (1 << 4), // PortResetStatus - Reset in progress
  PPS  = (1 << 8),   // PortPowerStatus - Port powered
  LSDA = (1 << 9),   // LowSpeedDeviceAttached
  
  // Status change bits (read, write 1 to clear)
  CSC  = (1 << 16),  // ConnectStatusChange
  PESC = (1 << 17),  // PortEnableStatusChange
  PSSC = (1 << 18),  // PortSuspendStatusChange
  OCIC = (1 << 19),  // OverCurrentIndicatorChange
  PRSC = (1 << 20),  // PortResetStatusChange
};

// Per-port device information
struct PortDeviceInfo {
  bool hasDevice = false;
  u16 vendorId = 0;
  u16 productId = 0;
  USBSpeed speed = USBSpeed::Full;
  u8 deviceAddress = 0;
};

class OHCI : public PCIDevice {
public:
  OHCI(const std::string &deviceName, u64 size, s32 instance, u32 ports);
  virtual ~OHCI();

  void Read(u64 readAddress, u8 *data, u64 size) override;
  void Write(u64 writeAddress, const u8 *data, u64 size) override;
  void MemSet(u64 writeAddress, s32 data, u64 size) override;
  void ConfigRead(u64 readAddress, u8 *data, u64 size) override;
  void ConfigWrite(u64 writeAddress, const u8 *data, u64 size) override;

  // Set RAM pointer for DMA operations
  void SetRAM(RAM* ram);
  void SetPCIBridge(PCIBridge* bridgePtr);

  // USB Passthrough interface
  USBPassthroughManager* GetPassthroughManager() { return passthroughManager.get(); }

  // Attach a USB device for passthrough (assigns to next available port)
  bool AttachUSBDevice(u16 vendorId, u16 productId);

  // Attach a USB device to a specific port
  bool AttachUSBDeviceToPort(u16 vendorId, u16 productId, u32 portIndex);

  // Detach a USB device
  bool DetachUSBDevice(u16 vendorId, u16 productId);

  // Get list of attached devices
  std::vector<USBDeviceInfo> GetAttachedDevices() const;

  // Get device info for a specific port
  const PortDeviceInfo& GetPortDeviceInfo(u32 portIndex) const;

  // Get number of ports
  u32 GetPortCount() const { return ports; }

protected:
  // Instance number (0 or 1)
  s32 instance;

  // Number of USB ports
  u32 ports;

  // RAM pointer for DMA
  RAM* ramPtr;
  // PCI Bridge for interrupts
  PCIBridge* parentBus;

  // Per-port device tracking
  std::array<PortDeviceInfo, OHCI_MAX_PORTS> portDevices;

  // OHCI Registers
  u32 HcRevision;         //  0x00
  u32 HcControl;          //  0x04
  u32 HcCommandStatus;    //  0x08
  u32 HcInterruptStatus;  //  0x0C
  u32 HcInterruptEnable;  //  0x10
  u32 HcHCCA;             //  0x18
  u32 HcPeriodCurrentED;  //  0x1C
  u32 HcControlHeadED;    //  0x20
  u32 HcControlCurrentED; //  0x24
  u32 HcBulkHeadED;       //  0x28
  u32 HcBulkCurrentED;    //  0x2C
  u32 HcDoneHead;         //  0x30
  u32 HcFmInterval;       //  0x34
  u32 HcFmRemaining;      //  0x38
  u32 HcFmNumber;         //  0x3C
  u32 HcPeriodicStart;    //  0x40
  u32 HcLSThreshold;      //  0x44
  u32 HcRhDescriptorA;    //  0x48
  u32 HcRhDescriptorB;    //  0x4C
  u32 HcRhStatus;         //  0x50
  // HcRhPortStatus registers start at 0x54
  u32 HcRhPortStatus[OHCI_MAX_PORTS];  //  0x54 - 0x74

  // USB Passthrough Manager
  std::unique_ptr<USBPassthroughManager> passthroughManager;

  // Transfer Processor
  std::unique_ptr<OHCITransferProcessor> transferProcessor;

  // Worker thread for processing transfers
  std::thread workerThread;
  std::atomic<bool> workerRunning;

  // Start/stop the worker thread
  void StartWorker();
  void StopWorker();

  // Worker thread function
  void WorkerLoop();

  // Process OHCI lists
  void ProcessLists();

  // Process control list
  void ProcessControlList();

  // Process bulk list
  void ProcessBulkList();

  // Process periodic list
  void ProcessPeriodicList();

  // Update done head in HCCA
  void UpdateDoneHead();

  // Set interrupt and potentially trigger IRQ
  void SetInterrupt(u32 interrupt);

  // Reset the controller
  void Reset();

  // Get operational state
  OHCIOperationalState GetOperationalState() const;

  // Memory access helpers
  void MemoryRead(u32 address, u8* data, u32 size);
  void MemoryWrite(u32 address, const u8* data, u32 size);

  // Port status helpers
  u32 GetPortStatus(u32 portIndex);
  void SetPortStatus(u32 portIndex, u32 value);

  // Port device connection helpers
  void ConnectDeviceToPort(u32 portIndex, u16 vendorId, u16 productId, USBSpeed speed);
  void DisconnectDeviceFromPort(u32 portIndex);
  s32 FindAvailablePort() const;
  s32 FindPortByDevice(u16 vendorId, u16 productId) const;
};

} // namespace PCIDev
} // namespace Xe
