/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "OHCI.h"
#include "USBPassthrough.h"

#include "Base/Assert.h"
#include "Base/Logging/Log.h"
#include "Core/RAM/RAM.h"
#include "Core/PCI/Bridge/PCIBridge.h"

#include <chrono>

using namespace std::chrono_literals;

namespace Xe {
  namespace PCIDev {

    OHCI::OHCI(const std::string &deviceName, u64 size, s32 instance, u32 ports) :
      PCIDevice(deviceName, size),
      instance(instance),
      ports(ports),
      ramPtr(nullptr),
      workerRunning(false) {
      pciConfigSpace.configSpaceHeader.reg0.hexData = instance == 0 ? 0x58041414 : 0x58051414;
      pciConfigSpace.configSpaceHeader.reg1.hexData = instance == 0 ? 0x02800156 : 0x02900106;
      pciConfigSpace.configSpaceHeader.reg2.hexData = instance == 0 ? 0x0C03100F : 0x0C032001;
      pciConfigSpace.configSpaceHeader.reg3.hexData = instance == 0 ? 0x00800000 : 0x00000000;
      pciConfigSpace.configSpaceHeader.regB.hexData = instance == 0 ? 0x58041414 : 0x58051414;
      pciConfigSpace.configSpaceHeader.regD.hexData = instance == 0 ? 0x00000000 : 0x00000050;
      pciConfigSpace.configSpaceHeader.regF.hexData = instance == 0 ? 0x50000100 : 0x50000400;

      // Set our PCI Dev Sizes
      pciDevSizes[0] = 0x1000; // BAR0

      // Initialize registers to reset state
      Reset();

      // Create USB passthrough manager
      passthroughManager = std::make_unique<USBPassthroughManager>();
      passthroughManager->Initialize();

      // Create transfer processor
      transferProcessor = std::make_unique<OHCITransferProcessor>();
      transferProcessor->SetPassthroughManager(passthroughManager.get());
    }

    OHCI::~OHCI() {
      StopWorker();
      if (passthroughManager) {
        passthroughManager->Shutdown();
      }
    }

    void OHCI::Reset() {
      HcRevision = 0x10;  // OHCI 1.0
      HcControl = 0;
      HcCommandStatus = 0;
      HcInterruptStatus = 0;
      HcInterruptEnable = 0;
      HcHCCA = 0;
      HcPeriodCurrentED = 0;
      HcControlHeadED = 0;
      HcControlCurrentED = 0;
      HcBulkHeadED = 0;
      HcBulkCurrentED = 0;
      HcDoneHead = 0;
      HcFmInterval = 0x2EDF;  // Default frame interval
      HcFmRemaining = 0;
      HcFmNumber = 0;
      HcPeriodicStart = 0;
      HcLSThreshold = 0x628;  // Default LS threshold
      HcRhDescriptorA = (1 << 24) | ports;  // NPS=1 (No Power Switching), NDP=ports
      HcRhDescriptorB = 0;
      HcRhStatus = 0;

      // Reset port status - keep device connection state but clear other bits
      for (u32 i = 0; i < OHCI_MAX_PORTS; i++) {
        if (portDevices[i].hasDevice) {
          // Device still connected, set appropriate bits
          HcRhPortStatus[i] = static_cast<u32>(OHCIPortStatus::CCS) |  // Connected
            static_cast<u32>(OHCIPortStatus::CSC) |  // Connect status changed
            static_cast<u32>(OHCIPortStatus::PPS);   // Port powered
          if (portDevices[i].speed == USBSpeed::Low) {
            HcRhPortStatus[i] |= static_cast<u32>(OHCIPortStatus::LSDA);
          }
        }
        else {
          HcRhPortStatus[i] = 0;
        }
      }
    }

    void OHCI::SetRAM(RAM *ram) {
      ramPtr = ram;
      // Also set RAM pointer in transfer processor for direct memory access
      if (transferProcessor) {
        transferProcessor->SetRAM(ram);
      }
    }

    void OHCI::SetPCIBridge(PCIBridge *bridgePtr) { parentBus = bridgePtr; }

    bool OHCI::AttachUSBDevice(u16 vendorId, u16 productId) {
      // Find an available port
      s32 portIndex = FindAvailablePort();
      if (portIndex < 0) {
        LOG_WARNING(OHCI, "{} No available ports for device {:04X}:{:04X}",
          instance, vendorId, productId);
        return false;
      }

      return AttachUSBDeviceToPort(vendorId, productId, static_cast<u32>(portIndex));
    }

