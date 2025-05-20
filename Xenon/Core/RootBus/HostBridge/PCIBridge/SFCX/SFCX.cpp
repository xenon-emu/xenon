// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "SFCX.h"

#include "Base/Logging/Log.h"
#include "Base/Config.h"
#include "Base/Thread.h"

#include "Core/Xe_Main.h"

//#define SFCX_DEBUG

// There are two SFCX Versions, pre-Jasper and post-Jasper
Xe::PCIDev::SFCX::SFCX(const std::string &deviceName, u64 size, const std::string &nandLoadPath, u32 cpi, PCIBridge *parentPCIBridge, RAM *ram) :
  PCIDevice(deviceName, size),
  cpi(cpi),
  parentBus(parentPCIBridge), mainMemory(ram)
{
  // Set PCI Properties
  pciConfigSpace.configSpaceHeader.reg0.hexData = 0x580B1414;
  pciConfigSpace.configSpaceHeader.reg1.hexData = 0x02000006;
  pciConfigSpace.configSpaceHeader.reg2.hexData = 0x05010001;

  // Set our PCI Dev Sizes
  pciDevSizes[0] = 0x400; // BAR0

  LOG_INFO(SFCX, "Xenon Secure Flash Controller for Xbox.");

  // Set the registers as a dump from my Corona 16MB. These were dumped at POR
  // via Xell before SFCX Init. These are also readable via JRunner and
  // simple360 flasher

  // Xenon Dev Kit ES DD2 64 MB
  // 0x01198030

  // Corona 16 Megs Retail
  // 0x000043000

  // Config Reg is VERY Important. Tells info
  // about Meta/NAND Type
  sfcxState.configReg = 0x000043000;
  sfcxState.statusReg = 0x00000600;
  sfcxState.statusReg = 0x00000600;
  sfcxState.addressReg = 0x00F70030;
  sfcxState.logicalReg = 0x00000100;
  sfcxState.physicalReg = 0x0000100;
  sfcxState.commandReg = NO_CMD;

  // Display Flash config
  LOG_INFO(SFCX, "FlashConfig: 0x{:X}", sfcxState.configReg);

  // Load the NAND dump
  LOG_INFO(SFCX, "Loading NAND from path: {}", nandLoadPath);

  // Check Image size
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

  // Check file magic
  if (!checkMagic()) {
    LOG_CRITICAL(SFCX, "Fatal error! The loaded 'nand.bin' doesn't correspond to a Xbox 360 NAND.");
    SYSTEM_PAUSE();
  }

  // Read NAND Image data
  nandFile.seekg(0, std::ios::beg);
  rawImageData.resize(imageSize);
  for (s32 b = 0; b < imageSize;  b += sfcxState.blockSize) {
    nandFile.read(reinterpret_cast<char*>(rawImageData.data() + b), sfcxState.blockSize);
  }
  // Close the file
  nandFile.close();

  // Get the block size based on the blockSize / pageSize * pageSizePhys
  sfcxState.blockSizePhys = (sfcxState.blockSize / sfcxState.pageSize) * sfcxState.pageSizePhys;

  // Set our device BAR register based on the image size
  pciDevSizes[1] = imageSize; // BAR1

  // Read NAND header.
  memcpy(&sfcxState.nandHeader, reinterpret_cast<char*>(rawImageData.data()),
    sizeof(sfcxState.nandHeader));

  // Display info about the loaded image header
  sfcxState.nandHeader.nandMagic = byteswap_be<u16>(sfcxState.nandHeader.nandMagic);
  LOG_INFO(SFCX, " * NAND Magic: 0x{:X}", sfcxState.nandHeader.nandMagic);

  sfcxState.nandHeader.build = byteswap_be<u16>(sfcxState.nandHeader.build);
  LOG_INFO(SFCX, " * Build: 0x{:X}", sfcxState.nandHeader.build);

  sfcxState.nandHeader.qfe = byteswap_be<u16>(sfcxState.nandHeader.qfe);
  LOG_INFO(SFCX, " * QFE: 0x{:X}", sfcxState.nandHeader.qfe);

  sfcxState.nandHeader.flags = byteswap_be<u16>(sfcxState.nandHeader.flags);
  LOG_INFO(SFCX, " * Flags: 0x{:X}", sfcxState.nandHeader.flags);

  sfcxState.nandHeader.entry = byteswap_be<u32>(sfcxState.nandHeader.entry);
  LOG_INFO(SFCX, " * Entry: 0x{:X}", sfcxState.nandHeader.entry);

  sfcxState.nandHeader.size = byteswap_be<u32>(sfcxState.nandHeader.size);
  LOG_INFO(SFCX, " * Size: 0x{:X}", sfcxState.nandHeader.size);

  sfcxState.nandHeader.keyvaultSize = byteswap_be<u32>(sfcxState.nandHeader.keyvaultSize);
  LOG_INFO(SFCX, " * Keyvault Size: 0x{:X}", sfcxState.nandHeader.keyvaultSize);

  sfcxState.nandHeader.sysUpdateAddr = byteswap_be<u32>(sfcxState.nandHeader.sysUpdateAddr);
  LOG_INFO(SFCX, " * System Update Addr: 0x{:X}", sfcxState.nandHeader.sysUpdateAddr);

  sfcxState.nandHeader.sysUpdateCount = byteswap_be<u16>(sfcxState.nandHeader.sysUpdateCount);
  LOG_INFO(SFCX, " * System Update Count: 0x{:X}", sfcxState.nandHeader.sysUpdateCount);

  sfcxState.nandHeader.keyvaultVer = byteswap_be<u16>(sfcxState.nandHeader.keyvaultVer);
  LOG_INFO(SFCX, " * Keyvault Ver: 0x{:X}", sfcxState.nandHeader.keyvaultVer);

  sfcxState.nandHeader.keyvaultAddr = byteswap_be<u32>(sfcxState.nandHeader.keyvaultAddr);
  LOG_INFO(SFCX, " * Keyvault Addr: 0x{:X}", sfcxState.nandHeader.keyvaultAddr);

  sfcxState.nandHeader.sysUpdateSize = byteswap_be<u32>(sfcxState.nandHeader.sysUpdateSize);
  LOG_INFO(SFCX, " * System Update Size: 0x{:X}", sfcxState.nandHeader.sysUpdateSize);

  sfcxState.nandHeader.smcConfigAddr = byteswap_be<u32>(sfcxState.nandHeader.smcConfigAddr);
  LOG_INFO(SFCX, " * SMC Config Addr: 0x{:X}", sfcxState.nandHeader.smcConfigAddr);

  sfcxState.nandHeader.smcBootSize = byteswap_be<u32>(sfcxState.nandHeader.smcBootSize);
  LOG_INFO(SFCX, " * SMC Boot Size: 0x{:X}", sfcxState.nandHeader.smcBootSize);

  sfcxState.nandHeader.smcBootAddr = byteswap_be<u32>(sfcxState.nandHeader.smcBootAddr);
  LOG_INFO(SFCX, " * SMC Boot Addr: 0x{:X}", sfcxState.nandHeader.smcBootAddr);

  // Get CB_A and CB_B headers
  BL_HEADER cbaHeader;
  BL_HEADER cbbHeader;

  // Get CB_A header data from image data.
  u32 cbaOffset = sfcxState.nandHeader.entry;
  cbaOffset = 1 ? ((cbaOffset / 0x200) * 0x210) + cbaOffset % 0x200 : cbaOffset;
  memcpy(&cbaHeader, reinterpret_cast<char*>(rawImageData.data() + cbaOffset),
    sizeof(cbaHeader));

  // Byteswap CB_A info from header
  cbaHeader.buildNumber = byteswap_be<u16>(cbaHeader.buildNumber);
  cbaHeader.lenght = byteswap_be<u32>(cbaHeader.lenght);
  cbaHeader.entryPoint = byteswap_be<u32>(cbaHeader.entryPoint);

  // Get CB_B header data from image data
  u32 cbbOffset = sfcxState.nandHeader.entry + cbaHeader.lenght;
  cbbOffset = 1 ? ((cbbOffset / 0x200) * 0x210) + cbbOffset % 0x200 : cbbOffset;
  memcpy(&cbbHeader, reinterpret_cast<char*>(rawImageData.data() + cbbOffset),
    sizeof(cbbHeader));

  // Byteswap CB_B info from header
  cbbHeader.buildNumber = byteswap_be<u16>(cbbHeader.buildNumber);
  cbbHeader.lenght = byteswap_be<u32>(cbbHeader.lenght);
  cbbHeader.entryPoint = byteswap_be<u32>(cbbHeader.entryPoint);

  // Check for CB(_A) Magic
  if (cbaHeader.name[0] == 'C' && cbaHeader.name[1] == 'B') {
    LOG_INFO(SFCX, "Found CB(_A) Header: Physical: 0x{:X}, LBA: 0x{:X}", sfcxState.nandHeader.entry, cbaOffset);
    LOG_INFO(SFCX, " * CB Entry: 0x{:X}", cbaHeader.entryPoint);
    LOG_INFO(SFCX, " * CB Lenght: 0x{:X}", cbaHeader.lenght);
  }

  // Check for CB(_B) Magic
  if (cbbHeader.name[0] == 'C' && cbbHeader.name[1] == 'B') {
    LOG_INFO(SFCX, "Found CB(_B) Header: Physical: 0x{:X}, LBA: 0x{:X}", sfcxState.nandHeader.entry + cbaHeader.lenght, cbbOffset);
    LOG_INFO(SFCX, " * CB Entry: 0x{:X}", cbbHeader.entryPoint);
    LOG_INFO(SFCX, " * CB Lenght: 0x{:X}", cbbHeader.lenght);
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

  if (Config::xcpu.overrideInitSkip) {
    initSkip1 = Config::xcpu.HW_INIT_SKIP_1;
    initSkip2 = Config::xcpu.HW_INIT_SKIP_2;
    LOG_INFO(SFCX, "Manual Hardware Init stage skip addresses set:");
    LOG_INFO(SFCX, " > CB({:#d}): Skip Address 1 set to: 0x{:X}", cbVersion, initSkip1);
    LOG_INFO(SFCX, " > CB({:#d}): Skip Address 2 set to: 0x{:X}", cbVersion, initSkip2);
  }
  else {
    LOG_INFO(SFCX, "Auto-detecting Hardware Init stage skip addresses:");
    switch (cbVersion) {
    // CB_B 6723
    case 6723:
      initSkip1 = 0x03009B10;
      LOG_INFO(SFCX, " > CB({:#d}): Skip Address 1 set to: 0x{:X}", cbVersion, initSkip1);
      initSkip2 = 0x03009BA4;
      LOG_INFO(SFCX, " > CB({:#d}): Skip Address 2 set to: 0x{:X}", cbVersion, initSkip2);
      break;
    // CB_B 9188, 15432
    case 9188:
    case 15432:
      initSkip1 = 0x03003DC0;
      LOG_INFO(SFCX, " > CB({:#d}): Skip Address 1 set to: 0x{:X}", cbVersion, initSkip1);
      initSkip2 = 0x03003E54;
      LOG_INFO(SFCX, " > CB({:#d}): Skip Address 2 set to: 0x{:X}", cbVersion, initSkip2);
      break;
    // CB_B 14352
    case 14352:
      initSkip1 = 0x03003F48;
      LOG_INFO(SFCX, " > CB({:#d}): Skip Address 1 set to: 0x{:X}", cbVersion, initSkip1);
      initSkip2 = 0x03003FDC;
      LOG_INFO(SFCX, " > CB({:#d}): Skip Address 2 set to: 0x{:X}", cbVersion, initSkip2);
      break;
    default:
      LOG_ERROR(SFCX, "Auto detection failed. Unimplemented CB found, version {:#d}. Please report to Xenon Devs.", cbVersion);
      break;
    }
  }
  // Enter SFCX Thread
  sfcxThreadRunning = true;
  sfcxThread = std::thread(&Xe::PCIDev::SFCX::sfcxMainLoop, this);
}

Xe::PCIDev::SFCX::~SFCX() {
  // Clear NAND image data
  rawImageData.clear();
  // Terminate thread
  sfcxThreadRunning = false;
  if (sfcxThread.joinable())
    sfcxThread.join();
}

void Xe::PCIDev::SFCX::Read(u64 readAddress, u8* data, u64 size) {
  // Set a lock on registers
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
    LOG_ERROR(SFCX, "Read from unknown register 0x{:X}", reg);
    break;
  }
}

