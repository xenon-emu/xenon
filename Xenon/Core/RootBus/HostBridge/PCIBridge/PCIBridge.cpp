// Copyright 2025 Xenon Emulator Project

#include "Core/Xe_Main.h"

#include "PCIBridge.h"

#include "Base/Logging/Log.h"

#include "PCIBridgeConfig.h"

// Device tree, taken from an actual system, Corona V6.
/*
      BUS0
      -Dev 0, Func 0: Microsoft Corp. XMA Decoder
      -Dev 1, Func 0: Microsoft Corp. SATA Controller - CDROM
      -Dev 2, Func 0: Microsoft Corp. SATA Controller - HDD
      -Dev 4, Func 0: Microsoft Corp. OHCI Controller 0
      -Dev 4, Func 1: Microsoft Corp. EHCI Controller 0
      -Dev 5, Func 0: Microsoft Corp. OHCI Controller 1
      -Dev 5, Func 1: Microsoft Corp. EHCI Controller 1
      -Dev 7, Func 0: Microsoft Corp. Fast Ethernet Adapter
      -Dev 8, Func 0: Microsoft Corp. Secure Flash Controller
      -Dev 9, Func 0: Microsoft Corp. 580C
      -Dev A, Func 0: Microsoft Corp. System Management Controller
      -Dev F, Func 0: Microsoft Corp. 5841
*/

#define XMA_DEV_NUM 0x0
#define CDROM_DEV_NUM 0x1
#define HDD_DEV_NUM 0x2
#define OHCI0_DEV_NUM 0x4
#define EHCI0_DEV_NUM 0x4
#define OHCI1_DEV_NUM 0x5
#define EHCI1_DEV_NUM 0x5
#define FAST_ETH_DEV_NUM 0x7
#define SFC_DEV_NUM 0x8
#define AUDIO_CTRLR_DEV_NUM 0x9
#define SMC_DEV_NUM 0xA
#define _5841_DEV_NUM 0xF

PCIBridge::PCIBridge() {
  // Set config registers
  u8 i = 0;
  for (u16 idx = 0; idx < 256; idx += 4) {
    memcpy(&pciBridgeConfigSpace[idx], &pciBridgeConfigMap[i], 4);
    i++;
  }

  pciBridgeConfig.configSpaceHeader.reg0.hexData = pciBridgeConfigMap[0];
  pciBridgeConfig.configSpaceHeader.reg1.hexData = pciBridgeConfigMap[1];
  pciBridgeConfig.configSpaceHeader.reg2.hexData = pciBridgeConfigMap[2];
  pciBridgeConfig.configSpaceHeader.reg3.hexData = pciBridgeConfigMap[3];
  pciBridgeConfig.configSpaceHeader.BAR0 = pciBridgeConfigMap[4];
  pciBridgeConfig.configSpaceHeader.BAR1 = pciBridgeConfigMap[5];
  pciBridgeConfig.configSpaceHeader.reg6.hexData = pciBridgeConfigMap[6];
  pciBridgeConfig.configSpaceHeader.reg7.hexData = pciBridgeConfigMap[7];
  pciBridgeConfig.configSpaceHeader.reg8.hexData = pciBridgeConfigMap[8];
  pciBridgeConfig.configSpaceHeader.reg9.hexData = pciBridgeConfigMap[9];
  pciBridgeConfig.configSpaceHeader.prefetchableBaseUpper32Bits =
      pciBridgeConfigMap[0xA];
  pciBridgeConfig.configSpaceHeader.prefetchableLimitUpper32Bits =
      pciBridgeConfigMap[0xB];
  pciBridgeConfig.configSpaceHeader.regC.hexData = pciBridgeConfigMap[0xC];
  pciBridgeConfig.configSpaceHeader.regD.hexData = pciBridgeConfigMap[0xD];
  pciBridgeConfig.configSpaceHeader.expansionROMBaseAddress =
      pciBridgeConfigMap[0xE];
  pciBridgeConfig.configSpaceHeader.regF.hexData = pciBridgeConfigMap[0xF];

  // PCI Bridge Config regs
  pciBridgeState.REG_EA00000C = 0x7CFF; // Software writes here to enable interrupts (Bus IRQL)
}

void PCIBridge::RegisterIIC(Xe::XCPU::IIC::XenonIIC *xenonIICPtr) {
  xenonIIC = xenonIICPtr;
}

