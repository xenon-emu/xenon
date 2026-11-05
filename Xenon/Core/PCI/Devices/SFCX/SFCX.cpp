/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "SFCX.h"

#include "Base/Config.h"
#include "Base/Global.h"
#include "Base/Logging/Log.h"
#include "Base/Param.h"

#include <algorithm>
#include <bit>
#include <cstring>

// Every register access, command and MMC command of the flash controller.
// #define SFCX_DEBUG

#ifdef SFCX_DEBUG
  #define SFCX_DEBUGP(x, ...) LOG_DEBUG(SFCX, "[SFCX] " x, ##__VA_ARGS__)
#else
  #define SFCX_DEBUGP(x, ...)
#endif

PARAM(sfcx_config,
      "Overrides the SFCX CONFIG value the guest sees (hex, e.g. 0x00023010). Default: derived from the NAND image");

using namespace Xe::PCIDev::Sfcx;

// MMCX interrupt status bits (SDHCI normal/error interrupt status, 0x30)
constexpr u32 MmcIntCommandComplete = 0x00000001;
constexpr u32 MmcIntTransferComplete = 0x00000002;
constexpr u32 MmcIntBufferWriteReady = 0x00000010;
constexpr u32 MmcIntBufferReadReady = 0x00000020;
constexpr u32 MmcIntError = 0x00008000;
constexpr u32 MmcIntDataTimeoutError = 0x00100000;
constexpr u32 MmcIntAdmaError = 0x02000000;

// MMCX registers
constexpr u32 MmcRegSdmaAddress = 0x00;
constexpr u32 MmcRegBlockSizeCount = 0x04;
constexpr u32 MmcRegArgument = 0x08;
constexpr u32 MmcRegTransferModeCommand = 0x0C;
constexpr u32 MmcRegResponse0 = 0x10;
constexpr u32 MmcRegBufferDataPort = 0x20;
constexpr u32 MmcRegPresentState = 0x24;
constexpr u32 MmcRegHostControl = 0x28;
constexpr u32 MmcRegClockTimeoutReset = 0x2C;
constexpr u32 MmcRegInterruptStatus = 0x30;
constexpr u32 MmcRegInterruptStatusEnable = 0x34;
constexpr u32 MmcRegInterruptSignalEnable = 0x38;
constexpr u32 MmcRegHostControl2 = 0x3C;
constexpr u32 MmcRegCapabilities = 0x40;
constexpr u32 MmcRegAdmaAddressLow = 0x58;
constexpr u32 MmcRegAdmaAddressHigh = 0x5C;
constexpr u32 MmcRegVendorControl = 0xF0;
constexpr u32 MmcRegHostVersion = 0xFC;

// VendorControl: bit 5 = sector-addressed part (read when the kernel sets the driver up), bit 21 = ready after a
// controller reset (the kernel polls it).
constexpr u32 MmcVendorSectorAddressing = 0x00000020;
constexpr u32 MmcVendorReady = 0x00200000;

// R1 card status: READY_FOR_DATA, CURRENT_STATE = tran.
constexpr u32 MmcCardStatusTran = 0x00000900;

// Power-on register values of the NAND controller.
constexpr u32 SfcxResetAddress = 0x00F70030;
constexpr u32 SfcxResetLogical = 0x00000100;
constexpr u32 SfcxResetPhysical = 0x00000100;

// Flash image

bool Xe::PCIDev::SFCX::FlashImage::Open(const std::string &path) {
  file = std::make_unique<Base::ReadWriteStorage>(path);
  if (!file->isHandleValid()) {
    file.reset();
    return false;
  }
  fileSize = file->Size();
  chunks.clear();
  chunks.resize((fileSize + ChunkSize - 1) / ChunkSize);
  return fileSize != 0;
}

u8 *Xe::PCIDev::SFCX::FlashImage::Chunk(u64 index) {
  if (index >= chunks.size()) { return nullptr; }
  std::unique_ptr<u8[]> &chunk = chunks[index];
  if (!chunk) {
    chunk = std::make_unique<u8[]>(ChunkSize);
    const u64 base = index * ChunkSize;
    const u64 length = std::min<u64>(ChunkSize, fileSize - base);
    memset(chunk.get() + length, 0xFF, ChunkSize - length);
    if (!file->Read(base, chunk.get(), static_cast<u32>(length))) {
      LOG_ERROR(SFCX, "Failed to read the flash image at {:#x}.", base);
      memset(chunk.get(), 0xFF, length);
    }
  }
  return chunk.get();
}

void Xe::PCIDev::SFCX::FlashImage::Read(u64 offset, u8 *destination, u64 length) {
  while (length != 0) {
    const u64 within = offset % ChunkSize;
    const u64 count = std::min(length, ChunkSize - within);
    if (const u8 *chunk = Chunk(offset / ChunkSize)) {
      memcpy(destination, chunk + within, count);
    } else {
      memset(destination, 0xFF, count);
    }
    offset += count;
    destination += count;
    length -= count;
  }
}

// mode: 0 copy, 1 NAND program (AND), 2 fill with source[0]
bool Xe::PCIDev::SFCX::FlashImage::Modify(u64 offset, u64 length, const u8 *source, int mode) {
  bool ok = true;
  while (length != 0) {
    const u64 within = offset % ChunkSize;
    const u64 count = std::min(length, ChunkSize - within);
    u8 *chunk = Chunk(offset / ChunkSize);
    if (!chunk) {
      // Past the end of the file: the guest sees a larger device than the dump holds.
      if (!writeFailureLogged) {
        LOG_WARNING(SFCX, "Write past the end of the flash image ({:#x}); the data is dropped.", offset);
        writeFailureLogged = true;
      }
      return false;
    }
    u8 *target = chunk + within;
    switch (mode) {
      case 0: memcpy(target, source, count); break;
      case 1:
        for (u64 i = 0; i < count; ++i) { target[i] &= source[i]; }
        break;
      default: memset(target, source[0], count); break;
    }
    // The last chunk may run past the end of the file. Only the part inside it is written back.
    const u64 inFile = offset < fileSize ? std::min<u64>(count, fileSize - offset) : 0;
    if (inFile != 0 && !file->Write(offset, target, static_cast<u32>(inFile))) {
      LOG_ERROR(SFCX, "Failed to write the flash image at {:#x}.", offset);
      ok = false;
    }
    if (mode != 2) { source += count; }
    offset += count;
    length -= count;
  }
  return ok;
}

bool Xe::PCIDev::SFCX::FlashImage::Write(u64 offset, const u8 *source, u64 length) {
  return Modify(offset, length, source, 0);
}

bool Xe::PCIDev::SFCX::FlashImage::Program(u64 offset, const u8 *source, u64 length) {
  return Modify(offset, length, source, 1);
}