void Xe::PCIDev::SFCX::ConfigRead(u64 readAddress, u8* data, u64 size) {
  const u8 offset = readAddress & 0xFF;
  memcpy(data, &pciConfigSpace.data[offset], size);
}

void Xe::PCIDev::SFCX::Write(u64 writeAddress, const u8* data, u64 size) {
  // Set a lock on registers
  std::lock_guard lck(mutex);

  const u16 reg = writeAddress & 0xFF;

  // Command register
  u32 command = NO_CMD;

  switch (reg) {
  case SFCX_CONFIG_REG:
    memcpy(&sfcxState.configReg, data, size);
    break;
  case SFCX_STATUS_REG:
    memcpy(&sfcxState.statusReg, data, size);
    // Read then write to status register resets its state
    sfcxState.statusReg = STATUS_PIN_WP_N | STATUS_PIN_BY_N;
    break;
  case SFCX_COMMAND_REG:
    // Set status to busy
    sfcxState.statusReg |= STATUS_BUSY;

    // Get the command
    memcpy(&command, data, size);

    // This command has high priority.
    if (command == PAGE_BUF_TO_REG) {
      // If we're reading from Data Buffer to Data reg the Address reg becomes
      // our buffer pointer
      memcpy(&sfcxState.dataReg, &sfcxState.pageBuffer[sfcxState.addressReg], 4);

#ifdef SFCX_DEBUG
      LOG_DEBUG(SFCX, "PAGE_BUF_TO_REG[0x{:X}] = 0x{:X}", sfcxState.addressReg, sfcxState.dataReg);
#endif // SFCX_DEBUG

      // Increase buffer pointer
      sfcxState.addressReg += 4;

      command = NO_CMD;
      // Reset status
      sfcxState.statusReg &= ~STATUS_BUSY;
    }

    // Set command register
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
    LOG_ERROR(SFCX, "Write from unknown register 0x{:X}", reg);
    break;
  }
}
void Xe::PCIDev::SFCX::MemSet(u64 writeAddress, s32 data, u64 size) {
  // Set a lock on registers
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
    LOG_ERROR(SFCX, "Write from unknown register 0x{:X}", reg);
    break;
  }
}