    bool OHCI::AttachUSBDeviceToPort(u16 vendorId, u16 productId, u32 portIndex) {
      if (portIndex >= ports) {
        LOG_ERROR(OHCI, "{} Invalid port index {} (max {})", instance, portIndex, ports);
        return false;
      }

      if (portDevices[portIndex].hasDevice) {
        LOG_WARNING(OHCI, "{} Port {} already has a device attached", instance, portIndex);
        return false;
      }

      // Attach to the passthrough manager
      if (!passthroughManager || !passthroughManager->AttachDevice(vendorId, productId)) {
        LOG_ERROR(OHCI, "{} Failed to attach device {:04X}:{:04X} to passthrough manager",
          instance, vendorId, productId);
        return false;
      }

      // Connect device to this port (assume full speed for now)
      ConnectDeviceToPort(portIndex, vendorId, productId, USBSpeed::Full);

      LOG_INFO(OHCI, "{} Attached device {:04X}:{:04X} to port {}",
        instance, vendorId, productId, portIndex);
      return true;
    }

    bool OHCI::DetachUSBDevice(u16 vendorId, u16 productId) {
      // Find which port has this device
      s32 portIndex = FindPortByDevice(vendorId, productId);
      if (portIndex < 0) {
        LOG_WARNING(OHCI, "{} Device {:04X}:{:04X} not found on any port",
          instance, vendorId, productId);
        return false;
      }

      // Detach from passthrough manager
      if (passthroughManager) {
        passthroughManager->DetachDevice(vendorId, productId);
      }

      // Disconnect from port
      DisconnectDeviceFromPort(static_cast<u32>(portIndex));

      LOG_INFO(OHCI, "{} Detached device {:04X}:{:04X} from port {}",
        instance, vendorId, productId, portIndex);
      return true;
    }

    std::vector<USBDeviceInfo> OHCI::GetAttachedDevices() const {
      if (passthroughManager) {
        return passthroughManager->GetAttachedDevices();
      }
      return {};
    }

    const PortDeviceInfo& OHCI::GetPortDeviceInfo(u32 portIndex) const {
      static PortDeviceInfo emptyInfo;
      if (portIndex >= OHCI_MAX_PORTS) {
        return emptyInfo;
      }
      return portDevices[portIndex];
    }

    s32 OHCI::FindAvailablePort() const {
      for (u32 i = 0; i < ports; i++) {
        if (!portDevices[i].hasDevice) {
          return static_cast<s32>(i);
        }
      }
      return -1;  // No available port
    }

    s32 OHCI::FindPortByDevice(u16 vendorId, u16 productId) const {
      for (u32 i = 0; i < ports; i++) {
        if (portDevices[i].hasDevice &&
          portDevices[i].vendorId == vendorId &&
          portDevices[i].productId == productId) {
          return static_cast<s32>(i);
        }
      }
      return -1;  // Device not found
    }

    void OHCI::ConnectDeviceToPort(u32 portIndex, u16 vendorId, u16 productId, USBSpeed speed) {
      if (portIndex >= OHCI_MAX_PORTS) return;

      // Update port device info
      portDevices[portIndex].hasDevice = true;
      portDevices[portIndex].vendorId = vendorId;
      portDevices[portIndex].productId = productId;
      portDevices[portIndex].speed = speed;
      portDevices[portIndex].deviceAddress = 0;  // Will be set by SET_ADDRESS

      // Update port status register
      u32 status = HcRhPortStatus[portIndex];

      // Set Current Connect Status
      status |= static_cast<u32>(OHCIPortStatus::CCS);

      // Set Connect Status Change (interrupt condition)
      status |= static_cast<u32>(OHCIPortStatus::CSC);

      // Set Port Power Status (port is powered)
      status |= static_cast<u32>(OHCIPortStatus::PPS);

      // Set Low Speed Device Attached if applicable
      if (speed == USBSpeed::Low) {
        status |= static_cast<u32>(OHCIPortStatus::LSDA);
      } else { status &= ~static_cast<u32>(OHCIPortStatus::LSDA); }

      HcRhPortStatus[portIndex] = status;

      // Trigger Root Hub Status Change interrupt
      SetInterrupt(static_cast<u32>(OHCIInterrupt::RootHubStatusChange));

      LOG_DEBUG(OHCI, "{} Device connected to port {}: VID={:04X} PID={:04X} Speed={}",
        instance, portIndex, vendorId, productId,
        speed == USBSpeed::Low ? "Low" : "Full");
    }