bool PCIBridge::RouteInterrupt(u8 prio) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "PCIBridge::RouteInterrupt", MP_AUTO);
  switch (prio) {
  case PRIO_CLOCK:
    if (pciBridgeState.PRIO_REG_CLCK.intEnabled) {
      xenonIIC->genInterrupt(PRIO_CLOCK,
        pciBridgeState.PRIO_REG_CLCK.targetCPU);
    }
    break;
  case PRIO_SATA_ODD:
    if (pciBridgeState.PRIO_REG_ODD.intEnabled) {
      xenonIIC->genInterrupt(PRIO_SATA_ODD,
        pciBridgeState.PRIO_REG_ODD.targetCPU);
    }
    break;
  case PRIO_SATA_HDD:
    if (pciBridgeState.PRIO_REG_HDD.intEnabled) {
      xenonIIC->genInterrupt(PRIO_SATA_HDD,
        pciBridgeState.PRIO_REG_HDD.targetCPU);
    }
    break;
  case PRIO_SMM:
    if (pciBridgeState.PRIO_REG_SMM.intEnabled) {
      xenonIIC->genInterrupt(PRIO_SMM,
        pciBridgeState.PRIO_REG_SMM.targetCPU);
    }
    break;
  case PRIO_OHCI_0:
    if (pciBridgeState.PRIO_REG_OHCI0.intEnabled) {
      xenonIIC->genInterrupt(PRIO_OHCI_0,
        pciBridgeState.PRIO_REG_OHCI0.targetCPU);
    }
    break;
  case PRIO_OHCI_1:
    if (pciBridgeState.PRIO_REG_OHCI1.intEnabled) {
      xenonIIC->genInterrupt(PRIO_OHCI_1,
        pciBridgeState.PRIO_REG_OHCI1.targetCPU);
    }
    break;
  case PRIO_EHCI_0:
    if (pciBridgeState.PRIO_REG_EHCI0.intEnabled) {
      xenonIIC->genInterrupt(PRIO_EHCI_0,
        pciBridgeState.PRIO_REG_EHCI0.targetCPU);
    }
    break;
  case PRIO_EHCI_1:
    if (pciBridgeState.PRIO_REG_EHCI1.intEnabled) {
      xenonIIC->genInterrupt(PRIO_EHCI_1,
        pciBridgeState.PRIO_REG_EHCI1.targetCPU);
    }
    break;
  case PRIO_XMA:
    if (pciBridgeState.PRIO_REG_XMA.intEnabled) {
      xenonIIC->genInterrupt(PRIO_XMA,
        pciBridgeState.PRIO_REG_XMA.targetCPU);
    }
    break;
  case PRIO_AUDIO:
    if (pciBridgeState.PRIO_REG_AUDIO.intEnabled) {
      xenonIIC->genInterrupt(PRIO_AUDIO,
        pciBridgeState.PRIO_REG_AUDIO.targetCPU);
    }
    break;
  case PRIO_ENET:
    if (pciBridgeState.PRIO_REG_ENET.intEnabled) {
      xenonIIC->genInterrupt(PRIO_ENET,
        pciBridgeState.PRIO_REG_ENET.targetCPU);
    }
    break;
  case PRIO_GRAPHICS:
    if (pciBridgeState.PRIO_REG_GRAPHICS.intEnabled) {
      xenonIIC->genInterrupt(PRIO_GRAPHICS,
        pciBridgeState.PRIO_REG_GRAPHICS.targetCPU);
    }
    break;
  case PRIO_SFCX:
    if (pciBridgeState.PRIO_REG_SFCX.intEnabled) {
      xenonIIC->genInterrupt(PRIO_SFCX,
        pciBridgeState.PRIO_REG_SFCX.targetCPU);
    }
    break;
  default:
    LOG_ERROR(PCIBridge, "Unknown interrupt being routed: {:#x}", prio);
    break;
  }
  return false;
}