bool Xe::PCIDev::SFCX::FlashImage::Fill(u64 offset, u8 value, u64 length) { return Modify(offset, length, &value, 2); }

// Construction

Xe::PCIDev::SFCX::SFCX(const std::string &deviceName, u64 size, const std::string &nandLoadPath,
                       PCIBridge *parentPCIBridge, RAM *ram)
    : PCIDevice(deviceName, size)
    , parentBus(parentPCIBridge)
    , dma(ram) {
  // Set PCI Properties
  pciConfigSpace.configSpaceHeader.id.hexData = 0x580B1414;
  pciConfigSpace.configSpaceHeader.commandStatus.hexData = 0x02000006;
  pciConfigSpace.configSpaceHeader.classRev.hexData = 0x05010001;
  // Register window (BAR0)
  pciDevSizes[0] = DevSize;
  // Flash window (BAR1, 64 MiB). The 1BL reads the flash through it before anything configures the controller, so it
  // decodes at 0xC8000000 out of reset (the kernel programs the same address later).
  pciDevSizes[1] = FlashWindowSize;
  SetBar(1, FlashWindowAddress);

  LOG_INFO(SFCX, "Xenon Secure Flash Controller for Xbox.");
  LOG_INFO(SFCX, "Loading flash image from path: {}", nandLoadPath);

  if (!image.Open(nandLoadPath)) {
    LOG_CRITICAL(SFCX, "Fatal error! Please make sure your NAND (or NAND path) is valid!");
    return;
  }

  if (!DetectGeometry()) { return; }

  // Flash header (data view, big-endian on flash).
  u8 headerBytes[sizeof(NandLayout::Header)];
  ReadFlashData(0, headerBytes, sizeof(headerBytes));
  nandHeader = NandLayout::ReadHeader(headerBytes);
  const u16 magic = nandHeader.nandMagic;
  // Retail 0xFF4F, devkit 0x0F4F, older devkits 0x0F3F.
  if (magic != 0xFF4F && magic != 0x0F4F && magic != 0x0F3F) {
    LOG_CRITICAL(SFCX, "Fatal error! The loaded image doesn't correspond to a Xbox 360 NAND (magic {:#06x}).", magic);
    return;
  }

  if (geometry.emmc) {
    mmc.sectorAddressing = image.Size() > 0x80000000ull;
    MmcBuildExtCsd();
  } else {
    sfcx.config = geometry.config;
    sfcx.address = SfcxResetAddress;
    sfcx.logical = SfcxResetLogical;
    sfcx.physical = SfcxResetPhysical;
    sfcx.pageBuffer.fill(0xFF);
    remapCache.assign(geometry.systemBlocks - geometry.spareBlocks, -2);
  }

  LogImageInfo();
  hasInitialised = true;
}

Xe::PCIDev::SFCX::~SFCX() = default;

// The console's reset line: controller / host registers and the partial-program assembly go back to power-on. The flash
// contents are non-volatile and stay. Commands run inside register writes under the mutex, so none is in flight once
// the lock is held.
void Xe::PCIDev::SFCX::PlatformReset() {
  if (hasInitialised) {
    std::lock_guard lock(mutex);
    raisePending = false;
    windowWriteLogged = false;
    if (geometry.emmc) {
      mmc = {};
      mmc.sectorAddressing = image.Size() > 0x80000000ull;
      MmcBuildExtCsd();
    } else {
      sfcx = {};
      sfcx.config = geometry.config;
      sfcx.address = SfcxResetAddress;
      sfcx.logical = SfcxResetLogical;
      sfcx.physical = SfcxResetPhysical;
      sfcx.pageBuffer.fill(0xFF);
      remapCache.assign(geometry.systemBlocks - geometry.spareBlocks, -2);
    }
  }
  if (parentBus) { parentBus->CancelInterrupt(PRIO_SFCX); }
}

