// Copyright 2025 Xenon Emulator Project

#include "SFCX.h"

#include "Base/Logging/Log.h"
                                                                          
SFCX::SFCX(const char* deviceName, const std::string nandLoadPath, u64 size,
  PCIBridge *parentPCIBridge) : PCIDevice(deviceName, size) {
  // Asign parent PCI Bridge pointer.
  parentBus = parentPCIBridge;

  // Set PCI Properties.
  pciConfigSpace.configSpaceHeader.reg0.hexData = 0x580B1414;
  pciConfigSpace.configSpaceHeader.reg1.hexData = 0x02000006;
  pciConfigSpace.configSpaceHeader.reg2.hexData = 0x05010001;

  // Set our PCI Dev Sizes.
  pciDevSizes[0] = 0x400; // BAR0
  pciDevSizes[1] = 0x1000000; // BAR1

  LOG_INFO(SFCX, "Xenon Secure Flash Controller for Xbox.");

  // Set the registers as a dump from my Corona 16MB. These were dumped at POR
  // via Xell before SFCX Init. These are also readable via JRunner and
  // simple360 flasher.

  // Xenon Dev Kit ES DD2 64 MB
  // 0x01198030

  // Corona 16 Megs Retail
  // 0x000043000

  sfcxState.configReg = 0x00043000; // Config Reg is VERY Important. Tells info
                                    // about Meta/NAND Type.
  sfcxState.statusReg = 0x00000600;
  sfcxState.statusReg = 0x00000600;
  sfcxState.addressReg = 0x00F70030;
  sfcxState.logicalReg = 0x00000100;
  sfcxState.physicalReg = 0x0000100;
  sfcxState.commandReg = NO_CMD;

  // Load the NAND dump.
  LOG_INFO(SFCX, "Loading NAND from path: {}", nandLoadPath);

  nandFile.open(nandLoadPath, std::ios_base::in | std::ios_base::binary);
                  
  if (!nandFile.is_open()) {
    LOG_CRITICAL(SFCX, "Fatal error! Please make sure your NAND (or NAND path) is valid!");
    SYSTEM_PAUSE();
  }

  // Check file magic.
  if (!checkMagic()) {
    LOG_CRITICAL(SFCX, "Fatal error! The loaded 'nand.bin' doesn't correspond to a Xbox 360 NAND.");
    SYSTEM_PAUSE();
  }

  // Load NAND header and display info about it.
  nandFile.seekg(0, std::ios::beg);
  nandFile.read(reinterpret_cast<char*>(&sfcxState.nandHeader), sizeof(sfcxState.nandHeader));
  nandFile.close();

  // Fix Endiannes
  sfcxState.nandHeader.nandMagic = byteswap_be<u16>(sfcxState.nandHeader.nandMagic);
  LOG_INFO(SFCX, " * NAND Magic: {:#x}", sfcxState.nandHeader.nandMagic);

  sfcxState.nandHeader.build = byteswap_be<u16>(sfcxState.nandHeader.build);
  LOG_INFO(SFCX, " * Build: {:#x}", sfcxState.nandHeader.build);

  sfcxState.nandHeader.qfe = byteswap_be<u16>(sfcxState.nandHeader.qfe);
  LOG_INFO(SFCX, " * QFE: {:#x}", sfcxState.nandHeader.qfe);

  sfcxState.nandHeader.flags = byteswap_be<u16>(sfcxState.nandHeader.flags);
  LOG_INFO(SFCX, " * Flags: {:#x}", sfcxState.nandHeader.flags);

  sfcxState.nandHeader.entry = byteswap_be<u32>(sfcxState.nandHeader.entry);
  LOG_INFO(SFCX, " * Entry: {:#x}", sfcxState.nandHeader.entry);

  sfcxState.nandHeader.size = byteswap_be<u32>(sfcxState.nandHeader.size);
  LOG_INFO(SFCX, " * Size: {:#x}", sfcxState.nandHeader.size);

  sfcxState.nandHeader.keyvaultSize = byteswap_be<u32>(sfcxState.nandHeader.keyvaultSize);
  LOG_INFO(SFCX, " * Keyvault Size: {:#x}", sfcxState.nandHeader.keyvaultSize);

  sfcxState.nandHeader.sysUpdateAddr = byteswap_be<u32>(sfcxState.nandHeader.sysUpdateAddr);
  LOG_INFO(SFCX, " * System Update Addr: {:#x}", sfcxState.nandHeader.sysUpdateAddr);

  sfcxState.nandHeader.sysUpdateCount = byteswap_be<u16>(sfcxState.nandHeader.sysUpdateCount);
  LOG_INFO(SFCX, " * System Update Count: {:#x}", sfcxState.nandHeader.sysUpdateCount);

  sfcxState.nandHeader.keyvaultVer = byteswap_be<u16>(sfcxState.nandHeader.keyvaultVer);
  LOG_INFO(SFCX, " * Keyvault Ver: {:#x}", sfcxState.nandHeader.keyvaultVer);

  sfcxState.nandHeader.keyvaultAddr = byteswap_be<u32>(sfcxState.nandHeader.keyvaultAddr);
  LOG_INFO(SFCX, " * Keyvault Addr: {:#x}", sfcxState.nandHeader.keyvaultAddr);

  sfcxState.nandHeader.sysUpdateSize = byteswap_be<u32>(sfcxState.nandHeader.sysUpdateSize);
  LOG_INFO(SFCX, " * System Update Size: {:#x}", sfcxState.nandHeader.sysUpdateSize);

  sfcxState.nandHeader.smcConfigAddr = byteswap_be<u32>(sfcxState.nandHeader.smcConfigAddr);  
  LOG_INFO(SFCX, " * SMC Config Addr: {:#x}", sfcxState.nandHeader.smcConfigAddr);

  sfcxState.nandHeader.smcBootSize = byteswap_be<u32>(sfcxState.nandHeader.smcBootSize);
  LOG_INFO(SFCX, " * SMC Boot Size: {:#x}", sfcxState.nandHeader.smcBootSize);

  sfcxState.nandHeader.smcBootAddr = byteswap_be<u32>(sfcxState.nandHeader.smcBootAddr);
  LOG_INFO(SFCX, " * SMC Boot Addr: {:#x}", sfcxState.nandHeader.smcBootAddr);

  // Check Image size and Meta type.
  size_t imageSize = std::filesystem::file_size(nandLoadPath);

  // There are two SFCX Versions, original (Pre Jasper) and Jasper+.

  // Enter SFCX Thread.
  sfcxThreadRunning = true;
  sfcxThread = std::thread(&SFCX::sfcxMainLoop, this);
}