    void OHCI::DisconnectDeviceFromPort(u32 portIndex) {
      if (portIndex >= OHCI_MAX_PORTS) return;

      // Clear port device info
      portDevices[portIndex].hasDevice = false;
      portDevices[portIndex].vendorId = 0;
      portDevices[portIndex].productId = 0;
      portDevices[portIndex].speed = USBSpeed::Full;
      portDevices[portIndex].deviceAddress = 0;

      // Update port status register
      u32 status = HcRhPortStatus[portIndex];

      // Clear Current Connect Status
      status &= ~static_cast<u32>(OHCIPortStatus::CCS);

      // Clear Port Enable Status (device disconnected = port disabled)
      status &= ~static_cast<u32>(OHCIPortStatus::PES);

      // Clear Low Speed Device Attached
      status &= ~static_cast<u32>(OHCIPortStatus::LSDA);

      // Set Connect Status Change (interrupt condition)
      status |= static_cast<u32>(OHCIPortStatus::CSC);

      // Set Port Enable Status Change
      status |= static_cast<u32>(OHCIPortStatus::PESC);

      HcRhPortStatus[portIndex] = status;

      // Trigger Root Hub Status Change interrupt
      SetInterrupt(static_cast<u32>(OHCIInterrupt::RootHubStatusChange));

      LOG_DEBUG(OHCI, "{} Device disconnected from port {}", instance, portIndex);
    }

    OHCIOperationalState OHCI::GetOperationalState() const {
      return static_cast<OHCIOperationalState>((HcControl >> 6) & 0x3);
    }

    void OHCI::StartWorker() {
      if (workerRunning.load()) {
        return;
      }

      workerRunning.store(true);
      workerThread = std::thread(&OHCI::WorkerLoop, this);
      LOG_INFO(OHCI, "{} Worker thread started", instance);
    }

    void OHCI::StopWorker() {
      if (!workerRunning.load()) {
        return;
      }

      workerRunning.store(false);
      if (workerThread.joinable()) {
        workerThread.join();
      }
      LOG_INFO(OHCI, "{} Worker thread stopped", instance);
    }

    void OHCI::WorkerLoop() {
      while (workerRunning.load()) {
        auto state = GetOperationalState();

        if (state == OHCIOperationalState::Operational) {
          ProcessLists();

          // Update frame number
          HcFmNumber = (HcFmNumber + 1) & 0xFFFF;

          // Check if we need to update done head
          UpdateDoneHead();
        }

        // Sleep for approximately one frame (1ms for USB full speed)
        std::this_thread::sleep_for(1ms);
      }
    }

    void OHCI::ProcessLists() {
      u32 control = HcControl;

      // Check if Control List is enabled
      if (control & (1 << 4)) {  // CLE bit
        ProcessControlList();
      }

      // Check if Bulk List is enabled
      if (control & (1 << 5)) {  // BLE bit
        ProcessBulkList();
      }

      // Check if Periodic List is enabled
      if (control & (1 << 2)) {  // PLE bit
        ProcessPeriodicList();
      }
    }

    void OHCI::ProcessControlList() {
      if (HcControlHeadED == 0 || !transferProcessor) { return; }

      // Check Control List Filled bit in command status
      if (HcCommandStatus & (1 << 1)) {
        transferProcessor->ProcessControlList(HcControlHeadED);
        HcCommandStatus &= ~(1 << 1);  // Clear CLF
      }
    }

    void OHCI::ProcessBulkList() {
      if (HcBulkHeadED == 0 || !transferProcessor) {
        return;
      }

      // Check Bulk List Filled bit in command status
      if (HcCommandStatus & (1 << 2)) {
        transferProcessor->ProcessBulkList(HcBulkHeadED);
        HcCommandStatus &= ~(1 << 2);  // Clear BLF
      }
    }

