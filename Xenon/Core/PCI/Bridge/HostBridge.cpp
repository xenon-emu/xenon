/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "HostBridge.h"

#include "Base/Logging/Log.h"
#include "Base/Global.h"

HostBridge::HostBridge(u64 ramSize) {
  // TODO: Fix these to pull the right data
  switch (Config::highlyExperimental.consoleRevison) {
  case Config::eConsoleRevision::Xenon:
    // Device/Vendor ID
    hostBridgeConfigSpace.reg0.hexData = 0x58301414;
    // Device Type/Revision
    hostBridgeConfigSpace.reg1.hexData = 0x06000010;
    break;
  case Config::eConsoleRevision::Zephyr:
    // Device/Vendor ID
    hostBridgeConfigSpace.reg0.hexData = 0x58301414;
    // Device Type/Revision
    hostBridgeConfigSpace.reg1.hexData = 0x06000010;
    break;
  case Config::eConsoleRevision::Falcon:
    // Device/Vendor ID
    hostBridgeConfigSpace.reg0.hexData = 0x58301414;
    // Device Type/Revision
    hostBridgeConfigSpace.reg1.hexData = 0x06000010;
    break;
  case Config::eConsoleRevision::Jasper:
    // Device/Vendor ID
    hostBridgeConfigSpace.reg0.hexData = 0x58301414;
    // Device Type/Revision
    hostBridgeConfigSpace.reg1.hexData = 0x06000010;
    break;
  case Config::eConsoleRevision::Trinity:
    // Device/Vendor ID
    hostBridgeConfigSpace.reg0.hexData = 0x58301414;
    // Device Type/Revision
    hostBridgeConfigSpace.reg1.hexData = 0x06000010;
    break;
  case Config::eConsoleRevision::Corona:
    // Device/Vendor ID
    hostBridgeConfigSpace.reg0.hexData = 0x58301414;
    // Device Type/Revision
    hostBridgeConfigSpace.reg1.hexData = 0x06000010;
    break;
  case Config::eConsoleRevision::Corona4GB:
    // Device/Vendor ID
    hostBridgeConfigSpace.reg0.hexData = 0x58301414;
    // Device Type/Revision
    hostBridgeConfigSpace.reg1.hexData = 0x06000010;
    break;
  case Config::eConsoleRevision::Winchester:
    // Device/Vendor ID
    hostBridgeConfigSpace.reg0.hexData = 0x58301414;
    // Device Type/Revision
    hostBridgeConfigSpace.reg1.hexData = 0x06000010;
    break;
  }

  // 0x20000000
  hostBridgeConfigSpace.BAR0 = 0xE0010000;
  hostBridgeConfigSpace.BAR1 = 0xE0030000;
  hostBridgeConfigSpace.BAR2 = 0xE1010000;
  hostBridgeConfigSpace.BAR3 = 0xE1030000;
  hostBridgeConfigSpace.BAR4 = 0xE2010000;
  hostBridgeConfigSpace.BAR5 = 0xE2030000;
  biuRegs.RAMSize = ramSize;
}

HostBridge::~HostBridge() {
  if (!lifetimeGuard.RetireAndWait()) {
    LOG_CRITICAL(HostBridge, "Timed out waiting for in-flight accesses to drain during destruction!");
  }

  xGPU.reset();
  pciBridge.reset();
}

std::weak_ptr<Xe::Xenos::XGPU> HostBridge::RegisterXGPU(std::unique_ptr<Xe::Xenos::XGPU> xgpu) {
  std::lock_guard lck(mutex);
  xGPU = std::move(xgpu);
  return xGPU;
}

std::weak_ptr<PCIBridge> HostBridge::RegisterPCIBridge(std::unique_ptr<PCIBridge> bridge) {
  std::lock_guard lck(mutex);
  pciBridge = std::move(bridge);
  return pciBridge;
}

