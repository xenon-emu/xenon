// Copyright 2025 Xenon Emulator Project

#include "SFCX.h"

#include "Base/Logging/Log.h"
#include "Base/Config.h"

//#define SFCX_DEBUG

// There are two SFCX Versions, pre-Jasper and post-Jasper
SFCX::SFCX(const char* deviceName, const std::string nandLoadPath, u64 size,
  PCIBridge* parentPCIBridge, RAM* ram) : PCIDevice(deviceName, size) {
  // Asign parent PCI Bridge and RAM pointer.
  parentBus = parentPCIBridge;
  mainMemory = ram;

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

  sfcxState.configReg = 0x000043000; // Config Reg is VERY Important. Tells info
  // about Meta/NAND Type.
  sfcxState.statusReg = 0x00000600;
  sfcxState.statusReg = 0x00000600;
  sfcxState.addressReg = 0x00F70030;
  sfcxState.logicalReg = 0x00000100;
  sfcxState.physicalReg = 0x0000100;
  sfcxState.commandReg = NO_CMD;

  // Display Flash config.
  LOG_INFO(SFCX, "FlashConfig: {:#x}", sfcxState.configReg);

  // Load the NAND dump.
  LOG_INFO(SFCX, "Loading NAND from path: {}", nandLoadPath);

  // Check Image size.
  u64 imageSize = 0;

  // fs::file_size can cause a exception if it is not a valid file
  try {
    std::error_code ec;
    imageSize = std::filesystem::file_size(nandLoadPath, ec);
    if (imageSize == -1 || !imageSize) {
      imageSize = 0;
      LOG_ERROR(Base_Filesystem, "Failed to retrieve the file size of {} (Error: {})", nandLoadPath, ec.message());
    }
  }
  catch (const std::exception& ex) {
    LOG_ERROR(Base_Filesystem, "Exception trying to get file size. Exception: {}",
      ex.what());
    return;
  }

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

  // Get CB_A and CB_B headers.
  BL_HEADER cbaHeader;
  BL_HEADER cbbHeader;

  // Get CB_A offset.
  u32 cbaOffset = sfcxState.nandHeader.entry;
  cbaOffset = 1 ? ((cbaOffset / 0x200) * 0x210) + cbaOffset % 0x200 : cbaOffset;

  // Read CB_A Header.
  nandFile.seekg(cbaOffset, std::ios::beg);
  nandFile.read(reinterpret_cast<char*>(&cbaHeader), sizeof(cbaHeader));

  // Byteswap CB_A info from header.
  cbaHeader.buildNumber = byteswap_be<u16>(cbaHeader.buildNumber);
  cbaHeader.lenght = byteswap_be<u32>(cbaHeader.lenght);
  cbaHeader.entryPoint = byteswap_be<u32>(cbaHeader.entryPoint);

  // Get CB_B offset.
  u32 cbbOffset = sfcxState.nandHeader.entry + cbaHeader.lenght;
  cbbOffset = 1 ? ((cbbOffset / 0x200) * 0x210) + cbbOffset % 0x200 : cbbOffset;

  // Read CB_B Header.
  nandFile.seekg(cbbOffset, std::ios::beg);
  nandFile.read(reinterpret_cast<char*>(&cbbHeader), sizeof(cbbHeader));

  // Byteswap CB_B info from header.
  cbbHeader.buildNumber = byteswap_be<u16>(cbbHeader.buildNumber);
  cbbHeader.lenght = byteswap_be<u32>(cbbHeader.lenght);
  cbbHeader.entryPoint = byteswap_be<u32>(cbbHeader.entryPoint);

  // Check for CB(_A) Magic
  if (cbaHeader.name[0] == 'C' && cbaHeader.name[1] == 'B') {
    LOG_INFO(SFCX, "Found CB(_A) Header: Physical: {:#x}, LBA: {:#x}", sfcxState.nandHeader.entry, cbaOffset);
    LOG_INFO(SFCX, " * CB Entry: {:#x}", cbaHeader.entryPoint);
    LOG_INFO(SFCX, " * CB Lenght: {:#x}", cbaHeader.lenght);
  }

  // Check for CB(_B) Magic
  if (cbbHeader.name[0] == 'C' && cbbHeader.name[1] == 'B') {
    LOG_INFO(SFCX, "Found CB(_B) Header: Physical: {:#x}, LBA: {:#x}", sfcxState.nandHeader.entry + cbaHeader.lenght, cbbOffset);
    LOG_INFO(SFCX, " * CB Entry: {:#x}", cbbHeader.entryPoint);
    LOG_INFO(SFCX, " * CB Lenght: {:#x}", cbbHeader.lenght);
  }

  u32 cbVersion = (cbaHeader.buildNumber == cbbHeader.buildNumber) ? cbaHeader.buildNumber : cbbHeader.buildNumber;

  if (cbaHeader.buildNumber == cbbHeader.buildNumber) {
    LOG_INFO(SFCX, "Detected Unified CB: ");
    LOG_INFO(SFCX, "   * CB Version: {:#d}", cbVersion);
  }
  else {
    LOG_INFO(SFCX, "Detected Split CB:");
    LOG_INFO(SFCX, " * CB_A Version: {:#d}", cbaHeader.buildNumber);
    LOG_INFO(SFCX, " * CB_B Version: {:#d}", cbbHeader.buildNumber);
  }

  if (Config::xcpu.SKIP_HW_INIT) {
    LOG_INFO(SFCX, "CB/SB Hardware Init stage skip enabled.");
    if (Config::xcpu.HW_INIT_SKIP_1 == 0 && Config::xcpu.HW_INIT_SKIP_2 == 0) {
      LOG_INFO(SFCX, "Auto-detecting Hardware Init stage skip addresses:");
      switch (cbVersion) {
        // CB_B 6723
      case 6723:
        Config::xcpu.HW_INIT_SKIP_1 = 0x03009B10;
        LOG_INFO(SFCX, " > CB({:#d}): Skip Address 1 set to: {:#x}", cbVersion, Config::xcpu.HW_INIT_SKIP_1);
        Config::xcpu.HW_INIT_SKIP_2 = 0x03009BA4;
        LOG_INFO(SFCX, " > CB({:#d}): Skip Address 2 set to: {:#x}", cbVersion, Config::xcpu.HW_INIT_SKIP_2);
        break;
        // CB_B 9188
      case 9188:
        Config::xcpu.HW_INIT_SKIP_1 = 0x03003DC0;
        LOG_INFO(SFCX, " > CB({:#d}): Skip Address 1 set to: {:#x}", cbVersion, Config::xcpu.HW_INIT_SKIP_1);
        Config::xcpu.HW_INIT_SKIP_2 = 0x03003E54;
        LOG_INFO(SFCX, " > CB({:#d}): Skip Address 2 set to: {:#x}", cbVersion, Config::xcpu.HW_INIT_SKIP_2);
        break;
        // CB_B 14352, 15432
      case 14352:
      case 15432:
        Config::xcpu.HW_INIT_SKIP_1 = 0x03003F48;
        LOG_INFO(SFCX, " > CB({:#d}): Skip Address 1 set to: {:#x}", cbVersion, Config::xcpu.HW_INIT_SKIP_1);
        Config::xcpu.HW_INIT_SKIP_2 = 0x03003FDC;
        LOG_INFO(SFCX, " > CB({:#d}): Skip Address 2 set to: {:#x}", cbVersion, Config::xcpu.HW_INIT_SKIP_2);
        break;
      default:
        LOG_ERROR(SFCX, "Auto detection failed. Unimplemented CB found, version {:#d}. Please report to Xenon Devs.", cbVersion);
        break;
      }
    }
    else {
      LOG_INFO(SFCX, "Manual Hardware Init stage skip addresses set:");
      LOG_INFO(SFCX, " > CB({:#d}): Skip Address 1 set to: {:#x}", cbVersion, Config::xcpu.HW_INIT_SKIP_1);
      LOG_INFO(SFCX, " > CB({:#d}): Skip Address 2 set to: {:#x}", cbVersion, Config::xcpu.HW_INIT_SKIP_2);
    }
  }
  // Enter SFCX Thread.
  sfcxThreadRunning = true;
  sfcxThread = std::thread(&SFCX::sfcxMainLoop, this);
}