    void OHCI::ProcessPeriodicList() {
      if (HcHCCA == 0 || !transferProcessor || !ramPtr) { return; }

      // Read the HCCA to get interrupt ED for current frame
      OHCI_HCCA hcca = {};
      MemoryRead(HcHCCA, reinterpret_cast<u8*>(&hcca), sizeof(hcca));

      // Get ED for current frame (frame number mod 32)
      u32 frameIndex = HcFmNumber & 0x1F;
      u32 edAddress = hcca.interruptTable[frameIndex];

      if (edAddress != 0) {
        transferProcessor->ProcessPeriodicList(edAddress);
      }
    }

    void OHCI::UpdateDoneHead() {
      if (!transferProcessor || HcHCCA == 0) {
        return;
      }

      u32 doneHead = transferProcessor->GetDoneHead();
      if (doneHead == 0) {
        return;
      }

      // Write done head to HCCA
      OHCI_HCCA hcca = {};
      MemoryRead(HcHCCA, reinterpret_cast<u8*>(&hcca), sizeof(hcca));

      // Set done head with interrupt pending bit if needed
      u32 doneHeadValue = doneHead;
      if (HcInterruptStatus & static_cast<u32>(OHCIInterrupt::WritebackDoneHead)) {
        doneHeadValue |= 1;  // Set interrupt pending bit
      }

      hcca.doneHead = doneHeadValue;
      MemoryWrite(HcHCCA, reinterpret_cast<const u8*>(&hcca), sizeof(hcca));

      transferProcessor->ClearDoneHead();
      HcDoneHead = doneHead;

      // Set WritebackDoneHead interrupt
      SetInterrupt(static_cast<u32>(OHCIInterrupt::WritebackDoneHead));
    }

    void OHCI::SetInterrupt(u32 interrupt) {
      HcInterruptStatus |= interrupt;

      // Check if interrupt is enabled and master enable is set
      if ((HcInterruptEnable & interrupt) &&
        (HcInterruptEnable & static_cast<u32>(OHCIInterrupt::MasterInterruptEnable))) {
        parentBus->RouteInterrupt(instance == 0 ? PRIO_OHCI_0 : PRIO_OHCI_1);
        LOG_DEBUG(OHCI, "{} Interrupt triggered: {:#x}", instance, interrupt);
      }
    }

    void OHCI::MemoryRead(u32 address, u8 *data, u32 size) {
      if (ramPtr && address != 0) { ramPtr->Read(address, data, size); }
    }

    void OHCI::MemoryWrite(u32 address, const u8 *data, u32 size) {
      if (ramPtr && address != 0) { ramPtr->Write(address, data, size); }
    }

    u32 OHCI::GetPortStatus(u32 portIndex) {
      if (portIndex >= ports) { return 0; }

      u32 status = HcRhPortStatus[portIndex];
      const PortDeviceInfo &deviceInfo = portDevices[portIndex];

      // Ensure status bits match actual device state
      if (deviceInfo.hasDevice) {
        // Device is connected
        status |= static_cast<u32>(OHCIPortStatus::CCS);  // Current Connect Status
        status |= static_cast<u32>(OHCIPortStatus::PPS);  // Port Power Status

        // Set speed indicator
        if (deviceInfo.speed == USBSpeed::Low) {
          status |= static_cast<u32>(OHCIPortStatus::LSDA);  // Low Speed Device Attached
        } else { status &= ~static_cast<u32>(OHCIPortStatus::LSDA); }
      } else {
        // No device connected
        status &= ~static_cast<u32>(OHCIPortStatus::CCS);
        status &= ~static_cast<u32>(OHCIPortStatus::PES);
        status &= ~static_cast<u32>(OHCIPortStatus::LSDA);
      }

      return status;
    }