void Xe::PCIDev::SFCX::ReadRaw(u64 readAddress, u8* data, u64 size) {
  u32 offset = static_cast<u32>(readAddress & 0xFFFFFF);
  offset = 1 ? ((offset / 0x200) * 0x210) + offset % 0x200 : offset;
#ifdef NAND_DEBUG
  LOG_DEBUG(SFCX, "Reading RAW data at 0x{:X} (offset 0x{:X}) for 0x{:X} bytes", readAddress, offset, size);
#endif // NAND_DEBUG
  memcpy(data, rawImageData.data() + offset, size);
}

void Xe::PCIDev::SFCX::WriteRaw(u64 writeAddress, const u8* data, u64 size) {
  u32 offset = static_cast<u32>(writeAddress & 0xFFFFFF);
  offset = 1 ? ((offset / 0x200) * 0x210) + offset % 0x200 : offset;
#ifdef NAND_DEBUG
  LOG_DEBUG(SFCX, "Writing RAW data at 0x{:X} (offset 0x{:X}) for 0x{:X} bytes", writeAddress, offset, size);
#endif // NAND_DEBUG
  memcpy(rawImageData.data() + offset, data, size);
}

void Xe::PCIDev::SFCX::MemSetRaw(u64 writeAddress, s32 data, u64 size) {
  u32 offset = static_cast<u32>(writeAddress & 0xFFFFFF);
  offset = 1 ? ((offset / 0x200) * 0x210) + offset % 0x200 : offset;
#ifdef NAND_DEBUG
  LOG_DEBUG(SFCX, "Setting RAW data at 0x{:X} to 0x{:X} (offset 0x{:X}) for 0x{:X} bytes", writeAddress, data, offset, size);
#endif // NAND_DEBUG
  memset(rawImageData.data() + offset, data, size);
}