SFCX::~SFCX() {
  sfcxThreadRunning = false;
  if (sfcxThread.joinable())
    sfcxThread.join();
}

void SFCX::Read(u64 readAddress, u8 *data, u8 byteCount) {
  const u16 reg = readAddress & 0xFF;

  switch (reg) {
  case SFCX_CONFIG_REG:
    *reinterpret_cast<u32*>(data) = sfcxState.configReg;
    break;
  case SFCX_STATUS_REG:
    *reinterpret_cast<u32*>(data) = sfcxState.statusReg;
    break;
  case SFCX_COMMAND_REG:
    *reinterpret_cast<u32*>(data) = sfcxState.commandReg;
    break;
  case SFCX_ADDRESS_REG:
    *reinterpret_cast<u32*>(data) = sfcxState.addressReg;
    break;
  case SFCX_DATA_REG:
    *reinterpret_cast<u32*>(data) = sfcxState.dataReg;
    break;
  case SFCX_LOGICAL_REG:
    *reinterpret_cast<u32*>(data) = sfcxState.logicalReg;
    break;
  case SFCX_PHYSICAL_REG:
    *reinterpret_cast<u32*>(data) = sfcxState.physicalReg;
    break;
  case SFCX_DATAPHYADDR_REG:
    *reinterpret_cast<u32*>(data) = sfcxState.dataPhysAddrReg;
    break;
  case SFCX_SPAREPHYADDR_REG:
    *reinterpret_cast<u32*>(data) = sfcxState.sparePhysAddrReg;
    break;
  case SFCX_MMC_ID_REG:
    *reinterpret_cast<u32*>(data) = sfcxState.mmcIDReg;
    break;
  default:
    LOG_ERROR(SFCX, "Read from unknown register {:#x}", reg);
    break;
  }
}

void SFCX::ConfigRead(u64 readAddress, u8 *data, u8 byteCount) {
  const u8 offset = readAddress & 0xFF;
  memcpy(data, &pciConfigSpace.data[offset], byteCount);
}

void SFCX::Write(u64 writeAddress, u8 *data, u8 byteCount) {
  const u16 reg = writeAddress & 0xFF;

  switch (reg) {
  case SFCX_CONFIG_REG:
    sfcxState.configReg = *reinterpret_cast<u32*>(data);
    break;
  case SFCX_STATUS_REG:
    sfcxState.statusReg = *reinterpret_cast<u32*>(data);
    break;
  case SFCX_COMMAND_REG:
    sfcxState.commandReg = *reinterpret_cast<u32*>(data);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    break;
  case SFCX_ADDRESS_REG:
    sfcxState.addressReg = *reinterpret_cast<u32*>(data);
    break;
  case SFCX_DATA_REG:
    sfcxState.dataReg = *reinterpret_cast<u32*>(data);
    break;
  case SFCX_LOGICAL_REG:
    sfcxState.logicalReg = *reinterpret_cast<u32*>(data);
    break;
  case SFCX_PHYSICAL_REG:
    sfcxState.physicalReg = *reinterpret_cast<u32*>(data);
    break;
  case SFCX_DATAPHYADDR_REG:
    sfcxState.dataPhysAddrReg = *reinterpret_cast<u32*>(data);
    break;
  case SFCX_SPAREPHYADDR_REG:
    sfcxState.sparePhysAddrReg = *reinterpret_cast<u32*>(data);
    break;
  case SFCX_MMC_ID_REG:
    sfcxState.mmcIDReg = *reinterpret_cast<u32*>(data);
    break;
  default:
    LOG_ERROR(SFCX, "Write from unknown register {:#x}", reg);
    break;
  }
}