void PCIBridge::CancelInterrupt(u8 prio) {
  switch (prio) {
  case PRIO_CLOCK:
    if (pciBridgeState.PRIO_REG_CLCK.intEnabled) {
      xenonIIC->cancelInterrupt(PRIO_CLOCK,
        pciBridgeState.PRIO_REG_CLCK.targetCPU);
    }
    break;
  case PRIO_SATA_ODD:
    if (pciBridgeState.PRIO_REG_ODD.intEnabled) {
      xenonIIC->cancelInterrupt(PRIO_SATA_ODD,
        pciBridgeState.PRIO_REG_ODD.targetCPU);
    }
    break;
  case PRIO_SATA_HDD:
    if (pciBridgeState.PRIO_REG_HDD.intEnabled) {
      xenonIIC->cancelInterrupt(PRIO_SATA_HDD,
        pciBridgeState.PRIO_REG_HDD.targetCPU);
    }
    break;
  case PRIO_SMM:
    if (pciBridgeState.PRIO_REG_SMM.intEnabled) {
      xenonIIC->cancelInterrupt(PRIO_SMM,
        pciBridgeState.PRIO_REG_SMM.targetCPU);
    }
    break;
  case PRIO_OHCI_0:
    if (pciBridgeState.PRIO_REG_OHCI0.intEnabled) {
      xenonIIC->cancelInterrupt(PRIO_OHCI_0,
        pciBridgeState.PRIO_REG_OHCI0.targetCPU);
    }
    break;
  case PRIO_OHCI_1:
    if (pciBridgeState.PRIO_REG_OHCI1.intEnabled) {
      xenonIIC->cancelInterrupt(PRIO_OHCI_1,
        pciBridgeState.PRIO_REG_OHCI1.targetCPU);
    }
    break;
  case PRIO_EHCI_0:
    if (pciBridgeState.PRIO_REG_EHCI0.intEnabled) {
      xenonIIC->cancelInterrupt(PRIO_EHCI_0,
        pciBridgeState.PRIO_REG_EHCI0.targetCPU);
    }
    break;
  case PRIO_EHCI_1:
    if (pciBridgeState.PRIO_REG_EHCI1.intEnabled) {
      xenonIIC->cancelInterrupt(PRIO_EHCI_1,
        pciBridgeState.PRIO_REG_EHCI1.targetCPU);
    }
    break;
  case PRIO_XMA:
    if (pciBridgeState.PRIO_REG_XMA.intEnabled) {
      xenonIIC->cancelInterrupt(PRIO_XMA,
        pciBridgeState.PRIO_REG_XMA.targetCPU);
    }
    break;
  case PRIO_AUDIO:
    if (pciBridgeState.PRIO_REG_AUDIO.intEnabled) {
      xenonIIC->cancelInterrupt(PRIO_AUDIO,
        pciBridgeState.PRIO_REG_AUDIO.targetCPU);
    }
    break;
  case PRIO_ENET:
    if (pciBridgeState.PRIO_REG_ENET.intEnabled) {
      xenonIIC->cancelInterrupt(PRIO_ENET,
        pciBridgeState.PRIO_REG_ENET.targetCPU);
    }
    break;
  case PRIO_GRAPHICS:
    if (pciBridgeState.PRIO_REG_GRAPHICS.intEnabled) {
      xenonIIC->cancelInterrupt(PRIO_GRAPHICS,
        pciBridgeState.PRIO_REG_GRAPHICS.targetCPU);
    }
    break;
  case PRIO_SFCX:
    if (pciBridgeState.PRIO_REG_SFCX.intEnabled) {
      xenonIIC->cancelInterrupt(PRIO_SFCX,
        pciBridgeState.PRIO_REG_SFCX.targetCPU);
    }
    break;
  default:
    LOG_ERROR(PCIBridge, "Unknown interrupt being cancelled: {:#x}", prio);
    break;
  }
}

bool PCIBridge::isAddressMappedinBAR(u32 address) {
  u32 bar0 = pciBridgeConfig.configSpaceHeader.BAR0;
  u32 bar1 = pciBridgeConfig.configSpaceHeader.BAR1;

  if ((address >= bar0 && address < (bar0 + PCI_BRIDGE_SIZE)) ||
      (address >= bar1 && address < (bar1 + PCI_BRIDGE_SIZE))) {
    return true;
  }

  return false;
}

void PCIBridge::addPCIDevice(PCIDevice *device) {
  LOG_INFO(PCIBridge, "Attached: {}", device->GetDeviceName());

  connectedPCIDevices.push_back(device);
}

void PCIBridge::resetPCIDevice(PCIDevice *device) {
  LOG_INFO(PCIBridge, "Resetting: {}", device->GetDeviceName());

  for (u64 i = 0; i != connectedPCIDevices.size(); ++i) {
    PCIDevice* dev = connectedPCIDevices[i];
    if (dev->GetDeviceName() == device->GetDeviceName()) {
      connectedPCIDevices.erase(connectedPCIDevices.begin() + i);
    }
  }

  connectedPCIDevices.push_back(device);
}

bool PCIBridge::Read(u64 readAddress, u8* data, u64 size) {
  // Reading to our own space?
  if (readAddress >= PCI_BRIDGE_BASE_ADDRESS &&
      readAddress <= PCI_BRIDGE_BASE_END_ADDRESS) {
    switch (readAddress) {
    case 0xEA000000:
      memcpy(data, &pciBridgeState.REG_EA000000, size);
      break;
    case 0xEA000004:
      memcpy(data, &pciBridgeState.REG_EA000004, size);
      break;
    case 0xEA00000C:
      memcpy(data, &pciBridgeState.REG_EA00000C, size);
      break;
    case 0xEA000010:
      memcpy(data, &pciBridgeState.PRIO_REG_CLCK.hexData, size);
      break;
    case 0xEA000014:
      memcpy(data, &pciBridgeState.PRIO_REG_ODD.hexData, size);
      break;
    case 0xEA000018:
      memcpy(data, &pciBridgeState.PRIO_REG_HDD.hexData, size);
      break;
    case 0xEA00001C:
      memcpy(data, &pciBridgeState.PRIO_REG_SMM.hexData, size);
      break;
    case 0xEA000020:
      memcpy(data, &pciBridgeState.PRIO_REG_OHCI0.hexData, size);
      break;
    case 0xEA000024:
      memcpy(data, &pciBridgeState.PRIO_REG_OHCI1.hexData, size);
      break;
    case 0xEA000028:
      memcpy(data, &pciBridgeState.PRIO_REG_EHCI0.hexData, size);
      break;
    case 0xEA00002C:
      memcpy(data, &pciBridgeState.PRIO_REG_EHCI1.hexData, size);
      break;
    case 0xEA000038:
      memcpy(data, &pciBridgeState.PRIO_REG_ENET.hexData, size);
      break;
    case 0xEA00003C:
      memcpy(data, &pciBridgeState.PRIO_REG_XMA.hexData, size);
      break;
    case 0xEA000040:
      memcpy(data, &pciBridgeState.PRIO_REG_AUDIO.hexData, size);
      break;
    case 0xEA000044:
      memcpy(data, &pciBridgeState.PRIO_REG_SFCX.hexData, size);
      break;
    default:
      LOG_ERROR(PCIBridge, "Unknown reg being read: {:#x}", readAddress);
      break;
    }
    return true;
  }

  // Try writing to one of the attached devices.
  for (auto &device : connectedPCIDevices) {
    if (device->isAddressMappedInBAR(static_cast<u32>(readAddress))) {
      // Hit
      device->Read(readAddress, data, size);
      return true;
    }
  }
  memset(data, 0xFF, size);
  return false;
}