bool HostBridge::Read(u64 address, u8 *data, u64 size) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "HostBridge::Read", MP_AUTO);
  auto lease = lifetimeGuard.TryAcquire();
  if (!lease) {
    return false;
  }

  // Reading from host bridge registers?
  if (IsAddressMappedinBAR(static_cast<u32>(address))) {
    switch (address) {
    // HostBridge
    case 0xE0020000:
      memcpy(data, &hostBridgeRegs.REG_E0020000, size);
      break;
    case 0xE0020004:
      memcpy(data, &hostBridgeRegs.REG_E0020004, size);
      break;
    // BIU
    case 0xE1020004:
      memcpy(data, &biuRegs.REG_E1020004, size);
      break;
    case 0xE1010010:
      memcpy(data, &biuRegs.REG_E1010010, size);
      break;
    case 0xE1018000:
      memcpy(data, &biuRegs.REG_E1018000, size);
      break;
    case 0xE1020000:
      memcpy(data, &biuRegs.REG_E1020000, size);
      break;
    case 0xE1040000:
      memcpy(data, &biuRegs.RAMSize, size);
      break;
    default:
      memset(data, 0, size);
      LOG_ERROR(HostBridge, "Unknown register being read at 0x{:X}", address);
      break;
    }
    return true;
  }

  // Check if this address is in the PCI Bridge
  if (xGPU->IsAddressMappedInBAR(static_cast<u32>(address))) {
    xGPU->Read(address, data, size);
    return true;
  }

  // Check if this address is in the PCI Bridge
  if (pciBridge->IsAddressMappedinBAR(static_cast<u32>(address))) {
    pciBridge->Read(address, data, size);
    return true;
  }

  // Read failed or address is not on this bus
  return false;
}

bool HostBridge::Write(u64 address, const u8 *data, u64 size) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "HostBridge::Write", MP_AUTO);
  auto lease = lifetimeGuard.TryAcquire();
  if (!lease) {
    return false;
  }

  // If we are not UART, send it to log
  if (false) {
    std::stringstream ss{};
    for (u64 i = 0; i != size; i++) {
      ss << FMT("0x{:02X}{}", static_cast<u16>(data[i]), i != (size - 1) ? " " : "");
    }
    LOG_DEBUG(HostBridge, "Address: 0x{:X} | Data({},0x{:X}): {}", address, size, size, ss.str());
  }

  // Writing to host bridge registers?
  if (IsAddressMappedinBAR(static_cast<u32>(address))) {
    switch (address) {
    // HostBridge
    case 0xE0020000:
      memcpy(&hostBridgeRegs.REG_E0020000, data, size);
      break;
    case 0xE0020004:
      memcpy(&hostBridgeRegs.REG_E0020004, data, size);
      break;
    // BIU
    case 0xE1003000:
      memcpy(&biuRegs.REG_E1003000, data, size);
      break;
    case 0xE1003100:
      memcpy(&biuRegs.REG_E1003100, data, size);
      break;
    case 0xE1003200:
      memcpy(&biuRegs.REG_E1003200, data, size);
      break;
    case 0xE1003300:
      memcpy(&biuRegs.REG_E1003300, data, size);
      break;
    case 0xE1010000:
      memcpy(&biuRegs.REG_E1010000, data, size);
      // Reading to this addr on a retail returns
      // the same data on this address
      memcpy(&biuRegs.REG_E1010010, data, size);
      break;
    case 0xE1010010:
      memcpy(&biuRegs.REG_E1010010, data, size);
      break;
    case 0xE1010020:
      memcpy(&biuRegs.REG_E1010020, data, size);
      break;
    case 0xE1013000:
      memcpy(&biuRegs.REG_E1013000, data, size);
      break;
    case 0xE1013100:
      memcpy(&biuRegs.REG_E1013100, data, size);
      break;
    case 0xE1013200:
      memcpy(&biuRegs.REG_E1013200, data, size);
      break;
    case 0xE1013300:
      memcpy(&biuRegs.REG_E1013300, data, size);
      break;
    case 0xE1018020:
      // Read the comment on E1010000
      memcpy(&biuRegs.REG_E1018000, data, size);
      memcpy(&biuRegs.REG_E1018020, data, size);
      break;
    case 0xE1020000:
      memcpy(&biuRegs.REG_E1020000, data, size);
      break;
    case 0xE1020004:
      memcpy(&biuRegs.REG_E1020004, data, size);
      break;
    case 0xE1020008:
      memcpy(&biuRegs.REG_E1020008, data, size);
      break;
    case 0xE1040000:
      memcpy(&biuRegs.RAMSize, data, size);
      break;
    case 0xE1040074:
      memcpy(&biuRegs.REG_E1040074, data, size);
      break;
    case 0xE1040078:
      memcpy(&biuRegs.REG_E1040078, data, size);
      break;
    default: {
      u64 tmp = 0;
      memcpy(&tmp, data, size);
      LOG_ERROR(HostBridge, "Unknown register being written at 0x{:X} with value '0x{:X}'", address, tmp);
    } break;
    }
    return true;
  }

  // Check if this address is mapped on the GPU
  if (xGPU->IsAddressMappedInBAR(static_cast<u32>(address))) {
    xGPU->Write(address, data, size);
    return true;
  }

  // Check if this address is in the PCI Bridge
  if (pciBridge->IsAddressMappedinBAR(static_cast<u32>(address))) {
    pciBridge->Write(address, data, size);
    return true;
  }

  // Write failed or address is not on this bus
  return false;
}