// Works out the part from the image: eMMC dumps have no spare area, NAND dumps interleave 16 spare bytes per 512 data
// bytes. The spare layout (and so the block size) is the one whose block-id field matches the block index on most
// blocks, which the factory and the kernel write into every programmed block.
bool Xe::PCIDev::SFCX::DetectGeometry() {
  const u64 size = image.Size();
  geometry = {};

  if (size % (32 * RawSectorSize) != 0) {
    if (size % SectorSize != 0) {
      LOG_CRITICAL(SFCX, "Flash image size {:#x} is neither a NAND dump (0x210 per sector) nor an eMMC dump.", size);
      return false;
    }
    geometry.emmc = true;
    geometry.sectorCount = size / SectorSize;
    geometry.sectorsPerBlock = 32; // 16 KiB, the MMC FlashFs block
    geometry.blockCount = static_cast<u32>(geometry.sectorCount / 32);
    if (Config::highlyExperimental.consoleRevison != Config::eConsoleRevision::Corona4GB
        && Config::highlyExperimental.consoleRevison != Config::eConsoleRevision::Winchester
        && Config::highlyExperimental.consoleRevison != Config::eConsoleRevision::Corona) {
      LOG_WARNING(SFCX, "eMMC image with a non-Corona console revision: the kernel only uses the MMC host when the "
                        "PCI bridge revision is >= 0x70.");
    }
    return true;
  }

  geometry.sectorCount = size / RawSectorSize;

  struct Candidate {
    u8 metaType;
    u32 sectorsPerBlock;
  };
  static constexpr Candidate candidates[] = {{0, 32}, {1, 32}, {2, 256}, {2, 512}};
  const Candidate *best = nullptr;
  u32 bestMatches = 0;
  for (const Candidate &candidate : candidates) {
    const u64 blocks = geometry.sectorCount / candidate.sectorsPerBlock;
    u32 matches = 0;
    for (u64 block = 1; block < blocks && block <= 512; ++block) {
      u8 spare[SpareSize];
      image.Read(block * candidate.sectorsPerBlock * RawSectorSize + SectorSize, spare, SpareSize);
      if (std::all_of(spare, spare + SpareSize, [](u8 b) { return b == 0xFF; })) { continue; }
      const bool good = candidate.metaType == 2 ? spare[0] == 0xFF : spare[5] == 0xFF;
      if (!good) { continue; }
      const u32 id = candidate.metaType == 0 ? spare[0] | ((spare[1] & 0xF) << 8) : spare[1] | ((spare[2] & 0xF) << 8);
      if (id == block) { ++matches; }
    }
    if (matches > bestMatches) {
      bestMatches = matches;
      best = &candidate;
    }
  }

  u8 metaType = 0;
  u32 sectorsPerBlock = 32;
  if (best && bestMatches >= 2) {
    metaType = best->metaType;
    sectorsPerBlock = best->sectorsPerBlock;
  } else {
    // Blank or unusual image: go by the console revision.
    const auto revision = Config::highlyExperimental.consoleRevison;
    const bool smallBlockConsole = revision == Config::eConsoleRevision::Xenon
                                   || revision == Config::eConsoleRevision::Zephyr
                                   || revision == Config::eConsoleRevision::Falcon;
    if (smallBlockConsole) {
      metaType = 0;
    } else if (geometry.sectorCount * SectorSize >= 0x4000000) {
      metaType = 2;
      sectorsPerBlock = 256;
    } else {
      metaType = 1;
    }
    LOG_WARNING(SFCX, "Couldn't identify the spare layout from the image; assuming metadata type {}.", metaType);
  }

  geometry.metaType = metaType;
  geometry.sectorsPerBlock = sectorsPerBlock;
  geometry.blockCount = static_cast<u32>(geometry.sectorCount / sectorsPerBlock);

  // CONFIG values the kernel decodes back into the same geometry:
  //   bits 17-18 type, bits 4-5 size code, big block: size = 1 << (bits 21-24 + bits 19-20 + 23)
  switch (metaType) {
    case 0: {
      // Small block: 16 KiB blocks, size code 1/2/3 = 1024/2048/4096 blocks, 1/32 of them spares.
      const u32 code = geometry.blockCount <= 1024 ? 1 : geometry.blockCount <= 2048 ? 2 : 3;
      geometry.sectorsPerPage = 1;
      geometry.systemBlocks = 1024u << (code - 1);
      geometry.spareBlocks = 32u << (code - 1);
      geometry.config = 0x01198000 | (code << 4);
    } break;
    case 1:
      // Big-on-small (Jasper/Trinity/Corona 16 MB), 1024 blocks. Jasper parts report type 1 code 1 (0x00023010),
      // Trinity/Corona parts type 2 code 0 (0x00043000). Current kernels decode both the same, but kernels from before
      // Trinity (2.0.9199) bugcheck on type 2 code 0, so only the newer boards get it.
      geometry.sectorsPerPage = 1;
      if (geometry.blockCount <= 1024) {
        geometry.systemBlocks = 1024;
        geometry.spareBlocks = 32;
        const auto revision = Config::highlyExperimental.consoleRevison;
        const bool trinityOrLater =
          revision == Config::eConsoleRevision::Trinity || revision == Config::eConsoleRevision::Corona
          || revision == Config::eConsoleRevision::Corona4GB || revision == Config::eConsoleRevision::Winchester;
        geometry.config = trinityOrLater ? 0x00043000 : 0x00023010;
      } else {
        geometry.systemBlocks = 4096;
        geometry.spareBlocks = 128;
        geometry.config = 0x00043010;
      }
      break;
    default: {
      // Big block: 128 KiB blocks / 2 KiB pages (code 2) or 256 KiB / 4 KiB (code 3). The file system region is the
      // first 512 (256) blocks, the rest is the built-in memory unit.
      const bool hugeBlocks = sectorsPerBlock == 512;
      geometry.sectorsPerPage = hugeBlocks ? 8 : 4;
      geometry.systemBlocks = hugeBlocks ? 256 : 512;
      geometry.spareBlocks = hugeBlocks ? 16 : 32;
      // Device size: the image rounded up to a power of two, at least 64 MiB (the kernel bugchecks below the system
      // region).
      const u64 dataBytes = std::max<u64>(geometry.sectorCount * SectorSize, 0x4000000);
      const u32 log2Size = static_cast<u32>(std::bit_width(dataBytes - 1));
      const u32 sizeHigh = std::min<u32>(log2Size - 24, 15);
      geometry.config = (sizeHigh << 21) | (1u << 19) | (1u << 17) | (hugeBlocks ? 0x3030 : 0x3020);
    } break;
  }

  if (PARAM_sfcx_config.Present()) {
    geometry.config = PARAM_sfcx_config.Get<u32>();
    LOG_WARNING(SFCX, "CONFIG overridden to {:#010x}; the image layout is still treated as detected.", geometry.config);
  }
  return true;
}

void Xe::PCIDev::SFCX::LogImageInfo() {
  if (geometry.emmc) {
    LOG_INFO(SFCX, "eMMC image: {:#x} sectors ({} MiB), {} addressing.", geometry.sectorCount,
             geometry.sectorCount / 2048, mmc.sectorAddressing ? "sector" : "byte");
  } else {
    static constexpr const char *layouts[] = {"small block", "big-on-small", "big block"};
    LOG_INFO(SFCX,
             "NAND image: {} (metadata type {}), {} blocks of {} KiB, {}-byte pages, FlashFs region {} blocks "
             "({} spares).",
             layouts[geometry.metaType], geometry.metaType, geometry.blockCount, geometry.sectorsPerBlock / 2,
             geometry.sectorsPerPage * SectorSize, geometry.systemBlocks, geometry.spareBlocks);
    LOG_INFO(SFCX, "FlashConfig: {:#010x}", geometry.config);
  }

  const NandLayout::Header &header = nandHeader;
  LOG_INFO(SFCX, " * NAND Magic: {:#x}", header.nandMagic);
  LOG_INFO(SFCX, " * Build: {:#x}", header.build);
  LOG_INFO(SFCX, " * QFE: {:#x}", header.qfe);
  LOG_INFO(SFCX, " * Flags: {:#x}", header.flags);
  LOG_INFO(SFCX, " * Entry: {:#x}", header.entry);
  LOG_INFO(SFCX, " * Size: {:#x}", header.size);
  LOG_INFO(SFCX, " * Keyvault Size: {:#x}", header.keyvaultSize);
  LOG_INFO(SFCX, " * System Update Addr: {:#x}", header.sysUpdateAddr);
  LOG_INFO(SFCX, " * System Update Count: {:#x}", header.sysUpdateCount);
  LOG_INFO(SFCX, " * Keyvault Ver: {:#x}", header.keyvaultVer);
  LOG_INFO(SFCX, " * Keyvault Addr: {:#x}", header.keyvaultAddr);
  LOG_INFO(SFCX, " * System Update Size: {:#x}", header.sysUpdateSize);
  LOG_INFO(SFCX, " * SMC Config Addr: {:#x}", header.smcConfigAddr);
  LOG_INFO(SFCX, " * SMC Boot Size: {:#x}", header.smcBootSize);
  LOG_INFO(SFCX, " * SMC Boot Addr: {:#x}", header.smcBootAddr);

  u8 cbBytes[sizeof(NandLayout::BootloaderHeader)] = {};
  const u32 cbaOffset = header.entry;
  ReadFlashData(cbaOffset, cbBytes, sizeof(cbBytes));
  const NandLayout::BootloaderHeader cba = NandLayout::ReadBootloaderHeader(cbBytes);
  const u32 cbbOffset = cbaOffset + cba.length;
  ReadFlashData(cbbOffset, cbBytes, sizeof(cbBytes));
  const NandLayout::BootloaderHeader cbb = NandLayout::ReadBootloaderHeader(cbBytes);

  if (cba.name[0] == 'C' && cba.name[1] == 'B') {
    LOG_INFO(SFCX, "Found CB(_A) Header at {:#x}: entry {:#x}, length {:#x}, build {}.", cbaOffset, cba.entryPoint,
             cba.length, cba.buildNumber);
  }
  if (cbb.name[0] == 'C' && cbb.name[1] == 'B') {
    LOG_INFO(SFCX, "Found CB(_B) Header at {:#x}: entry {:#x}, length {:#x}, build {}.", cbbOffset, cbb.entryPoint,
             cbb.length, cbb.buildNumber);
    // A second CB header right after the first makes it a split CB, whatever the builds (Jasper BB: 6754 + 6754).
    LOG_INFO(SFCX, "Detected Split CB: CB_A {}, CB_B {}.", cba.buildNumber, cbb.buildNumber);
  } else if (cba.name[0] == 'C' && cba.name[1] == 'B') {
    LOG_INFO(SFCX, "Detected Unified CB, version {}.", cba.buildNumber);
  }
}