bool PCIBridge::Write(u64 writeAddress, const u8 *data, u64 size) {
  u64 tmp{};
  memcpy(&tmp, data, sizeof(tmp) > size ? size : sizeof(tmp));
  bool enabled = (tmp & 0x00800000) >> 20;
  bool latched = (tmp & 0x00200000) >> 20;
  u8 targetCPU = (tmp & 0x00003F00) >> 8;
  u8 cpuIRQ = (tmp & 0x0000003F) << 2;

  // Writing to our own space?
  if (writeAddress >= PCI_BRIDGE_BASE_ADDRESS &&
      writeAddress <= PCI_BRIDGE_BASE_END_ADDRESS) {
    switch (writeAddress) {
    case 0xEA000000:
      memcpy(&pciBridgeState.REG_EA000000, data, sizeof(pciBridgeState.REG_EA000000) > size ? size : sizeof(pciBridgeState.REG_EA000000));
      break;
    case 0xEA000004:
      memcpy(&pciBridgeState.REG_EA000004, data, sizeof(pciBridgeState.REG_EA000004) > size ? size : sizeof(pciBridgeState.REG_EA000004));
      break;
    case 0xEA00000C:
      memcpy(&pciBridgeState.REG_EA00000C, data, sizeof(pciBridgeState.REG_EA00000C) > size ? size : sizeof(pciBridgeState.REG_EA00000C));
      break;
    // PRIO_CLOCK
    case 0xEA000010:
      memcpy(&pciBridgeState.PRIO_REG_CLCK.hexData, data, sizeof(pciBridgeState.PRIO_REG_CLCK.hexData) > size ? size : sizeof(pciBridgeState.PRIO_REG_CLCK.hexData));
      pciBridgeState.PRIO_REG_CLCK.intEnabled = enabled;
      pciBridgeState.PRIO_REG_CLCK.latched = latched;
      pciBridgeState.PRIO_REG_CLCK.targetCPU = targetCPU;
      pciBridgeState.PRIO_REG_CLCK.cpuIRQ = cpuIRQ;
      break;
    // PRIO_SATA_ODD
    case 0xEA000014:
      memcpy(&pciBridgeState.PRIO_REG_ODD.hexData, data, sizeof(pciBridgeState.PRIO_REG_ODD.hexData) > size ? size : sizeof(pciBridgeState.PRIO_REG_ODD.hexData));
      pciBridgeState.PRIO_REG_ODD.intEnabled = enabled;
      pciBridgeState.PRIO_REG_ODD.latched = latched;
      pciBridgeState.PRIO_REG_ODD.targetCPU = targetCPU;
      pciBridgeState.PRIO_REG_ODD.cpuIRQ = cpuIRQ;
      break;
    // PRIO_SATA_HDD
    case 0xEA000018:
      memcpy(&pciBridgeState.PRIO_REG_HDD.hexData, data, sizeof(pciBridgeState.PRIO_REG_HDD.hexData) > size ? size : sizeof(pciBridgeState.PRIO_REG_HDD.hexData));
      pciBridgeState.PRIO_REG_HDD.intEnabled = enabled;
      pciBridgeState.PRIO_REG_HDD.latched = latched;
      pciBridgeState.PRIO_REG_HDD.targetCPU = targetCPU;
      pciBridgeState.PRIO_REG_HDD.cpuIRQ = cpuIRQ;
      break;
    // PRIO_SMM
    case 0xEA00001C:
      memcpy(&pciBridgeState.PRIO_REG_SMM.hexData, data, sizeof(pciBridgeState.PRIO_REG_SMM.hexData) > size ? size : sizeof(pciBridgeState.PRIO_REG_SMM.hexData));
      pciBridgeState.PRIO_REG_SMM.intEnabled = enabled;
      pciBridgeState.PRIO_REG_SMM.latched = latched;
      pciBridgeState.PRIO_REG_SMM.targetCPU = targetCPU;
      pciBridgeState.PRIO_REG_SMM.cpuIRQ = cpuIRQ;
      break;
    // PRIO_OHCI0
    case 0xEA000020:
      memcpy(&pciBridgeState.PRIO_REG_OHCI0.hexData, data, sizeof(pciBridgeState.PRIO_REG_OHCI0.hexData) > size ? size : sizeof(pciBridgeState.PRIO_REG_OHCI0.hexData));
      pciBridgeState.PRIO_REG_OHCI0.intEnabled = enabled;
      pciBridgeState.PRIO_REG_OHCI0.latched = latched;
      pciBridgeState.PRIO_REG_OHCI0.targetCPU = targetCPU;
      pciBridgeState.PRIO_REG_OHCI0.cpuIRQ = cpuIRQ;
      break;
    // PRIO_OHCI1
    case 0xEA000024:
      memcpy(&pciBridgeState.PRIO_REG_OHCI1.hexData, data, sizeof(pciBridgeState.PRIO_REG_OHCI1.hexData) > size ? size : sizeof(pciBridgeState.PRIO_REG_OHCI1.hexData));
      pciBridgeState.PRIO_REG_OHCI1.intEnabled = enabled;
      pciBridgeState.PRIO_REG_OHCI1.latched = latched;
      pciBridgeState.PRIO_REG_OHCI1.targetCPU = targetCPU;
      pciBridgeState.PRIO_REG_OHCI1.cpuIRQ = cpuIRQ;
      break;
    // PRIO_EHCI0
    case 0xEA000028:
      memcpy(&pciBridgeState.PRIO_REG_EHCI0.hexData, data, sizeof(pciBridgeState.PRIO_REG_EHCI0.hexData) > size ? size : sizeof(pciBridgeState.PRIO_REG_EHCI0.hexData));
      pciBridgeState.PRIO_REG_EHCI0.intEnabled = enabled;
      pciBridgeState.PRIO_REG_EHCI0.latched = latched;
      pciBridgeState.PRIO_REG_EHCI0.targetCPU = targetCPU;
      pciBridgeState.PRIO_REG_EHCI0.cpuIRQ = cpuIRQ;
      break;
    // PRIO_EHCI1
    case 0xEA00002C:
      memcpy(&pciBridgeState.PRIO_REG_EHCI1.hexData, data, sizeof(pciBridgeState.PRIO_REG_EHCI1.hexData) > size ? size : sizeof(pciBridgeState.PRIO_REG_EHCI1.hexData));
      pciBridgeState.PRIO_REG_EHCI1.intEnabled = enabled;
      pciBridgeState.PRIO_REG_EHCI1.latched = latched;
      pciBridgeState.PRIO_REG_EHCI1.targetCPU = targetCPU;
      pciBridgeState.PRIO_REG_EHCI1.cpuIRQ = cpuIRQ;
      break;
    // PRIO_ENET
    case 0xEA000038:
      memcpy(&pciBridgeState.PRIO_REG_ENET.hexData, data, sizeof(pciBridgeState.PRIO_REG_ENET.hexData) > size ? size : sizeof(pciBridgeState.PRIO_REG_ENET.hexData));
      pciBridgeState.PRIO_REG_ENET.intEnabled = enabled;
      pciBridgeState.PRIO_REG_ENET.latched = latched;
      pciBridgeState.PRIO_REG_ENET.targetCPU = targetCPU;
      pciBridgeState.PRIO_REG_ENET.cpuIRQ = cpuIRQ;
      break;
    // PRIO_XMA
    case 0xEA00003C:
      memcpy(&pciBridgeState.PRIO_REG_XMA.hexData, data, sizeof(pciBridgeState.PRIO_REG_XMA.hexData) > size ? size : sizeof(pciBridgeState.PRIO_REG_XMA.hexData));
      pciBridgeState.PRIO_REG_XMA.intEnabled = enabled;
      pciBridgeState.PRIO_REG_XMA.latched = latched;
      pciBridgeState.PRIO_REG_XMA.targetCPU = targetCPU;
      pciBridgeState.PRIO_REG_XMA.cpuIRQ = cpuIRQ;
      break;
    // PRIO_AUDIO
    case 0xEA000040:
      memcpy(&pciBridgeState.PRIO_REG_AUDIO.hexData, data, sizeof(pciBridgeState.PRIO_REG_AUDIO.hexData) > size ? size : sizeof(pciBridgeState.PRIO_REG_AUDIO.hexData));
      pciBridgeState.PRIO_REG_AUDIO.intEnabled = enabled;
      pciBridgeState.PRIO_REG_AUDIO.latched = latched;
      pciBridgeState.PRIO_REG_AUDIO.targetCPU = targetCPU;
      pciBridgeState.PRIO_REG_AUDIO.cpuIRQ = cpuIRQ;
      break;
    // PRIO_SFCX - Secure Flash Controller for Xbox Int
    case 0xEA000044:
      memcpy(&pciBridgeState.PRIO_REG_SFCX.hexData, data, sizeof(pciBridgeState.PRIO_REG_SFCX.hexData) > size ? size : sizeof(pciBridgeState.PRIO_REG_SFCX.hexData));
      pciBridgeState.PRIO_REG_SFCX.intEnabled = enabled;
      pciBridgeState.PRIO_REG_SFCX.latched = latched;
      pciBridgeState.PRIO_REG_SFCX.targetCPU = targetCPU;
      pciBridgeState.PRIO_REG_SFCX.cpuIRQ = cpuIRQ;
      break;
    default:
      LOG_ERROR(PCIBridge, "Unknown reg being written: {:#x}, {:#x}", writeAddress, tmp);
      break;
    }
    return true;
  }

  // Try writing to one of the attached devices.
  for (auto &device : connectedPCIDevices) {
    if (device->isAddressMappedInBAR(static_cast<u32>(writeAddress))) {
      // Hit
      device->Write(writeAddress, data, size);
      return true;
    }
  }
  return false;
}