bool HostBridge::MemSet(u64 address, s32 data, u64 size) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "HostBridge::MemSet", MP_AUTO);
  auto lease = lifetimeGuard.TryAcquire();
  if (!lease) {
    return false;
  }

  // Writing to host bridge registers?
  if (IsAddressMappedinBAR(static_cast<u32>(address))) {
    switch (address) {
    // HostBridge
    case 0xE0020000:
      memset(&hostBridgeRegs.REG_E0020000, data, size);
      break;
    case 0xE0020004:
      memset(&hostBridgeRegs.REG_E0020004, data, size);
      break;
    // BIU
    case 0xE1003000:
      memset(&biuRegs.REG_E1003000, data, size);
      break;
    case 0xE1003100:
      memset(&biuRegs.REG_E1003100, data, size);
      break;
    case 0xE1003200:
      memset(&biuRegs.REG_E1003200, data, size);
      break;
    case 0xE1003300:
      memset(&biuRegs.REG_E1003300, data, size);
      break;
    case 0xE1010000:
      memset(&biuRegs.REG_E1010000, data, size);
      // Reading to this addr on a retail returns
      // the same data on this address
      memset(&biuRegs.REG_E1010010, data, size);
      break;
    case 0xE1010010:
      memset(&biuRegs.REG_E1010010, data, size);
      break;
    case 0xE1010020:
      memset(&biuRegs.REG_E1010020, data, size);
      break;
    case 0xE1013000:
      memset(&biuRegs.REG_E1013000, data, size);
      break;
    case 0xE1013100:
      memset(&biuRegs.REG_E1013100, data, size);
      break;
    case 0xE1013200:
      memset(&biuRegs.REG_E1013200, data, size);
      break;
    case 0xE1013300:
      memset(&biuRegs.REG_E1013300, data, size);
      break;
    case 0xE1018020:
      // Read the comment on E1010000
      memset(&biuRegs.REG_E1018000, data, size);
      memset(&biuRegs.REG_E1018020, data, size);
      break;
    case 0xE1020000:
      memset(&biuRegs.REG_E1020000, data, size);
      break;
    case 0xE1020004:
      memset(&biuRegs.REG_E1020004, data, size);
      break;
    case 0xE1020008:
      memset(&biuRegs.REG_E1020008, data, size);
      break;
    case 0xE1040000:
      memset(&biuRegs.RAMSize, data, size);
      break;
    case 0xE1040074:
      memset(&biuRegs.REG_E1040074, data, size);
      break;
    case 0xE1040078:
      memset(&biuRegs.REG_E1040078, data, size);
      break;
    default: {
      u64 tmp = 0;
      memset(&tmp, data, size);
      LOG_ERROR(HostBridge, "Unknown register being written at address 0x{:X} with value '0x{:X}'", address, tmp);
    } break;
    }
    return true;
  }

  // Check if this address is mapped on the GPU
  if (xGPU->IsAddressMappedInBAR(static_cast<u32>(address))) {
    xGPU->MemSet(address, data, size);
    return true;
  }

  // Check if this address is in the PCI Bridge
  if (pciBridge->IsAddressMappedinBAR(static_cast<u32>(address))) {
    pciBridge->MemSet(address, data, size);
    return true;
  }

  // Write failed or address is not on this bus
  return false;
}