    void OHCI::SetPortStatus(u32 portIndex, u32 value) {
      if (portIndex >= ports) { return; }

      u32 currentStatus = HcRhPortStatus[portIndex];
      const PortDeviceInfo &deviceInfo = portDevices[portIndex];

      // Handle write-to-set and write-to-clear bits
      // OHCI spec: Writing to port status register has special semantics

      // Bit 0: ClearPortEnable - Writing 1 clears PES
      if (value & (1 << 0)) {
        currentStatus &= ~static_cast<u32>(OHCIPortStatus::PES);
        LOG_DEBUG(OHCI, "{} Port {} ClearPortEnable", instance, portIndex);
      }

      // Bit 1: SetPortEnable - Writing 1 sets PES (only if device connected)
      if (value & (1 << 1)) {
        if (deviceInfo.hasDevice) {
          currentStatus |= static_cast<u32>(OHCIPortStatus::PES);
          LOG_DEBUG(OHCI, "{} Port {} SetPortEnable", instance, portIndex);
        }
      }

      // Bit 2: SetPortSuspend - Writing 1 sets PSS
      if (value & (1 << 2)) {
        if (currentStatus & static_cast<u32>(OHCIPortStatus::PES)) {
          currentStatus |= static_cast<u32>(OHCIPortStatus::PSS);
          LOG_DEBUG(OHCI, "{} Port {} SetPortSuspend", instance, portIndex);
        }
      }

      // Bit 3: ClearSuspendStatus - Writing 1 clears PSS and initiates resume
      if (value & (1 << 3)) {
        if (currentStatus & static_cast<u32>(OHCIPortStatus::PSS)) {
          currentStatus &= ~static_cast<u32>(OHCIPortStatus::PSS);
          currentStatus |= static_cast<u32>(OHCIPortStatus::PSSC);  // Suspend status changed
          LOG_DEBUG(OHCI, "{} Port {} ClearSuspendStatus", instance, portIndex);
        }
      }

      // Bit 4: SetPortReset - Writing 1 initiates port reset
      if (value & (1 << 4)) {
        if (deviceInfo.hasDevice) {
          LOG_DEBUG(OHCI, "{} Port {} SetPortReset", instance, portIndex);

          // Set PRS (reset in progress)
          currentStatus |= static_cast<u32>(OHCIPortStatus::PRS);

          // Simulate reset completion (in real hardware this takes ~10ms)
          // For emulation, we complete it immediately
          currentStatus &= ~static_cast<u32>(OHCIPortStatus::PRS);  // Reset complete
          currentStatus |= static_cast<u32>(OHCIPortStatus::PRSC);  // Reset status changed
          currentStatus |= static_cast<u32>(OHCIPortStatus::PES);   // Port enabled after reset

          // Clear suspend if it was set
          currentStatus &= ~static_cast<u32>(OHCIPortStatus::PSS);

          LOG_DEBUG(OHCI, "{} Port {} reset complete, port enabled", instance, portIndex);
        }
      }

      // Bit 8: SetPortPower - Writing 1 sets PPS
      if (value & (1 << 8)) {
        currentStatus |= static_cast<u32>(OHCIPortStatus::PPS);
        LOG_DEBUG(OHCI, "{} Port {} SetPortPower", instance, portIndex);
      }

      // Bit 9: ClearPortPower - Writing 1 clears PPS
      if (value & (1 << 9)) {
        currentStatus &= ~static_cast<u32>(OHCIPortStatus::PPS);
        // Clearing power also disables the port
        currentStatus &= ~static_cast<u32>(OHCIPortStatus::PES);
        LOG_DEBUG(OHCI, "{} Port {} ClearPortPower", instance, portIndex);
      }

      // Clear status change bits by writing 1 to them
      // Bit 16: CSC - Connect Status Change
      if (value & static_cast<u32>(OHCIPortStatus::CSC)) {
        currentStatus &= ~static_cast<u32>(OHCIPortStatus::CSC);
      }
      // Bit 17: PESC - Port Enable Status Change
      if (value & static_cast<u32>(OHCIPortStatus::PESC)) {
        currentStatus &= ~static_cast<u32>(OHCIPortStatus::PESC);
      }
      // Bit 18: PSSC - Port Suspend Status Change
      if (value & static_cast<u32>(OHCIPortStatus::PSSC)) {
        currentStatus &= ~static_cast<u32>(OHCIPortStatus::PSSC);
      }
      // Bit 19: OCIC - Over Current Indicator Change
      if (value & static_cast<u32>(OHCIPortStatus::OCIC)) {
        currentStatus &= ~static_cast<u32>(OHCIPortStatus::OCIC);
      }
      // Bit 20: PRSC - Port Reset Status Change
      if (value & static_cast<u32>(OHCIPortStatus::PRSC)) {
        currentStatus &= ~static_cast<u32>(OHCIPortStatus::PRSC);
      }

      HcRhPortStatus[portIndex] = currentStatus;

      LOG_DEBUG(OHCI, "{} SetPortStatus[{}]: write=0x{:08X} result=0x{:08X}",
        instance, portIndex, value, currentStatus);
    }