bool PCIBridge::MemSet(u64 writeAddress, s32 data, u64 size) {
  u64 tmp{};
  memset(&tmp, data, sizeof(tmp) > size ? size : sizeof(tmp));
  bool enabled = (tmp & 0x00800000) >> 20;
  bool latched = (tmp & 0x00200000) >> 20;
  u8 targetCPU = (tmp & 0x00003F00) >> 8;
  u8 cpuIRQ = (tmp & 0x0000003F) << 2;

  // Writing to our own space?
  if (writeAddress >= PCI_BRIDGE_BASE_ADDRESS &&
      writeAddress <= PCI_BRIDGE_BASE_END_ADDRESS) {
    switch (writeAddress) {
    case 0xEA000000:
      memset(&pciBridgeState.REG_EA000000, data, sizeof(pciBridgeState.REG_EA000000) > size ? size : sizeof(pciBridgeState.REG_EA000000));
      break;
    case 0xEA000004:
      memset(&pciBridgeState.REG_EA000004, data, sizeof(pciBridgeState.REG_EA000004) > size ? size : sizeof(pciBridgeState.REG_EA000004));
      break;
    case 0xEA00000C:
      memset(&pciBridgeState.REG_EA00000C, data, sizeof(pciBridgeState.REG_EA00000C) > size ? size : sizeof(pciBridgeState.REG_EA00000C));
      break;
    // PRIO_CLOCK
    case 0xEA000010:
      memset(&pciBridgeState.PRIO_REG_CLCK.hexData, data, sizeof(pciBridgeState.PRIO_REG_CLCK.hexData) > size ? size : sizeof(pciBridgeState.PRIO_REG_CLCK.hexData));
      pciBridgeState.PRIO_REG_CLCK.intEnabled = enabled;
      pciBridgeState.PRIO_REG_CLCK.latched = latched;
      pciBridgeState.PRIO_REG_CLCK.targetCPU = targetCPU;
      pciBridgeState.PRIO_REG_CLCK.cpuIRQ = cpuIRQ;
      break;
    // PRIO_SATA_ODD
    case 0xEA000014:
      memset(&pciBridgeState.PRIO_REG_ODD.hexData, data, sizeof(pciBridgeState.PRIO_REG_ODD.hexData) > size ? size : sizeof(pciBridgeState.PRIO_REG_ODD.hexData));
      pciBridgeState.PRIO_REG_ODD.intEnabled = enabled;
      pciBridgeState.PRIO_REG_ODD.latched = latched;
      pciBridgeState.PRIO_REG_ODD.targetCPU = targetCPU;
      pciBridgeState.PRIO_REG_ODD.cpuIRQ = cpuIRQ;
      break;
    // PRIO_SATA_HDD
    case 0xEA000018:
      memset(&pciBridgeState.PRIO_REG_HDD.hexData, data, sizeof(pciBridgeState.PRIO_REG_HDD.hexData) > size ? size : sizeof(pciBridgeState.PRIO_REG_HDD.hexData));
      pciBridgeState.PRIO_REG_HDD.intEnabled = enabled;
      pciBridgeState.PRIO_REG_HDD.latched = latched;
      pciBridgeState.PRIO_REG_HDD.targetCPU = targetCPU;
      pciBridgeState.PRIO_REG_HDD.cpuIRQ = cpuIRQ;
      break;
    // PRIO_SMM
    case 0xEA00001C:
      memset(&pciBridgeState.PRIO_REG_SMM.hexData, data, sizeof(pciBridgeState.PRIO_REG_SMM.hexData) > size ? size : sizeof(pciBridgeState.PRIO_REG_SMM.hexData));
      pciBridgeState.PRIO_REG_SMM.intEnabled = enabled;
      pciBridgeState.PRIO_REG_SMM.latched = latched;
      pciBridgeState.PRIO_REG_SMM.targetCPU = targetCPU;
      pciBridgeState.PRIO_REG_SMM.cpuIRQ = cpuIRQ;
      break;
    // PRIO_OHCI0
    case 0xEA000020:
      memset(&pciBridgeState.PRIO_REG_OHCI0.hexData, data, sizeof(pciBridgeState.PRIO_REG_OHCI0.hexData) > size ? size : sizeof(pciBridgeState.PRIO_REG_OHCI0.hexData));
      pciBridgeState.PRIO_REG_OHCI0.intEnabled = enabled;
      pciBridgeState.PRIO_REG_OHCI0.latched = latched;
      pciBridgeState.PRIO_REG_OHCI0.targetCPU = targetCPU;
      pciBridgeState.PRIO_REG_OHCI0.cpuIRQ = cpuIRQ;
      break;
    // PRIO_OHCI1
    case 0xEA000024:
      memset(&pciBridgeState.PRIO_REG_OHCI1.hexData, data, sizeof(pciBridgeState.PRIO_REG_OHCI1.hexData) > size ? size : sizeof(pciBridgeState.PRIO_REG_OHCI1.hexData));
      pciBridgeState.PRIO_REG_OHCI1.intEnabled = enabled;
      pciBridgeState.PRIO_REG_OHCI1.latched = latched;
      pciBridgeState.PRIO_REG_OHCI1.targetCPU = targetCPU;
      pciBridgeState.PRIO_REG_OHCI1.cpuIRQ = cpuIRQ;
      break;
    // PRIO_EHCI0
    case 0xEA000028:
      memset(&pciBridgeState.PRIO_REG_EHCI0.hexData, data, sizeof(pciBridgeState.PRIO_REG_EHCI0.hexData) > size ? size : sizeof(pciBridgeState.PRIO_REG_EHCI0.hexData));
      pciBridgeState.PRIO_REG_EHCI0.intEnabled = enabled;
      pciBridgeState.PRIO_REG_EHCI0.latched = latched;
      pciBridgeState.PRIO_REG_EHCI0.targetCPU = targetCPU;
      pciBridgeState.PRIO_REG_EHCI0.cpuIRQ = cpuIRQ;
      break;
    // PRIO_EHCI1
    case 0xEA00002C:
      memset(&pciBridgeState.PRIO_REG_EHCI1.hexData, data, sizeof(pciBridgeState.PRIO_REG_EHCI1.hexData) > size ? size : sizeof(pciBridgeState.PRIO_REG_EHCI1.hexData));
      pciBridgeState.PRIO_REG_EHCI1.intEnabled = enabled;
      pciBridgeState.PRIO_REG_EHCI1.latched = latched;
      pciBridgeState.PRIO_REG_EHCI1.targetCPU = targetCPU;
      pciBridgeState.PRIO_REG_EHCI1.cpuIRQ = cpuIRQ;
      break;
    // PRIO_ENET
    case 0xEA000038:
      memset(&pciBridgeState.PRIO_REG_ENET.hexData, data, sizeof(pciBridgeState.PRIO_REG_ENET.hexData) > size ? size : sizeof(pciBridgeState.PRIO_REG_ENET.hexData));
      pciBridgeState.PRIO_REG_ENET.intEnabled = enabled;
      pciBridgeState.PRIO_REG_ENET.latched = latched;
      pciBridgeState.PRIO_REG_ENET.targetCPU = targetCPU;
      pciBridgeState.PRIO_REG_ENET.cpuIRQ = cpuIRQ;
      break;
    // PRIO_XMA
    case 0xEA00003C:
      memset(&pciBridgeState.PRIO_REG_XMA.hexData, data, sizeof(pciBridgeState.PRIO_REG_XMA.hexData) > size ? size : sizeof(pciBridgeState.PRIO_REG_XMA.hexData));
      pciBridgeState.PRIO_REG_XMA.intEnabled = enabled;
      pciBridgeState.PRIO_REG_XMA.latched = latched;
      pciBridgeState.PRIO_REG_XMA.targetCPU = targetCPU;
      pciBridgeState.PRIO_REG_XMA.cpuIRQ = cpuIRQ;
      break;
    // PRIO_AUDIO
    case 0xEA000040:
      memset(&pciBridgeState.PRIO_REG_AUDIO.hexData, data, sizeof(pciBridgeState.PRIO_REG_AUDIO.hexData) > size ? size : sizeof(pciBridgeState.PRIO_REG_AUDIO.hexData));
      pciBridgeState.PRIO_REG_AUDIO.intEnabled = enabled;
      pciBridgeState.PRIO_REG_AUDIO.latched = latched;
      pciBridgeState.PRIO_REG_AUDIO.targetCPU = targetCPU;
      pciBridgeState.PRIO_REG_AUDIO.cpuIRQ = cpuIRQ;
      break;
    // PRIO_SFCX - Secure Flash Controller for Xbox Int.
    case 0xEA000044:
      memset(&pciBridgeState.PRIO_REG_SFCX.hexData, data, sizeof(pciBridgeState.PRIO_REG_SFCX.hexData) > size ? size : sizeof(pciBridgeState.PRIO_REG_SFCX.hexData));
      pciBridgeState.PRIO_REG_SFCX.intEnabled = enabled;
      pciBridgeState.PRIO_REG_SFCX.latched = latched;
      pciBridgeState.PRIO_REG_SFCX.targetCPU = targetCPU;
      pciBridgeState.PRIO_REG_SFCX.cpuIRQ = cpuIRQ;
      break;
    default:
      LOG_ERROR(PCIBridge, "Unknown reg being written: {:#x}, {:#x}", writeAddress, tmp);
      break;
    }
    return true;
  }

  // Try writing to one of the attached devices
  for (auto &device : connectedPCIDevices) {
    if (device->isAddressMappedInBAR(static_cast<u32>(writeAddress))) {
      // Hit
      device->MemSet(writeAddress, data, size);
      return true;
    }
  }
  return false;
}