bool HostBridge::ConfigRead(u64 address, u8 *data, u64 size) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "HostBridge::ConfigRead", MP_AUTO);
  auto lease = lifetimeGuard.TryAcquire();
  if (!lease) {
    return false;
  }

  PCIE_CONFIG_ADDR configAddress = {};
  configAddress.hexData = static_cast<u32>(address);

  if (configAddress.busNumber == 0) {
    switch (configAddress.deviceNumber) {
    case 0x0: // PCI-PCI Bridge
      pciBridge->ConfigRead(address, data, size);
      break;
    case 0x1: // Host Bridge
      memcpy(data, &hostBridgeConfigSpace.data[configAddress.regOffset], size);
      break;
    case 0x2: // GPU + Memory Controller
      xGPU->ConfigRead(address, data, size);
      break;
    default:
      LOG_ERROR(HostBridge, "BUS0: Config read to a non-existant PCI bridge at address 0x{:X}", address);
      break;
    }
    return true;
  }

  // Config address belongs to a secondary bus,
  //  let's send it to the guest PCI-PCI bridge
  return pciBridge->ConfigRead(address, data, size);
}

bool HostBridge::ConfigWrite(u64 address, const u8 *data, u64 size) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "HostBridge::ConfigWrite", MP_AUTO);
  auto lease = lifetimeGuard.TryAcquire();
  if (!lease) {
    return false;
  }

  PCIE_CONFIG_ADDR configAddress = { static_cast<u32>(address) };
  if (configAddress.busNumber == 0) {
    switch (configAddress.deviceNumber) {
    // PCI-PCI Bridge
    case 0x0:
      pciBridge->ConfigWrite(address, data, size);
      break;
    // Host Bridge
    case 0x1:
      memcpy(&hostBridgeConfigSpace.data[configAddress.regOffset], data, size);
      break;
    // GPU/Memory Controller
    case 0x2:
      xGPU->ConfigWrite(address, data, size);
      break;
    default: {
      u64 tmp = 0;
      memcpy(&tmp, data, size);
      LOG_ERROR(HostBridge, "BUS0: Config write to a non-existant PCI bridge at address 0x{:X} with data '0x{:X}'", address, tmp);
    } break;
    }
    return true;
  }

  // Config address belongs to a secondary bus,
  //  let's send it to the PCI-PCI bridge
  return pciBridge->ConfigWrite(address, data, size);
}

#define ADDRESS_BOUNDS_CHECK(a, b) (address >= a && address <= (a + b))
bool HostBridge::IsAddressMappedinBAR(u32 address) {
  if (ADDRESS_BOUNDS_CHECK(hostBridgeConfigSpace.BAR0, XGPU_DEVICE_SIZE) ||
      ADDRESS_BOUNDS_CHECK(hostBridgeConfigSpace.BAR1, XGPU_DEVICE_SIZE) ||
      ADDRESS_BOUNDS_CHECK(hostBridgeConfigSpace.BAR2, XGPU_DEVICE_SIZE) ||
      ADDRESS_BOUNDS_CHECK(hostBridgeConfigSpace.BAR3, XGPU_DEVICE_SIZE) ||
      ADDRESS_BOUNDS_CHECK(hostBridgeConfigSpace.BAR4, XGPU_DEVICE_SIZE) ||
      ADDRESS_BOUNDS_CHECK(hostBridgeConfigSpace.BAR5, XGPU_DEVICE_SIZE)
    )
  {
    return true;
  }

  return false;
}