// Data view of the flash, what the memory-mapped window shows. NAND reads go through the controller's logical mapping
// (bad blocks of the file-system region replaced by their spare blocks), anything outside it is physical.
void Xe::PCIDev::SFCX::ReadFlashData(u64 offset, u8 *data, u64 size) {
  if (geometry.emmc) {
    image.Read(offset, data, size);
    return;
  }
  while (size != 0) {
    const u64 within = offset & (SectorSize - 1);
    const u64 count = std::min<u64>(size, SectorSize - within);
    u64 sector = offset / SectorSize;
    u64 physical = sector;
    u32 status = 0;
    if (!remapCache.empty() && SfcxTranslate(sector, physical, status)) { sector = physical; }
    image.Read(sector * RawSectorSize + within, data, count);
    offset += count;
    data += count;
    size -= count;
  }
}

// Register access

void Xe::PCIDev::SFCX::RaiseInterrupt() {
  if (parentBus) { parentBus->RouteInterrupt(PRIO_SFCX); }
}

void Xe::PCIDev::SFCX::Read(u64 barOffset, u8 *data, u64 size) {
  if (!hasInitialised) {
    memset(data, 0, size);
    return;
  }
  const u32 offset = static_cast<u32>(barOffset & 0xFF);
  bool raise = false;
  {
    std::lock_guard lock(mutex);
    // The register file is 0x100 bytes and repeats over the BAR. Every register is read once per access, which matters
    // for registers with read side effects (the MMC data port).
    u64 done = 0;
    for (u32 reg = offset & ~3u; done < size; reg = (reg + 4) & 0xFF) {
      const u32 value = ReadRegister(reg);
      const u32 lane = done == 0 ? offset & 3 : 0;
      const u64 count = std::min<u64>(size - done, 4 - lane);
      memcpy(data + done, reinterpret_cast<const u8 *>(&value) + lane, count);
      done += count;
    }
    raise = raisePending;
    raisePending = false;
  }
  if (raise) { RaiseInterrupt(); }
}

void Xe::PCIDev::SFCX::Write(u64 barOffset, const u8 *data, u64 size) {
  if (!hasInitialised) { return; }
  const u32 offset = static_cast<u32>(barOffset & 0xFF);
  bool raise = false;
  {
    std::lock_guard lock(mutex);
    if ((offset & 3) != 0 || (size != 4 && size != 8)) {
      // The kernel only does aligned 32-bit accesses. Treat anything else as a write of the touched lanes.
      LOG_WARNING(SFCX, "Unaligned/partial register write at {:#x} (size {})", offset, size);
    }
    u64 done = 0;
    for (u32 reg = offset & ~3u; done < size; reg = (reg + 4) & 0xFF) {
      u32 value = 0;
      const u32 lane = done == 0 ? offset & 3 : 0;
      const u64 count = std::min<u64>(size - done, 4 - lane);
      memcpy(reinterpret_cast<u8 *>(&value) + lane, data + done, count);
      WriteRegister(reg, value);
      done += count;
    }
    raise = raisePending;
    raisePending = false;
  }
  if (raise) { RaiseInterrupt(); }
}

u32 Xe::PCIDev::SFCX::ReadRegister(u32 reg) { return geometry.emmc ? MmcRead(reg) : SfcxRead(reg); }

void Xe::PCIDev::SFCX::WriteRegister(u32 reg, u32 value) {
  if (geometry.emmc) {
    MmcWrite(reg, value);
  } else {
    SfcxWrite(reg, value);
  }
}

// Memory-mapped flash window

void Xe::PCIDev::SFCX::BarRead(u32 bar, u64 offset, u8 *data, u64 size) {
  if (bar == 1) {
    ReadRaw(offset, data, size);
  } else {
    Read(offset, data, size);
  }
}

void Xe::PCIDev::SFCX::BarWrite(u32 bar, u64 offset, const u8 *data, u64 size) {
  if (bar == 1) {
    WriteRaw(offset, data, size);
  } else {
    Write(offset, data, size);
  }
}

void Xe::PCIDev::SFCX::ReadRaw(u64 offset, u8 *data, u64 size) {
  if (!hasInitialised) {
    memset(data, 0xFF, size);
    return;
  }
  std::lock_guard lock(mutex);
  ReadFlashData(offset, data, size);
}

void Xe::PCIDev::SFCX::WriteRaw(u64 offset, const u8 *data, u64 size) {
  std::lock_guard lock(mutex);
  if (!windowWriteLogged) {
    LOG_WARNING(SFCX, "Write to the read-only flash window at {:#x} ignored (flash is written through the controller).",
                offset);
    windowWriteLogged = true;
  }
}

// NAND controller (SFCX)

u32 Xe::PCIDev::SFCX::SfcxRead(u32 reg) {
  switch (reg) {
    case RegConfig: return sfcx.config;
    case RegStatus: return sfcx.status;
    case RegCommand: return sfcx.command;
    case RegAddress: return sfcx.address;
    case RegData: return sfcx.data;
    case RegLogical: return sfcx.logical;
    case RegPhysical: return sfcx.physical;
    case RegDataPhysAddr: return sfcx.dataPhysAddr;
    case RegMetaPhysAddr: return sfcx.metaPhysAddr;
    case RegMmcPresent: return 0; // NAND controller: the kernel keeps the SFCX path
    default: SFCX_DEBUGP("Read from unknown register {:#x}", reg); return 0;
  }
}