void PCIBridge::ConfigRead(u64 readAddress, u8 *data, u64 size) {
  PCIE_CONFIG_ADDR configAddr = {};
  configAddr.hexData = static_cast<u32>(readAddress);

  if (configAddr.busNum == 0 && configAddr.devNum == 0) {
    // Reading from our own config space!
    memcpy(data, &pciBridgeConfig.data[configAddr.regOffset], size);
    return;
  }

  // Current device name
  std::string currentDevName = {};

  switch (configAddr.devNum) {
  case XMA_DEV_NUM:
    currentDevName = "XMA";
    break;
  case CDROM_DEV_NUM:
    currentDevName = "CDROM";
    break;
  case HDD_DEV_NUM:
    currentDevName = "HDD";
    break;
  case OHCI0_DEV_NUM:
    if (configAddr.functNum == 0) {
      currentDevName = "OHCI0";
    } else if (configAddr.functNum == 1) {
      currentDevName = "EHCI0";
    }
    break;
  case OHCI1_DEV_NUM:
    if (configAddr.functNum == 0) {
      currentDevName = "OHCI1";
    } else if (configAddr.functNum == 1) {
      currentDevName = "EHCI1";
    }
    break;
  case FAST_ETH_DEV_NUM:
    currentDevName = "ETHERNET";
    break;
  case SFC_DEV_NUM:
    currentDevName = "SFCX";
    break;
  case AUDIO_CTRLR_DEV_NUM:
    currentDevName = "AUDIOCTRLR";
    break;
  case SMC_DEV_NUM:
    currentDevName = "SMC";
    break;
  case _5841_DEV_NUM:
    currentDevName = "5841";
    break;
  default:
    LOG_ERROR(PCIBridge, "Config Space Read: Unknown device accessed: Dev {:#x}, Reg {:#x}",
        configAddr.devNum, configAddr.regOffset);
    return;
    break;
  }

  for (auto &device : connectedPCIDevices) {
    if (device->GetDeviceName() == currentDevName) {
      // Hit!
      LOG_TRACE(PCIBridge, "Config read, device: {} offset = {:#x}", currentDevName, configAddr.regOffset);
      device->ConfigRead(readAddress, data, size);
      return;
    }
  }

  LOG_ERROR(PCIBridge, "Read to unimplemented device: {}", currentDevName);
  memset(data, 0xFF, size);
}