SFCX::~SFCX() {
  // Close NAND I/O stream.
  nandFile.close();
  // Terminate thread.
  sfcxThreadRunning = false;
  if (sfcxThread.joinable())
    sfcxThread.join();
}

void SFCX::Read(u64 readAddress, u8* data, u64 size) {
  // Set a lock on registers.
  std::lock_guard lck(mutex);

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

void SFCX::ConfigRead(u64 readAddress, u8* data, u64 size) {
  const u8 offset = readAddress & 0xFF;
  memcpy(data, &pciConfigSpace.data[offset], size);
}

void SFCX::Write(u64 writeAddress, const u8* data, u64 size) {
  // Set a lock on registers.
  std::lock_guard lck(mutex);

  const u16 reg = writeAddress & 0xFF;

  // Command register.
  u32 command = NO_CMD;

  switch (reg) {
  case SFCX_CONFIG_REG:
    memcpy(&sfcxState.configReg, data, size);
    break;
  case SFCX_STATUS_REG:
    memcpy(&sfcxState.statusReg, data, size);
    // Read then write to status register resets its state.
    sfcxState.statusReg = STATUS_PIN_WP_N | STATUS_PIN_BY_N;
    break;
  case SFCX_COMMAND_REG:
    // Set status to busy.
    sfcxState.statusReg |= STATUS_BUSY;

    // Get the command.
    memcpy(&command, data, size);

    // This command has high priority.
    if (command == PAGE_BUF_TO_REG) {
      // If we're reading from Data Buffer to Data reg the Address reg becomes
      // our buffer pointer.
      memcpy(&sfcxState.dataReg, &sfcxState.pageBuffer[sfcxState.addressReg], 4);

#ifdef SFCX_DEBUG
      LOG_DEBUG(SFCX, "PAGE_BUF_TO_REG[{:#x}] = {:#x}", sfcxState.addressReg, sfcxState.dataReg);
#endif // SFCX_DEBUG

      // Increase buffer pointer.
      sfcxState.addressReg += 4;

      command = NO_CMD;
      // Reset status.
      sfcxState.statusReg &= ~STATUS_BUSY;
    }

    // Set command register.
    sfcxState.commandReg = command;
    break;
  case SFCX_ADDRESS_REG:
    memcpy(&sfcxState.addressReg, data, size);
    break;
  case SFCX_DATA_REG:
    memcpy(&sfcxState.dataReg, data, size);
    break;
  case SFCX_LOGICAL_REG:
    memcpy(&sfcxState.logicalReg, data, size);
    break;
  case SFCX_PHYSICAL_REG:
    memcpy(&sfcxState.physicalReg, data, size);
    break;
  case SFCX_DATAPHYADDR_REG:
    memcpy(&sfcxState.dataPhysAddrReg, data, size);
    break;
  case SFCX_SPAREPHYADDR_REG:
    memcpy(&sfcxState.sparePhysAddrReg, data, size);
    break;
  case SFCX_MMC_ID_REG:
    memcpy(&sfcxState.mmcIDReg, data, size);
    break;
  default:
    LOG_ERROR(SFCX, "Write from unknown register {:#x}", reg);
    break;
  }
}
void SFCX::MemSet(u64 writeAddress, s32 data, u64 size) {
  // Set a lock on registers.
  std::lock_guard lck(mutex);

  const u16 reg = writeAddress & 0xFF;

  switch (reg) {
  case SFCX_CONFIG_REG:
    memset(&sfcxState.configReg, data, size);
    break;
  case SFCX_STATUS_REG:
    memset(&sfcxState.statusReg, data, size);
    break;
  case SFCX_COMMAND_REG:
    memset(&sfcxState.commandReg, data, size);
    break;
  case SFCX_ADDRESS_REG:
    memset(&sfcxState.addressReg, data, size);
    break;
  case SFCX_DATA_REG:
    memset(&sfcxState.dataReg, data, size);
    break;
  case SFCX_LOGICAL_REG:
    memset(&sfcxState.logicalReg, data, size);
    break;
  case SFCX_PHYSICAL_REG:
    memset(&sfcxState.physicalReg, data, size);
    break;
  case SFCX_DATAPHYADDR_REG:
    memset(&sfcxState.dataPhysAddrReg, data, size);
    break;
  case SFCX_SPAREPHYADDR_REG:
    memset(&sfcxState.sparePhysAddrReg, data, size);
    break;
  case SFCX_MMC_ID_REG:
    memset(&sfcxState.mmcIDReg, data, size);
    break;
  default:
    LOG_ERROR(SFCX, "Write from unknown register {:#x}", reg);
    break;
  }
}

void SFCX::ConfigWrite(u64 writeAddress, const u8* data, u64 size) {
  const u8 offset = writeAddress & 0xFF;

  // Check if we're being scanned.
  u64 tmp = 0;
  memcpy(&tmp, data, size);
  if (static_cast<u8>(writeAddress) >= 0x10 && static_cast<u8>(writeAddress) < 0x34) {
    const u32 regOffset = (static_cast<u8>(writeAddress) - 0x10) >> 2;
    if (pciDevSizes[regOffset] != 0) {
      if (tmp == 0xFFFFFFFF) { // PCI BAR Size discovery.
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
    if (static_cast<u8>(writeAddress) == 0x30) { // Expansion ROM Base Address.
      tmp = 0; // Register not implemented.
    }
  }

  memcpy(&pciConfigSpace.data[offset], &tmp, size);
}

void SFCX::sfcxMainLoop() {
  // Config register should be initialized by now.
  while (sfcxThreadRunning) {
    // Did we got a command?
    if (sfcxState.commandReg != NO_CMD) {

      // Set a lock on registers.
      std::lock_guard lck(mutex);

      // Check the command reg to see what command was issued.
      switch (sfcxState.commandReg) {
      case PHY_PAGE_TO_BUF: sfcxReadPageFromNAND(true); break;
      case LOG_PAGE_TO_BUF: sfcxReadPageFromNAND(false); break;
      case DMA_PHY_TO_RAM: sfcxDoDMAfromNAND(true); break;
      default:
        LOG_ERROR(SFCX, "Unrecognized command was issued. {:#x}", sfcxState.commandReg);
        break;
      }

      // Clear Command Register.
      sfcxState.commandReg = NO_CMD;

      // Set Status to Ready again.
      sfcxState.statusReg &= ~STATUS_BUSY;
    }
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

void SFCX::sfcxReadPageFromNAND(bool physical) {
  // Calculate NAND offset.
  u32 nandOffset = sfcxState.addressReg;
  nandOffset = 1 ? ((nandOffset / 0x200) * 0x210) + nandOffset % 0x200 : nandOffset;

#ifdef SFCX_DEBUG
  LOG_DEBUG(SFCX, "Reading Page[Physical = {:#d}] Logical address: {:#x}, Physical address: {:#x}",
    physical, sfcxState.addressReg, nandOffset);
#endif // SFCX_DEBUG

  // Clear the page buffer.
  memset(reinterpret_cast<void*>(sfcxState.pageBuffer), 0, sizeof(sfcxState.pageBuffer));

  // Perform the Read.
  nandFile.seekg(nandOffset, std::ios::beg);
  nandFile.read(reinterpret_cast<char*>(sfcxState.pageBuffer), physical ? sfcxState.pageSizePhys : sfcxState.pageSize);

  // Issue Interrupt if interrupts are enabled in flash config.
  if (sfcxState.configReg & CONFIG_INT_EN) {
    parentBus->RouteInterrupt(PRIO_SFCX);
    sfcxState.statusReg |= STATUS_INT_CP;
  }
}

void SFCX::sfcxDoDMAfromNAND(bool physical) {
  // Physical address when doing DMA.
  u32 physAddr = sfcxState.addressReg;
  // Calculate Physical address offset for starting page.
  physAddr = 1 ? ((physAddr / 0x200) * 0x210) + physAddr % 0x200 : physAddr;

  // Number of pages to be transfered when doing DMA.
  u32 dmaPagesNum = ((sfcxState.configReg & CONFIG_DMA_LEN) >> 6) + 1;

  // Get RAM pointers for both buffers.
  u8* dataPhysAddrPtr = mainMemory->getPointerToAddress(sfcxState.dataPhysAddrReg);
  u8* sparePhysAddrPtr = mainMemory->getPointerToAddress(sfcxState.sparePhysAddrReg);

#ifdef SFCX_DEBUG
  LOG_DEBUG(SFCX, "DMA_PHY_TO_RAM: Reading {:#x} pages. Logical Address: {:#x}, Physical Address: {:#x}, Data DMA address: {:#x}, Spare DMA address: {:#x}",
    dmaPagesNum, sfcxState.addressReg, physAddr, sfcxState.dataPhysAddrReg, sfcxState.sparePhysAddrReg);
#endif // SFCX_DEBUG

  // Read Pages to SFCX_DATAPHYADDR_REG and page sapre to SFCX_SPAREPHYADDR_REG.
  for (size_t pageNum = 0; pageNum < dmaPagesNum; pageNum++) {
#ifdef SFCX_DEBUG
    LOG_DEBUG(SFCX, "DMA_PHY_TO_RAM: Reading Page {:#x}. Physical Address: {:#x}", pageNum, physAddr);
#endif // SFCX_DEBUG

    // Clear the page buffer.
    memset(reinterpret_cast<void*>(sfcxState.pageBuffer), 0, sizeof(sfcxState.pageBuffer));

    // Get page data.
    nandFile.seekg(physAddr, std::ios::beg);
    nandFile.read(reinterpret_cast<char*>(sfcxState.pageBuffer), sizeof(sfcxState.pageBuffer));

    // Write page and spare to RAM.
    // On DMA, physical pages are split into Page data and Spare Data, and stored at different locations in memory. 
    memcpy(dataPhysAddrPtr, &sfcxState.pageBuffer, sfcxState.pageSize);
    memcpy(sparePhysAddrPtr, &sfcxState.pageBuffer[sfcxState.pageSize], sfcxState.spareSize);

    // Increase buffer pointers.
    dataPhysAddrPtr += sfcxState.pageSize;    // Logical page size.
    sparePhysAddrPtr += sfcxState.spareSize;  // Spare Size.

    // Increase read address.
    physAddr += sfcxState.pageSizePhys;
  }

  // Issue Interrupt if interrupts are enabled in flash config.
  if (sfcxState.configReg & CONFIG_INT_EN) {
    parentBus->RouteInterrupt(PRIO_SFCX);
    sfcxState.statusReg |= STATUS_INT_CP;
  }
}