void Xe::PCIDev::SFCX::SfcxWrite(u32 reg, u32 value) {
  switch (reg) {
    case RegConfig: {
      // Geometry bits are fixed by the part. The kernel only read-modify-writes the others.
      constexpr u32 readOnly = ConfigSizeMask | ConfigTypeMask;
      u32 config = (value & ~readOnly) | (sfcx.config & readOnly);
      if (config & ConfigSoftReset) {
        // Self-clearing controller reset.
        sfcx.status = StatusPinNotBusy | StatusPinNotWriteProtected;
        sfcx.unlock55 = sfcx.unlockAA = false;
        sfcx.partialActive = false;
        sfcx.pageBuffer.fill(0xFF);
        config &= ~ConfigSoftReset;
      }
      const bool enabled = !(sfcx.config & ConfigIntEnable) && (config & ConfigIntEnable);
      sfcx.config = config;
      // A completion latched while interrupts were off asserts the (level) line now.
      if (enabled && (sfcx.status & StatusIntComplete)) { raisePending = true; }
    } break;
    case RegStatus:
      // Write 1 to clear. The kernel writes back what it read.
      sfcx.status &= ~(value & StatusLatchedMask);
      // Level-sensitive line: still asserted if the guest left INT_CP set.
      if ((sfcx.status & StatusIntComplete) && (sfcx.config & ConfigIntEnable)) { raisePending = true; }
      break;
    case RegCommand: {
      sfcx.command = value;
      const u8 command = static_cast<u8>(value);
      switch (command) {
        case CmdPageBufToReg:
          // DATA <- page buffer[ADDRESS], ADDRESS += 4. Synchronous, no interrupt.
          if (sfcx.address + 4 <= RawSectorSize) {
            memcpy(&sfcx.data, &sfcx.pageBuffer[sfcx.address], 4);
          } else {
            sfcx.data = 0xFFFFFFFF;
          }
          sfcx.address += 4;
          break;
        case CmdRegToPageBuf:
          // page buffer[ADDRESS] <- DATA, ADDRESS += 4. Synchronous, no interrupt.
          if (sfcx.address + 4 <= RawSectorSize) { memcpy(&sfcx.pageBuffer[sfcx.address], &sfcx.data, 4); }
          sfcx.address += 4;
          break;
        case CmdUnlock0: sfcx.unlock55 = true; break;
        case CmdUnlock1: sfcx.unlockAA = true; break;
        default: SfcxExecute(command); break;
      }
    } break;
    case RegAddress: sfcx.address = value; break;
    case RegData: sfcx.data = value; break;
    case RegLogical: sfcx.logical = value; break;
    case RegPhysical: sfcx.physical = value; break;
    case RegDataPhysAddr: sfcx.dataPhysAddr = value; break;
    case RegMetaPhysAddr: sfcx.metaPhysAddr = value; break;
    default: SFCX_DEBUGP("Write to unknown register {:#x} = {:#x}", reg, value); break;
  }
}

// Program/erase need CONFIG write-enable and both unlock bytes since the previous command (the kernel sends 0x55,0xAA
// before programs and 0xAA,0x55 before erases, libxenon does the same).
bool Xe::PCIDev::SFCX::SfcxUnlocked() {
  if ((sfcx.config & ConfigWriteEnable) && sfcx.unlock55 && sfcx.unlockAA) { return true; }
  LOG_WARNING(SFCX, "Program/erase at {:#x} without write enable / unlock sequence (config {:#x}).", sfcx.address,
              sfcx.config);
  return false;
}

void Xe::PCIDev::SFCX::SfcxExecute(u8 command) {
  const u64 sector = sfcx.address / SectorSize;
  u32 result = 0;
  switch (command) {
    case CmdLogPageToBuf:
    case CmdPhyPageToBuf:
      result = SfcxReadSector(sector, command == CmdLogPageToBuf, sfcx.pageBuffer.data(), true);
      break;
    case CmdWritePageToPhy:
      result = SfcxUnlocked() ? SfcxProgramSector(sector, sfcx.pageBuffer.data()) : StatusWriteError;
      break;
    case CmdBlockErase: result = SfcxUnlocked() ? SfcxEraseBlock(sector) : StatusWriteError; break;
    case CmdDmaLogToRam:
    case CmdDmaPhyToRam: result = SfcxDma(false, command == CmdDmaLogToRam); break;
    case CmdDmaRamToPhy: result = SfcxUnlocked() ? SfcxDma(true, false) : StatusWriteError; break;
    case CmdPartialFirst:
    case CmdPartialNext:
    case CmdPartialCommit: result = SfcxPartial(command); break;
    default: LOG_ERROR(SFCX, "Unknown command {:#x} (address {:#x}).", command, sfcx.address); break;
  }
  SFCX_DEBUGP("Command {:#x} address {:#x} -> status bits {:#x}", command, sfcx.address, result);
  sfcx.unlock55 = sfcx.unlockAA = false;
  // Every command but the page-buffer transfers and the unlock bytes completes with INT_CP. The interrupt is level:
  // asserted while INT_CP and INT_EN are both set.
  sfcx.status = (sfcx.status & ~StatusBusy) | result | StatusIntComplete;
  if (sfcx.config & ConfigIntEnable) { raisePending = true; }
}

bool Xe::PCIDev::SFCX::BlockIsGood(u32 block) {
  u8 spare[SpareSize];
  image.Read(static_cast<u64>(block) * geometry.sectorsPerBlock * RawSectorSize + SectorSize, spare, SpareSize);
  // Bad-block marker: byte 0 on big block, byte 5 otherwise. 0xFF = good.
  return (geometry.metaType == 2 ? spare[0] : spare[5]) == 0xFF;
}

u32 Xe::PCIDev::SFCX::BlockId(const u8 *spare) const {
  return geometry.metaType == 0 ? spare[0] | ((spare[1] & 0xF) << 8) : spare[1] | ((spare[2] & 0xF) << 8);
}

// Bad blocks of the file-system region are replaced by spare blocks taken from the top of the region, and a spare
// block's metadata names the logical block it stands for. Same scheme as the kernel's.
s64 Xe::PCIDev::SFCX::SfcxTranslateBlock(u32 logicalBlock) {
  s64 &entry = remapCache[logicalBlock];
  if (entry != -2) { return entry; }
  // Blocks past the end of a short dump read as erased. Nothing to replace there.
  if (logicalBlock >= geometry.blockCount || BlockIsGood(logicalBlock)) { return entry = logicalBlock; }
  for (u32 i = 0; i < geometry.spareBlocks; ++i) {
    const u32 block = geometry.systemBlocks - 1 - i;
    if (block >= geometry.blockCount || !BlockIsGood(block)) { continue; }
    u8 spare[SpareSize];
    image.Read(static_cast<u64>(block) * geometry.sectorsPerBlock * RawSectorSize + SectorSize, spare, SpareSize);
    if (std::all_of(spare, spare + SpareSize, [](u8 b) { return b == 0xFF; })) { continue; }
    if (BlockId(spare) == logicalBlock) { return entry = block; }
  }
  return entry = -1;
}