void PCIBridge::ConfigWrite(u64 writeAddress, const u8 *data, u64 size) {
  PCIE_CONFIG_ADDR configAddr = {};
  configAddr.hexData = static_cast<u32>(writeAddress);

  if (configAddr.busNum == 0 && configAddr.devNum == 0) {
    // Writing to our own config space!
    memcpy(&pciBridgeConfig.data[configAddr.regOffset], data, size);
    return;
  }

  // Current device name
  std::string currentDevName = {};

  switch (configAddr.devNum) {
  case XMA_DEV_NUM:
    currentDevName = "XMA";
    break;
  case CDROM_DEV_NUM:
    currentDevName = "CDROM";
    break;
  case HDD_DEV_NUM:
    currentDevName = "HDD";
    break;
  case OHCI0_DEV_NUM:
    if (configAddr.functNum == 0) {
      currentDevName = "OHCI0";
    } else if (configAddr.functNum == 1) {
      currentDevName = "EHCI0";
    }
    break;
  case OHCI1_DEV_NUM:
    if (configAddr.functNum == 0) {
      currentDevName = "OHCI1";
    } else if (configAddr.functNum == 1) {
      currentDevName = "EHCI1";
    }
    break;
  case FAST_ETH_DEV_NUM:
    currentDevName = "ETHERNET";
    break;
  case SFC_DEV_NUM:
    currentDevName = "SFCX";
    break;
  case AUDIO_CTRLR_DEV_NUM:
    currentDevName = "AUDIOCTRLR";
    break;
  case SMC_DEV_NUM:
    currentDevName = "SMC";
    break;
  case _5841_DEV_NUM:
    currentDevName = "5841";
    break;
  default:
    u64 value = 0;
    memcpy(&value, data, size);
    LOG_ERROR(PCIBridge, "Config Space Write: Unknown device accessed: Dev {:#x} Func {:#x}"
        "Reg {:#x} data = {:#x}", configAddr.devNum, configAddr.functNum, configAddr.regOffset, value);
    return;
    break;
  }

  for (auto &device : connectedPCIDevices) {
    if (device->GetDeviceName() == currentDevName) {
      // Hit!
      u64 value = 0;
      memcpy(&value, data, size);
      LOG_TRACE(PCIBridge, "Config write, device: {}, offset = {:#x} data = {:#x}", currentDevName, configAddr.regOffset, value);
      device->ConfigWrite(writeAddress, data, size);
      return;
    }
  }

  u64 value = 0;
  memcpy(&value, data, size);
  LOG_ERROR(PCIBridge, "Write to unimplemented device: {} data = {:#x}", currentDevName, value);
}