    void OHCI::Read(u64 readAddress, u8 *data, u64 size) {
      u64 offset = readAddress & 0xFFF;
      ASSERT(size == 4);

      u32 ret = 0;

      switch (offset) {
      case 0x00:
        ret = HcRevision;
        break;
      case 0x04:
        ret = HcControl;
        break;
      case 0x08:
        ret = HcCommandStatus;
        break;
      case 0x0C:
        ret = HcInterruptStatus;
        break;
      case 0x10:
      case 0x14:
        ret = HcInterruptEnable;
        break;
      case 0x18:
        ret = HcHCCA;
        break;
      case 0x1C:
        ret = HcPeriodCurrentED;
        break;
      case 0x20:
        ret = HcControlHeadED;
        break;
      case 0x24:
        ret = HcControlCurrentED;
        break;
      case 0x28:
        ret = HcBulkHeadED;
        break;
      case 0x2C:
        ret = HcBulkCurrentED;
        break;
      case 0x30:
        ret = HcDoneHead;
        break;
      case 0x34:
        ret = HcFmInterval;
        break;
      case 0x38:
        ret = HcFmRemaining;
        break;
      case 0x3C:
        ret = HcFmNumber;
        break;
      case 0x40:
        ret = HcPeriodicStart;
        break;
      case 0x44:
        ret = HcLSThreshold;
        break;
      case 0x48:
        ret = HcRhDescriptorA;
        break;
      case 0x4C:
        ret = HcRhDescriptorB;
        break;
      case 0x50:
        ret = HcRhStatus;
        break;
      default:
        if (offset >= 0x54 && offset < 0x54 + (ports * 4)) {
          u32 portIndex = (offset - 0x54) / 4;
          ret = GetPortStatus(portIndex);
          LOG_DEBUG(OHCI, "HUB{} Read to HcRhPortStatus[{}] == {:#x}", instance, portIndex, ret);
        }
        break;
      }

      ret = byteswap_le<u32>(ret);
      LOG_DEBUG(OHCI, "{} Read(0x{:X}) == 0x{:X}", instance, offset, ret);
      memcpy(data, &ret, size);
    }