bool Xe::PCIDev::SFCX::SfcxTranslate(u64 logicalSector, u64 &physicalSector, u32 &status) {
  const u64 logicalBlock = logicalSector / geometry.sectorsPerBlock;
  if (logicalBlock >= remapCache.size()) {
    status |= StatusIllegalLogical;
    return false;
  }
  const s64 block = SfcxTranslateBlock(static_cast<u32>(logicalBlock));
  if (block < 0) {
    status |= StatusReplacementNotFound;
    return false;
  }
  physicalSector = static_cast<u64>(block) * geometry.sectorsPerBlock + logicalSector % geometry.sectorsPerBlock;
  return true;
}

void Xe::PCIDev::SFCX::SfcxInvalidateRemap(u64 sector) {
  // Only the first sector of a block carries the bad-block marker and block id the translation looks at.
  if (sector % geometry.sectorsPerBlock == 0 && sector / geometry.sectorsPerBlock < geometry.systemBlocks) {
    std::fill(remapCache.begin(), remapCache.end(), -2);
  }
}

u32 Xe::PCIDev::SFCX::SfcxReadSector(u64 sector, bool logical, u8 *raw, bool updateTranslation) {
  u32 status = 0;
  u64 physical = sector;
  if (logical) {
    const bool mapped = SfcxTranslate(sector, physical, status);
    if (updateTranslation) {
      sfcx.logical = static_cast<u32>(sector * SectorSize);
      sfcx.physical = mapped ? ((static_cast<u32>(physical * SectorSize) & PagePidMask) | PageValid) : 0;
    }
    if (!mapped) {
      // Unmapped logical page: the kernel's legacy reader returns zeros for it.
      memset(raw, 0xFF, RawSectorSize);
      return status;
    }
  }
  if (physical >= geometry.sectorCount) {
    memset(raw, 0xFF, RawSectorSize);
    return status | StatusAddressError;
  }
  image.Read(physical * RawSectorSize, raw, RawSectorSize);
  // ECC is not checked: dumps carry the controller's ECC and the kernel computes its own on writes.
  return status;
}

u32 Xe::PCIDev::SFCX::SfcxProgramSector(u64 sector, const u8 *raw) {
  if (sector >= geometry.sectorCount) { return StatusAddressError | StatusWriteError; }
  // NAND program: bits only go from 1 to 0 (the kernel always erases first).
  const bool ok = image.Program(sector * RawSectorSize, raw, RawSectorSize);
  SfcxInvalidateRemap(sector);
  return ok ? 0 : StatusWriteError;
}

u32 Xe::PCIDev::SFCX::SfcxEraseBlock(u64 sector) {
  const u64 first = sector - sector % geometry.sectorsPerBlock;
  if (first >= geometry.sectorCount) { return StatusAddressError | StatusWriteError; }
  SFCX_DEBUGP("Erasing block {:#x}", first / geometry.sectorsPerBlock);
  // Erased NAND reads all ones, data and spare (bad marker 0xFF, sequence -1).
  const bool ok = image.Fill(first * RawSectorSize, 0xFF, static_cast<u64>(geometry.sectorsPerBlock) * RawSectorSize);
  SfcxInvalidateRemap(first);
  return ok ? 0 : StatusWriteError;
}

// DMA between flash and RAM: CONFIG.DMA_LEN + 1 native pages (512/2048/4096 bytes), data at DataPhysAddr and 16 spare
// bytes per sector at MetaPhysAddr (0 means no spare transfer, the kernel does that for logical reads).
u32 Xe::PCIDev::SFCX::SfcxDma(bool toFlash, bool logical) {
  const u32 pages = ((sfcx.config & ConfigDmaLenMask) >> ConfigDmaLenShift) + 1;
  const u32 sectors = pages * geometry.sectorsPerPage;
  const u64 first = sfcx.address / SectorSize;
  const u64 dataBytes = static_cast<u64>(sectors) * SectorSize;
  const u64 metaBytes = static_cast<u64>(sectors) * SpareSize;
  const bool withSpare = sfcx.metaPhysAddr != 0;
  if (!dma.Contains(sfcx.dataPhysAddr, dataBytes) || (withSpare && !dma.Contains(sfcx.metaPhysAddr, metaBytes))) {
    LOG_ERROR(SFCX, "DMA outside guest memory (data {:#x}, spare {:#x}, {} sectors).", sfcx.dataPhysAddr,
              sfcx.metaPhysAddr, sectors);
    return StatusMasterAbort;
  }

  u32 status = 0;
  std::array<u8, RawSectorSize> raw{};
  for (u32 i = 0; i < sectors; ++i) {
    // Both ranges were checked above.
    const u32 dataAddress = sfcx.dataPhysAddr + i * SectorSize;
    const u32 spareAddress = sfcx.metaPhysAddr + i * SpareSize;
    if (toFlash) {
      dma.Read(dataAddress, raw.data(), SectorSize);
      if (withSpare) {
        dma.Read(spareAddress, raw.data() + SectorSize, SpareSize);
      } else {
        memset(raw.data() + SectorSize, 0xFF, SpareSize);
      }
      status |= SfcxProgramSector(first + i, raw.data());
    } else {
      status |= SfcxReadSector(first + i, logical, raw.data(), i == 0);
      dma.Write(dataAddress, raw.data(), SectorSize);
      if (withSpare) { dma.Write(spareAddress, raw.data() + SectorSize, SpareSize); }
    }
  }
  return status;
}

// Big page program: 0x10 starts a page with the 512+16 slice in the page buffer at ADDRESS, 0x11 adds a slice, 0x12
// (after the unlock bytes) programs the slices.
u32 Xe::PCIDev::SFCX::SfcxPartial(u8 command) {
  const u32 sectorsPerPage = geometry.sectorsPerPage;
  const u64 sector = sfcx.address / SectorSize;
  const u64 pageSector = sector - sector % sectorsPerPage;

  if (command == CmdPartialFirst) {
    sfcx.partialPage.fill(0xFF);
    sfcx.partialSlots = 0;
    sfcx.partialPageSector = pageSector;
    sfcx.partialActive = true;
  }
  if (command != CmdPartialCommit) {
    if (!sfcx.partialActive || pageSector != sfcx.partialPageSector) {
      LOG_WARNING(SFCX, "Partial program slice at {:#x} outside the open page.", sfcx.address);
      return StatusAddressError;
    }
    const u64 slot = sector - pageSector;
    memcpy(&sfcx.partialPage[slot * RawSectorSize], sfcx.pageBuffer.data(), RawSectorSize);
    sfcx.partialSlots |= 1u << slot;
    return 0;
  }

  if (!sfcx.partialActive) {
    LOG_WARNING(SFCX, "Partial program commit without an open page.");
    return StatusWriteError;
  }
  sfcx.partialActive = false;
  if (!SfcxUnlocked()) { return StatusWriteError; }
  u32 status = 0;
  for (u32 slot = 0; slot < sectorsPerPage; ++slot) {
    if (sfcx.partialSlots & (1u << slot)) {
      status |= SfcxProgramSector(sfcx.partialPageSector + slot, &sfcx.partialPage[slot * RawSectorSize]);
    }
  }
  return status;
}