void SFCX::ConfigWrite(u64 writeAddress, u8 *data, u8 byteCount) {
  const u8 offset = writeAddress & 0xFF;

  // Check if we're being scanned.
  if (static_cast<u8>(writeAddress) >= 0x10 && static_cast<u8>(writeAddress) < 0x34) {
    const u32 regOffset = (static_cast<u8>(writeAddress) - 0x10) >> 2;
    if (pciDevSizes[regOffset] != 0) {
      if (*reinterpret_cast<u64*>(data) == 0xFFFFFFFF) { // PCI BAR Size discovery.
        u64 x = 2;
        for (int idx = 2; idx < 31; idx++) {
          *reinterpret_cast<u64*>(data) &= ~x;
          x <<= 1;
          if (x >= pciDevSizes[regOffset]) {
            break;
          }
        }
        *reinterpret_cast<u64*>(data) &= ~0x3;
      }
    }
    if (static_cast<u8>(writeAddress) == 0x30) { // Expansion ROM Base Address.
      *reinterpret_cast<u64*>(data) = 0; // Register not implemented.
    }
  }

  memcpy(&pciConfigSpace.data[offset], data, byteCount);
}

void SFCX::sfcxMainLoop() {
  // Config register should be initialized by now.
  while (sfcxThreadRunning) {
    // Did we got a command?
    if (sfcxState.commandReg != NO_CMD) {
      // Set status to busy.
      sfcxState.statusReg |= STATUS_BUSY;

      // Check the command reg to see what command was issued.
      switch (sfcxState.commandReg) {
      case PAGE_BUF_TO_REG:
        // If we're reading from data buffer to data reg the Address reg becomes
        // our buffer pointer.
        memcpy(&sfcxState.dataReg, &sfcxState.pageBuffer[sfcxState.addressReg],
               4);
        sfcxState.addressReg += 4;
        break;
      // case REG_TO_PAGE_BUF:
      //	break;
      // case LOG_PAGE_TO_BUF:
      //	break;
      case PHY_PAGE_TO_BUF:
        // Read Phyisical page into page buffer.
        // Physical pages are 0x210 bytes long, logical page (0x200) + meta data
        // (0x10).
        LOG_DEBUG(SFCX, "Reading from address {:#x} (reading {:#x} bytes)", sfcxState.addressReg, sizeof(sfcxState.pageBuffer));
        nandFile.seekg(sfcxState.addressReg);
        nandFile.read(reinterpret_cast<char*>(sfcxState.pageBuffer), sizeof(sfcxState.pageBuffer));
        // Issue Interrupt.
        if (sfcxState.configReg & CONFIG_INT_EN) {
          // Set a delay for our interrupt?
          std::this_thread::sleep_for(std::chrono::milliseconds(150));
          parentBus->RouteInterrupt(PRIO_SFCX);
          sfcxState.statusReg |= STATUS_INT_CP;
        }
        break;
      // case WRITE_PAGE_TO_PHY:
      //	break;
      // case BLOCK_ERASE:
      //	break;
      // case DMA_LOG_TO_RAM:
      //	break;
      // case DMA_PHY_TO_RAM:
      //	break;
      // case DMA_RAM_TO_PHY:
      //	break;
      // case UNLOCK_CMD_0:
      //	break;
      // case UNLOCK_CMD_1:
      //	break;
      default:
        LOG_ERROR(SFCX, "Unrecognized command was issued. {:#x}", sfcxState.commandReg);
        break;
      }

      // Clear Command Register.
      sfcxState.commandReg = NO_CMD;

      // Set Status to Ready again.
      sfcxState.statusReg &= ~STATUS_BUSY;
    }
    // Sleep for some time.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

bool SFCX::checkMagic() {
  char magic[2];

  nandFile.read(reinterpret_cast<char*>(magic), sizeof(magic));

  // Retail Nand Magic is 0xFF4F.
  // Devkit Nand Magic is 0x0F4F.
  // Older Devkit Nand's magic is 0x0F3F.

  if (magic[0] == (char)0xFF && magic[1] == (char)0x4F) {
    LOG_INFO(SFCX, "Retail NAND Magic found.");
    return true;
  }
  if (magic[0] == (char)0x0F && magic[1] == (char)0x4F) {
    LOG_INFO(SFCX, "Devkit NAND Magic found.");
    return true;
  }
  if (magic[0] == (char)0x0F && magic[1] == (char)0x3F) {
    LOG_INFO(SFCX, "Old Devkit NAND Magic found.");
    return true;
  }
  return false;
}