void Xe::PCIDev::SFCX::ConfigWrite(u64 writeAddress, const u8* data, u64 size) {
  const u8 offset = writeAddress & 0xFF;

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

  memcpy(&pciConfigSpace.data[offset], &tmp, size);
}

void Xe::PCIDev::SFCX::sfcxMainLoop() {
  Base::SetCurrentThreadName("[Xe] SFCX");
  // Config register should be initialized by now
  while (sfcxThreadRunning) {
    // Did we got a command?
    if (sfcxState.commandReg != NO_CMD) {
      // Check the command reg to see what command was issued
      std::lock_guard lck(mutex);
      switch (sfcxState.commandReg) {
      case PHY_PAGE_TO_BUF:
        sfcxReadPageFromNAND(true);
        break;
      case LOG_PAGE_TO_BUF:
        sfcxReadPageFromNAND(false);
        break;
      case DMA_PHY_TO_RAM:
        sfcxDoDMAfromNAND();
        break;
      case DMA_RAM_TO_PHY:
        sfcxDoDMAtoNAND();
        break;
      case BLOCK_ERASE:
        sfcxEraseBlock();
        break;
      default:
        LOG_ERROR(SFCX, "Unrecognized command was issued. 0x{:X}. Issuing interrupt if enabled.", sfcxState.commandReg);
        break;
      }
      if (sfcxState.configReg & CONFIG_INT_EN) {
        parentBus->RouteInterrupt(PRIO_SFCX);
        sfcxState.statusReg |= STATUS_INT_CP;
      }

      // Clear Command Register
      sfcxState.commandReg = NO_CMD;

      // Set Status to Ready again
      sfcxState.statusReg &= ~STATUS_BUSY;
    }
  }
}