// eMMC host (MMCX)

void Xe::PCIDev::SFCX::MmcBuildExtCsd() {
  mmc.extCsd.fill(0);
  const u32 sectors = static_cast<u32>(std::min<u64>(geometry.sectorCount, 0xFFFFFFFFull));
  mmc.extCsd[192] = 5;    // EXT_CSD_REV (eMMC 4.41)
  mmc.extCsd[194] = 2;    // CSD_STRUCTURE
  mmc.extCsd[196] = 0x07; // DEVICE_TYPE
  // SEC_COUNT, little-endian: the kernel's sector count (must be >= 0x18000, the 48 MB system area).
  memcpy(&mmc.extCsd[212], &sectors, sizeof(sectors));
  mmc.extCsd[504] = 1; // S_CMD_SET
}

u32 Xe::PCIDev::SFCX::MmcPresentState() const {
  // Card inserted, stable, detect, not write protected, DAT[3:0] and CMD lines high.
  u32 state = 0x01FF0000;
  if (mmc.pioActive) {
    state |= 0x00000002;                            // DAT inhibit
    state |= mmc.pioRead ? 0x00000A00 : 0x00000500; // read/write transfer active + buffer enable
  }
  return state;
}

u32 Xe::PCIDev::SFCX::MmcRead(u32 reg) {
  switch (reg) {
    case MmcRegSdmaAddress: return mmc.sdmaAddress;
    case MmcRegBlockSizeCount: return mmc.blockSizeCount;
    case MmcRegArgument: return mmc.argument;
    case MmcRegTransferModeCommand: return mmc.transferModeCommand;
    case MmcRegResponse0:
    case MmcRegResponse0 + 4:
    case MmcRegResponse0 + 8:
    case MmcRegResponse0 + 12: return mmc.response[(reg - MmcRegResponse0) / 4];
    case MmcRegBufferDataPort: {
      // PIO read (the kernel reads one block per buffer-read-ready). Plain byte order.
      if (!mmc.pioActive || !mmc.pioRead) { return 0xFFFFFFFF; }
      u32 value = 0xFFFFFFFF;
      if (mmc.pioPosition + 4 <= mmc.pioBuffer.size()) { memcpy(&value, &mmc.pioBuffer[mmc.pioPosition], 4); }
      mmc.pioPosition += 4;
      if (mmc.pioPosition % mmc.pioBlockSize == 0) {
        if (mmc.pioPosition >= mmc.pioBuffer.size()) {
          mmc.pioActive = false;
          MmcSetStatus(MmcIntTransferComplete);
        } else {
          MmcSetStatus(MmcIntBufferReadReady);
        }
      }
      return value;
    }
    case MmcRegPresentState: return MmcPresentState();
    case MmcRegHostControl: return mmc.hostControl;
    case MmcRegClockTimeoutReset: return mmc.clockTimeoutReset;
    case MmcRegInterruptStatus: return mmc.interruptStatus;
    case MmcRegInterruptStatusEnable: return mmc.interruptStatusEnable;
    case MmcRegInterruptSignalEnable: return mmc.interruptSignalEnable;
    case MmcRegHostControl2: return mmc.hostControl2;
    case MmcRegCapabilities: return 0x01EC0080; // ADMA2, 8-bit bus, 3.3 V (not read by the kernel)
    case MmcRegAdmaAddressLow: return mmc.admaAddressLow;
    case MmcRegAdmaAddressHigh: return mmc.admaAddressHigh;
    case MmcRegVendorControl:
      return mmc.vendorControl | MmcVendorReady | (mmc.sectorAddressing ? MmcVendorSectorAddressing : 0);
    case MmcRegHostVersion: return 0x00010000; // nonzero: the kernel takes this as an MMC console
    default: return 0;
  }
}

void Xe::PCIDev::SFCX::MmcWrite(u32 reg, u32 value) {
  switch (reg) {
    case MmcRegSdmaAddress: mmc.sdmaAddress = value; break;
    case MmcRegBlockSizeCount: mmc.blockSizeCount = value; break;
    case MmcRegArgument: mmc.argument = value; break;
    case MmcRegTransferModeCommand: MmcCommand(value); break;
    case MmcRegBufferDataPort: {
      // PIO write, one block per buffer-write-ready.
      if (!mmc.pioActive || mmc.pioRead) { break; }
      if (mmc.pioPosition + 4 <= mmc.pioBuffer.size()) { memcpy(&mmc.pioBuffer[mmc.pioPosition], &value, 4); }
      mmc.pioPosition += 4;
      if (mmc.pioPosition % mmc.pioBlockSize == 0) {
        const u32 blockStart = mmc.pioPosition - mmc.pioBlockSize;
        image.Write(mmc.pioByteOffset + blockStart, &mmc.pioBuffer[blockStart], mmc.pioBlockSize);
        if (mmc.pioPosition >= mmc.pioBuffer.size()) {
          mmc.pioActive = false;
          MmcSetStatus(MmcIntTransferComplete);
        } else {
          MmcSetStatus(MmcIntBufferWriteReady);
        }
      }
    } break;
    case MmcRegHostControl: mmc.hostControl = value; break;
    case MmcRegClockTimeoutReset: {
      u32 clock = value & 0x00FFFFFF;
      // Internal clock stable follows internal clock enable.
      clock = (clock & ~0x2u) | ((clock & 0x1u) << 1);
      mmc.clockTimeoutReset = clock;
      // Software reset bits (24 all, 25 CMD line, 26 DAT line) complete immediately and read back as 0.
      if (value & 0x07000000) { MmcSoftwareReset(value & 0x07000000); }
    } break;
    case MmcRegInterruptStatus:
      mmc.interruptStatus &= ~value;
      if ((mmc.interruptStatus & 0xFFFF0000) == 0) { mmc.interruptStatus &= ~MmcIntError; }
      // Level line: re-assert while anything is still pending.
      if (mmc.interruptStatus != 0) { raisePending = true; }
      break;
    case MmcRegInterruptStatusEnable: mmc.interruptStatusEnable = value; break;
    case MmcRegInterruptSignalEnable: mmc.interruptSignalEnable = value; break;
    case MmcRegHostControl2: mmc.hostControl2 = value; break;
    case MmcRegAdmaAddressLow: mmc.admaAddressLow = value; break;
    case MmcRegAdmaAddressHigh: mmc.admaAddressHigh = value; break;
    case MmcRegVendorControl: mmc.vendorControl = value & ~(MmcVendorReady | MmcVendorSectorAddressing); break;
    default: break;
  }
}