    void OHCI::Write(u64 writeAddress, const u8 *data, u64 size) {
      u64 offset = writeAddress & 0xFFF;
      ASSERT(size == 4);

      u32 value = 0;
      memcpy(&value, data, size);
      value = byteswap_le<u32>(value);

      switch (offset) {
      case 0x00:
        HcRevision = value;
        LOG_DEBUG(OHCI, "{} HcRevision = 0x{:X}", instance, value);
        break;
      case 0x04: {
        u32 oldControl = HcControl;
        HcControl = value;
        LOG_DEBUG(OHCI, "{} HcControl = 0x{:X}", instance, value);

        // Check for state transitions
        auto oldState = static_cast<OHCIOperationalState>((oldControl >> 6) & 0x3);
        auto newState = GetOperationalState();

        if (oldState != newState) {
          LOG_INFO(OHCI, "{} State transition: {} -> {}",
            instance, static_cast<u32>(oldState), static_cast<u32>(newState));

          if (newState == OHCIOperationalState::Operational) {
            StartWorker();
          } else if (newState == OHCIOperationalState::Reset ||
            newState == OHCIOperationalState::Suspend) {
            StopWorker();
          }
        }
        break;
      }
      case 0x08:
        // Check for Host Controller Reset first
        if (value & 1) {
          LOG_INFO(OHCI, "{} Host Controller Reset requested", instance);
          StopWorker();
          Reset();
        }
        // Other bits are set by writing 1
        HcCommandStatus |= (value & ~1);  // Don't store HCR bit
        LOG_DEBUG(OHCI, "{} HcCommandStatus = 0x{:X}", instance, value);
        break;
      case 0x0C:
        // Writing 1 clears the corresponding bit
        HcInterruptStatus &= ~value;
        LOG_DEBUG(OHCI, "{} HcInterruptStatus clear = 0x{:X}", instance, value);
        break;
      case 0x10:
        HcInterruptEnable |= value;
        LOG_DEBUG(OHCI, "{} HcInterruptEnable |= 0x{:X}", instance, value);
        break;
      case 0x14:
        HcInterruptEnable &= ~value;
        LOG_DEBUG(OHCI, "{} HcInterruptEnable &= ~0x{:X}", instance, value);
        break;
      case 0x18:
        HcHCCA = value & ~0xFF;  // Lower 8 bits are reserved
        LOG_DEBUG(OHCI, "{} HcHCCA = 0x{:X}", instance, value);
        break;
      case 0x1C:
        // HcPeriodCurrentED is read-only
        LOG_WARNING(OHCI, "{} Attempt to write read-only HcPeriodCurrentED", instance);
        break;
      case 0x20:
        HcControlHeadED = value & ~0xF;  // Lower 4 bits must be 0
        LOG_DEBUG(OHCI, "{} HcControlHeadED = 0x{:X}", instance, value);
        break;
      case 0x24:
        HcControlCurrentED = value & ~0xF;
        LOG_DEBUG(OHCI, "{} HcControlCurrentED = 0x{:X}", instance, value);
        break;
      case 0x28:
        HcBulkHeadED = value & ~0xF;
        LOG_DEBUG(OHCI, "{} HcBulkHeadED = 0x{:X}", instance, value);
        break;
      case 0x2C:
        HcBulkCurrentED = value & ~0xF;
        LOG_DEBUG(OHCI, "{} HcBulkCurrentED = 0x{:X}", instance, value);
        break;
      case 0x30:
        // HcDoneHead is read-only
        LOG_WARNING(OHCI, "{} Attempt to write read-only HcDoneHead", instance);
        break;
      case 0x34:
        HcFmInterval = value;
        LOG_DEBUG(OHCI, "{} HcFmInterval = 0x{:X}", instance, value);
        break;
      case 0x38:
        // HcFmRemaining is read-only
        break;
      case 0x3C:
        // HcFmNumber is read-only
        break;
      case 0x40:
        HcPeriodicStart = value;
        LOG_DEBUG(OHCI, "{} HcPeriodicStart = 0x{:X}", instance, value);
        break;
      case 0x44:
        HcLSThreshold = value;
        LOG_DEBUG(OHCI, "{} HcLSThreshold = 0x{:X}", instance, value);
        break;
      case 0x48:
        // HcRhDescriptorA - some bits are read-only
        HcRhDescriptorA = (HcRhDescriptorA & 0xFF) | (value & ~0xFF);
        LOG_DEBUG(OHCI, "{} HcRhDescriptorA = 0x{:X}", instance, value);
        break;
      case 0x4C:
        HcRhDescriptorB = value;
        LOG_DEBUG(OHCI, "{} HcRhDescriptorB = 0x{:X}", instance, value);
        break;
      case 0x50:
        HcRhStatus = value;
        LOG_DEBUG(OHCI, "{} HcRhStatus = 0x{:X}", instance, value);
        break;
      default:
        if (offset >= 0x54 && offset < 0x54 + (ports * 4)) {
          u32 portIndex = (offset - 0x54) / 4;
          SetPortStatus(portIndex, value);
        } else {
          LOG_WARNING(OHCI, "{} Write(0x{:X}, 0x{:X}, {})", instance, offset, value, size);
        }
        break;
      }
    }

    void OHCI::MemSet(u64 writeAddress, s32 data, u64 size) {
      // Not typically used for OHCI
    }

    void OHCI::ConfigRead(u64 readAddress, u8 *data, u64 size) {
      memcpy(data, &pciConfigSpace.data[static_cast<u8>(readAddress)], size);
    }

    void OHCI::ConfigWrite(u64 writeAddress, const u8 *data, u64 size) {
      // Check if we're being scanned
      u64 tmp = 0;
      memcpy(&tmp, data, size);

      if (static_cast<u8>(writeAddress) >= 0x10 && static_cast<u8>(writeAddress) < 0x34) {
        const u32 regOffset = (static_cast<u8>(writeAddress) - 0x10) >> 2;
        if (pciDevSizes[regOffset] != 0) {
          if (tmp == 0xFFFFFFFF) { // PCI BAR Size discovery
            u64 x = 2;
            for (int idx = 2; idx < 31; idx++) {
              tmp &= ~x;
              x <<= 1;
              if (x >= pciDevSizes[regOffset]) {
                break;
              }
            }
            tmp &= ~0x3;
          }
        }
        if (static_cast<u8>(writeAddress) == 0x30) { // Expansion ROM Base Address
          tmp = 0; // Register not implemented
        }
      }

      memcpy(&pciConfigSpace.data[static_cast<u8>(writeAddress)], &tmp, size);
    }

  } // namespace PCIDev
} // namespace Xe