bool Xe::PCIDev::SFCX::checkMagic() {
  char magic[2];

  nandFile.read(reinterpret_cast<char*>(magic), sizeof(magic));

  // Retail Nand Magic is 0xFF4F
  // Devkit Nand Magic is 0x0F4F
  // Older Devkit Nand's magic is 0x0F3F

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

void Xe::PCIDev::SFCX::sfcxReadPageFromNAND(bool physical) {
  // Calculate NAND offset
  u32 nandOffset = sfcxState.addressReg;
  nandOffset = 1 ? ((nandOffset / sfcxState.pageSize) * sfcxState.pageSizePhys) + nandOffset % sfcxState.pageSize : nandOffset;

#ifdef SFCX_DEBUG
  LOG_DEBUG(SFCX, "Reading Page[Physical = {:#d}] Logical address: 0x{:X}, Physical address: 0x{:X}",
    physical, sfcxState.addressReg, nandOffset);
#endif // SFCX_DEBUG

  // Clear the page buffer
  memset(sfcxState.pageBuffer, 0, sizeof(sfcxState.pageBuffer));

  if (cpi == 0 && XeMain::GetCPU())
    cpi = XeMain::GetCPU()->GetCPI();
#ifndef SFCX_DEBUG
  // Simulate the time required to read
  std::this_thread::sleep_for(std::chrono::milliseconds(2 * cpi));
#endif

  // Perform the read
  memcpy(sfcxState.pageBuffer, &rawImageData[nandOffset], physical ? sfcxState.pageSizePhys : sfcxState.pageSize);
}

void Xe::PCIDev::SFCX::sfcxEraseBlock() {
  // Block address, or whatever the nand register is pointing to
  u32 nandOffset = sfcxState.addressReg;
  // Calculate offset for the starting page
  nandOffset = 1 ? ((nandOffset / sfcxState.pageSize) * sfcxState.pageSizePhys) + nandOffset % sfcxState.pageSize : nandOffset;

#ifdef SFCX_DEBUG
  LOG_DEBUG(SFCX, "Erasing page at logical address: 0x{:X}, physical address: 0x{:X}", sfcxState.addressReg, nandOffset);
#endif // SFCX_DEBUG

  // Clear the page buffer
  memset(sfcxState.pageBuffer, 0, sizeof(sfcxState.pageBuffer));

  if (cpi == 0 && XeMain::GetCPU())
    cpi = XeMain::GetCPU()->GetCPI();
#ifndef SFCX_DEBUG
  // Simulate the time required to erase
  std::this_thread::sleep_for(std::chrono::milliseconds(2 * cpi));
#endif

  // Perform the erase
  memset(&rawImageData[nandOffset], 0, sfcxState.blockSizePhys);
}

void Xe::PCIDev::SFCX::sfcxDoDMAfromNAND() {
  // Physical address when doing DMA
  u32 physAddr = sfcxState.addressReg;
  // Calculate Physical address offset for starting page
  physAddr = 1 ? ((physAddr / sfcxState.pageSize) * sfcxState.pageSizePhys) + physAddr % sfcxState.pageSize : physAddr;

  // Number of pages to be transfered when doing DMA
  u32 dmaPagesNum = ((sfcxState.configReg & CONFIG_DMA_LEN) >> 6) + 1;

  // Get RAM pointers for both buffers
  u8* dataPhysAddrPtr = mainMemory->getPointerToAddress(sfcxState.dataPhysAddrReg);
  u8* sparePhysAddrPtr = mainMemory->getPointerToAddress(sfcxState.sparePhysAddrReg);

#ifdef SFCX_DEBUG
  LOG_DEBUG(SFCX, "DMA_PHY_TO_RAM: Reading 0x{:X} pages. Logical Address: 0x{:X}, Physical Address: 0x{:X}, Data DMA address: 0x{:X}, Spare DMA address: 0x{:X}",
    dmaPagesNum, sfcxState.addressReg, physAddr, sfcxState.dataPhysAddrReg, sfcxState.sparePhysAddrReg);
#endif // SFCX_DEBUG

  // Read Pages to SFCX_DATAPHYADDR_REG and page sapre to SFCX_SPAREPHYADDR_REG
  for (size_t pageNum = 0; pageNum < dmaPagesNum; pageNum++) {
#ifdef SFCX_DEBUG
    LOG_DEBUG(SFCX, "DMA_PHY_TO_RAM: Reading Page 0x{:X}. Physical Address: 0x{:X}", pageNum, physAddr);
#endif // SFCX_DEBUG

    // Clear the page buffer
    memset(sfcxState.pageBuffer, 0, sizeof(sfcxState.pageBuffer));

    // Get page data
    memcpy(sfcxState.pageBuffer, &rawImageData[physAddr], sfcxState.pageSizePhys);

    // Write page and spare to RAM
    // On DMA, physical pages are split into Page data and Spare Data, and stored at different locations in memory
    memcpy(dataPhysAddrPtr, &sfcxState.pageBuffer, sfcxState.pageSize);
    memcpy(sparePhysAddrPtr, &sfcxState.pageBuffer[sfcxState.pageSize], sfcxState.spareSize);

    // Increase buffer pointers
    dataPhysAddrPtr += sfcxState.pageSize;   // Logical page size
    sparePhysAddrPtr += sfcxState.spareSize; // Spare Size

    if (cpi == 0 && XeMain::GetCPU())
      cpi = XeMain::GetCPU()->GetCPI();

    // Add a small delay to simulate the time it takes to read the page.
    std::this_thread::sleep_for(std::chrono::nanoseconds(100 * cpi));

    // Increase read address
    physAddr += sfcxState.pageSizePhys;
  }
}

void Xe::PCIDev::SFCX::sfcxDoDMAtoNAND() {
  // Physical address when doing DMA
  u32 physAddr = sfcxState.addressReg;
  // Calculate Physical address offset for starting page
  physAddr = 1 ? ((physAddr / sfcxState.pageSize) * sfcxState.pageSizePhys) + physAddr % sfcxState.pageSize : physAddr;

  // Number of pages to be transfered when doing DMA
  u32 dmaPagesNum = ((sfcxState.configReg & CONFIG_DMA_LEN) >> 6) + 1;

  // Get RAM pointers for both buffers
  u8 *dataPhysAddrPtr = mainMemory->getPointerToAddress(sfcxState.dataPhysAddrReg);
  u8 *sparePhysAddrPtr = mainMemory->getPointerToAddress(sfcxState.sparePhysAddrReg);

#ifdef SFCX_DEBUG
  LOG_DEBUG(SFCX, "DMA_RAM_TO_PHY: Writing 0x{:X} pages. Logical Address: 0x{:X}, Physical Address: 0x{:X}, Data DMA address: 0x{:X}, Spare DMA address: 0x{:X}",
    dmaPagesNum, sfcxState.addressReg, physAddr, sfcxState.dataPhysAddrReg, sfcxState.sparePhysAddrReg);
#endif // SFCX_DEBUG

  // Read Pages to SFCX_DATAPHYADDR_REG and page sapre to SFCX_SPAREPHYADDR_REG
  for (size_t pageNum = 0; pageNum < dmaPagesNum; pageNum++) {
#ifdef SFCX_DEBUG
    LOG_DEBUG(SFCX, "DMA_RAM_TO_PHY: Writing page 0x{:X}. Physical Address: 0x{:X}", pageNum, physAddr);
#endif // SFCX_DEBUG

    // Clear the page buffer
    memset(sfcxState.pageBuffer, 0, sizeof(sfcxState.pageBuffer));

    // Write page and spare to NAND
    // On DMA, physical pages are split into Page data and Spare Data, and stored at different locations in memory
    memcpy(&rawImageData[physAddr], dataPhysAddrPtr, sfcxState.pageSize);
    memcpy(&rawImageData[physAddr + sfcxState.pageSize], sparePhysAddrPtr, sfcxState.spareSize);

    // Increase buffer pointers
    dataPhysAddrPtr += sfcxState.pageSize;   // Logical page size
    sparePhysAddrPtr += sfcxState.spareSize; // Spare Size

    if (cpi == 0 && XeMain::GetCPU())
      cpi = XeMain::GetCPU()->GetCPI();
    // Add a small delay to simulate the time it takes to read the page.
    std::this_thread::sleep_for(std::chrono::nanoseconds(100 * cpi));

    // Increase read address
    physAddr += sfcxState.pageSizePhys;
  }
}