// Status bits are latched whatever the enable registers say, the kernel never programs them. The line is asserted when
// the status goes from clear to pending.
void Xe::PCIDev::SFCX::MmcSetStatus(u32 bits) {
  const u32 previous = mmc.interruptStatus;
  mmc.interruptStatus |= bits;
  if (bits & 0xFFFF0000) { mmc.interruptStatus |= MmcIntError; }
  if (previous == 0 && mmc.interruptStatus != 0) { raisePending = true; }
}

void Xe::PCIDev::SFCX::MmcSoftwareReset(u32 bits) {
  if (bits & 0x01000000) {
    // Reset all: command/data state and interrupt status. The card keeps its contents.
    mmc.interruptStatus = 0;
    mmc.response[0] = mmc.response[1] = mmc.response[2] = mmc.response[3] = 0;
    mmc.blockSizeCount = 0;
    mmc.blockCountFromCmd23 = 0;
    mmc.hostControl = 0;
    mmc.clockTimeoutReset = 0;
    mmc.pioActive = false;
  }
  if (bits & 0x04000000) { mmc.pioActive = false; }
}

// ADMA2: 8-byte descriptors {attr | length << 16, address}, little-endian. attr bit 0 is valid, bit 1 end, bits 4-5 the
// action (2 transfer, 3 link).
bool Xe::PCIDev::SFCX::MmcAdmaTransfer(u8 *buffer, u64 length, bool toHost) {
  u32 table = mmc.admaAddressLow;
  u64 done = 0;
  for (u32 guard = 0; guard < 0x1000 && done < length; ++guard) {
    const u8 *descriptor = dma.ReadPointer(table, 8);
    if (!descriptor) { return false; }
    const u32 attributes = Xe::PCI::LoadLe32(descriptor);
    const u32 address = Xe::PCI::LoadLe32(descriptor + 4);
    if (!(attributes & 1)) { return false; }
    const u32 action = (attributes >> 4) & 3;
    if (action == 3) {
      table = address;
      continue;
    }
    if (action == 2) {
      u64 count = attributes >> 16;
      if (count == 0) { count = 0x10000; }
      count = std::min(count, length - done);
      if (!(toHost ? dma.Write(address, buffer + done, count) : dma.Read(address, buffer + done, count))) {
        return false;
      }
      done += count;
    }
    if (attributes & 2) { break; }
    table += 8;
  }
  return done == length;
}

// TRANSFER_MODE | COMMAND << 16: command index bits 24-29, response type 16-17, data present 21. Transfer mode: 0x01
// DMA, 0x02 block count, 0x04 auto CMD12, 0x10 read, 0x20 multi-block. The command completes immediately: CMD_COMPLETE
// plus TRANSFER_COMPLETE (ADMA) or the first buffer-ready (PIO).
void Xe::PCIDev::SFCX::MmcCommand(u32 transferModeCommand) {
  mmc.transferModeCommand = transferModeCommand;
  const u32 index = (transferModeCommand >> 24) & 0x3F;
  const bool dataPresent = transferModeCommand & 0x00200000;
  const bool useDma = transferModeCommand & 0x01;
  const bool multiBlock = transferModeCommand & 0x20;
  const u32 argument = mmc.argument;

  mmc.response[0] = MmcCardStatusTran;
  mmc.response[1] = mmc.response[2] = mmc.response[3] = 0;
  u32 status = MmcIntCommandComplete;

  switch (index) {
    case 0: // GO_IDLE_STATE
      mmc.response[0] = 0;
      mmc.blockCountFromCmd23 = 0;
      break;
    case 1: // SEND_OP_COND: OCR, ready, access mode
      mmc.response[0] = mmc.sectorAddressing ? 0xC0FF8080 : 0x80FF8080;
      break;
    case 2:  // ALL_SEND_CID
    case 9:  // SEND_CSD
    case 10: // SEND_CID
      mmc.response[0] = 0x00000000;
      mmc.response[1] = 0x00000000;
      mmc.response[2] = 0x00000000;
      mmc.response[3] = 0x00150100;
      break;
    case 23: // SET_BLOCK_COUNT
      mmc.blockCountFromCmd23 = argument & 0xFFFF;
      break;
    default: // CMD3/6/7/12/13 ...: R1 / R1b, card in tran state
      break;
  }

  if (dataPresent) {
    const bool isRead =
      index == 8 || index == 17 || index == 18 || (index != 24 && index != 25 && (transferModeCommand & 0x10));
    u32 blockSize = mmc.blockSizeCount & 0xFFF;
    if (blockSize == 0) { blockSize = 512; }
    u32 blockCount = 1;
    if (multiBlock) {
      blockCount = (transferModeCommand & 0x02) ? (mmc.blockSizeCount >> 16) : mmc.blockCountFromCmd23;
      if (blockCount == 0) { blockCount = mmc.blockCountFromCmd23 ? mmc.blockCountFromCmd23 : 1; }
    }
    const u64 length = static_cast<u64>(blockSize) * blockCount;

    std::vector<u8> buffer(length, 0xFF);
    u64 byteOffset = 0;
    bool rangeOk = true;
    if (index == 8) {
      // SEND_EXT_CSD (PIO, 512 bytes)
      memcpy(buffer.data(), mmc.extCsd.data(), std::min<u64>(length, mmc.extCsd.size()));
    } else {
      // Sector-addressed parts take a sector number, byte-addressed ones a byte offset.
      const u64 sector = mmc.sectorAddressing ? argument : argument / SectorSize;
      byteOffset = sector * SectorSize;
      rangeOk = byteOffset + length <= image.Size();
      if (isRead) { image.Read(byteOffset, buffer.data(), length); }
    }
    mmc.blockCountFromCmd23 = 0;

    if (!rangeOk && !isRead) {
      LOG_ERROR(SFCX, "MMC write past the end of the image ({:#x} + {:#x}).", byteOffset, length);
      status |= MmcIntDataTimeoutError;
    } else if (useDma) {
      if (MmcAdmaTransfer(buffer.data(), length, isRead)) {
        if (!isRead) { image.Write(byteOffset, buffer.data(), length); }
        status |= MmcIntTransferComplete;
      } else {
        LOG_ERROR(SFCX, "MMC ADMA error (descriptor table {:#x}).", mmc.admaAddressLow);
        status |= MmcIntAdmaError;
      }
    } else {
      mmc.pioActive = true;
      mmc.pioRead = isRead;
      mmc.pioBlockSize = blockSize;
      mmc.pioPosition = 0;
      mmc.pioByteOffset = byteOffset;
      mmc.pioBuffer = std::move(buffer);
      status |= isRead ? MmcIntBufferReadReady : MmcIntBufferWriteReady;
    }
  }
  SFCX_DEBUGP("MMC CMD{} arg {:#x} mode/cmd {:#x} -> status {:#x}", index, argument, transferModeCommand, status);
  MmcSetStatus(status);
}
