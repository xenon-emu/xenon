/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/RWStorage.h"
#include "Core/KeyVault/NandLayout.h"
#include "Core/PCI/Bridge/PCIBridge.h"
#include "Core/PCI/DmaEngine.h"
#include "Core/PCI/PCIDevice.h"
#include "Core/RAM/RAM.h"

#include <array>
#include <memory>
#include <mutex>
#include <vector>

// Secure Flash Controller for Xbox (SFCX), PCI BAR at 0xEA00C000 (kernel VA 0x7FEAC000).
//
// Commands complete synchronously inside the COMMAND write and latch STATUS.INT_CP, and with CONFIG.INT_EN set that 
// raises the SFC interrupt (IRQL 0x18, bridge source 13). On Corona 4 GB (eMMC) the same BAR is an SDHCI-style host, 
// selected when the image has no spare area (raw eMMC dump). Both follow what the kernel's flash drivers expect.
//
// NAND dumps interleave 16 spare bytes after every 512 data bytes (0x210 per sector), for small block, big-on-small and
// big-block parts alike. eMMC dumps are plain sectors.

namespace Xe::PCIDev {

  namespace Sfcx {
    // Register window size (the 0x100 byte register file repeats over it).
    constexpr u32 DevSize = 0x400;

    // Flash memory-mapped 1:1 (read-only data view, no spare bytes): BAR1, at FlashWindowAddress out of reset. Only the
    // first 64 MiB of the flash are visible through it.
    constexpr u32 FlashWindowAddress = 0xC8000000;
    constexpr u32 FlashWindowSize = 0x4000000;

    // SFCX registers (little-endian, offsets within the 0x100 byte file)
    constexpr u32 RegConfig = 0x00;
    constexpr u32 RegStatus = 0x04;
    constexpr u32 RegCommand = 0x08;
    constexpr u32 RegAddress = 0x0C;
    constexpr u32 RegData = 0x10;
    constexpr u32 RegLogical = 0x14;
    constexpr u32 RegPhysical = 0x18;
    constexpr u32 RegDataPhysAddr = 0x1C;
    constexpr u32 RegMetaPhysAddr = 0x20;
    constexpr u32 RegMmcPresent = 0xFC; // 0 on NAND controllers, the host version on the eMMC host

    // CONFIG
    constexpr u32 ConfigSoftReset = 0x00000001;
    constexpr u32 ConfigIntEnable = 0x00000004;
    constexpr u32 ConfigWriteEnable = 0x00000008; // "WP_EN" in libxenon: set around every program/erase
    constexpr u32 ConfigSizeMask = 0x00000030;    // geometry, read-only
    constexpr u32 ConfigDmaLenMask = 0x000003C0;  // DMA length in native pages - 1
    constexpr u32 ConfigDmaLenShift = 6;
    constexpr u32 ConfigTypeMask = 0x01FE0000; // bits 17-24: controller type + big-block size, read-only

    // STATUS
    constexpr u32 StatusBusy = 0x00000001;
    constexpr u32 StatusWriteError = 0x00000002; // program/erase failed
    constexpr u32 StatusReplacementNotFound = 0x00000020;
    constexpr u32 StatusAddressError = 0x00000080;
    constexpr u32 StatusIntComplete = 0x00000100; // the only bit the kernel's ISR acts on
    constexpr u32 StatusPinNotBusy = 0x00000200;
    constexpr u32 StatusPinNotWriteProtected = 0x00000400;
    constexpr u32 StatusIllegalLogical = 0x00000800;
    constexpr u32 StatusMasterAbort = 0x00004000;
    constexpr u32 StatusLatchedMask = 0x000069FE; // write-1-to-clear bits (errors, aborts, INT_CP, pins)

    // COMMAND
    constexpr u8 CmdPageBufToReg = 0x00;
    constexpr u8 CmdRegToPageBuf = 0x01;
    constexpr u8 CmdLogPageToBuf = 0x02;
    constexpr u8 CmdPhyPageToBuf = 0x03;
    constexpr u8 CmdWritePageToPhy = 0x04;
    constexpr u8 CmdBlockErase = 0x05;
    constexpr u8 CmdDmaLogToRam = 0x06;
    constexpr u8 CmdDmaPhyToRam = 0x07;
    constexpr u8 CmdDmaRamToPhy = 0x08;
    constexpr u8 CmdPartialFirst = 0x10;  // big page program: load the first 512+16 slice
    constexpr u8 CmdPartialNext = 0x11;   // load another slice of the same page
    constexpr u8 CmdPartialCommit = 0x12; // program the assembled page
    constexpr u8 CmdUnlock0 = 0x55;
    constexpr u8 CmdUnlock1 = 0xAA;

    // LOGICAL/PHYSICAL after a logical command (libxenon PAGE_VALID / PAGE_PID)
    constexpr u32 PageValid = 0x04000000;
    constexpr u32 PagePidMask = 0x03FFFE00;

    constexpr u32 SectorSize = 0x200;
    constexpr u32 SpareSize = 0x10;
    constexpr u32 RawSectorSize = SectorSize + SpareSize;
    constexpr u32 MaxSectorsPerPage = 8; // 4 KiB pages
  } // namespace Sfcx

  // Flash organisation as the kernel decodes it from CONFIG.
  struct FLASH_GEOMETRY {
    bool emmc = false;        // raw eMMC image, MMCX host model
    u8 metaType = 0;          // spare layout: 0 small block, 1 big-on-small, 2 big block
    u32 sectorsPerBlock = 32; // 512-byte sectors
    u32 sectorsPerPage = 1;   // native NAND page / 512
    u64 sectorCount = 0;      // sectors in the image
    u32 blockCount = 0;       // blocks in the image
    u32 systemBlocks = 0;     // FlashFs region incl. spare blocks (kernel)
    u32 spareBlocks = 0;      // top spare blocks of the system region
    u32 config = 0;           // SFCX_CONFIG value presented to the guest
  };

  class SFCX : public PCIDevice {
  public:
    SFCX(const std::string &deviceName, u64 size, const std::string &nandLoadPath, PCIBridge *parentPCIBridge,
         RAM *ram);
    ~SFCX();

    void PlatformReset() override;

    // PCI Read/Write methods to the SFCX device.
    void Read(u64 barOffset, u8 *data, u64 size) override;
    void Write(u64 barOffset, const u8 *data, u64 size) override;

    // What the bridge decodes: BAR0 is the register file, BAR1 the flash window.
    void BarRead(u32 bar, u64 offset, u8 *data, u64 size) override;
    void BarWrite(u32 bar, u64 offset, const u8 *data, u64 size) override;

    // Memory-mapped flash window (BAR1). Reads return the data bytes only, writes are ignored (the window is
    // read-only).
    void ReadRaw(u64 offset, u8 *data, u64 size);
    void WriteRaw(u64 offset, const u8 *data, u64 size);

    bool hasInitialised = false;

  private:
    // Flash image: loaded on demand in 64 KiB chunks, every change written through to the file.
    class FlashImage {
    public:
      bool Open(const std::string &path);
      u64 Size() const { return fileSize; }
      // Bytes past the end of the file read as 0xFF (erased).
      void Read(u64 offset, u8 *destination, u64 length);
      // Overwrites (the caller applies NAND semantics). Bytes past the end of the file are dropped.
      bool Write(u64 offset, const u8 *source, u64 length);
      // NAND program: bits can only go from 1 to 0.
      bool Program(u64 offset, const u8 *source, u64 length);
      bool Fill(u64 offset, u8 value, u64 length);

    private:
      static constexpr u64 ChunkSize = 0x10000;
      u8 *Chunk(u64 index);
      bool Modify(u64 offset, u64 length, const u8 *source, int mode);
      std::unique_ptr<Base::ReadWriteStorage> file{};
      u64 fileSize = 0;
      std::vector<std::unique_ptr<u8[]>> chunks{};
      bool writeFailureLogged = false;
    };

    // Geometry
    bool DetectGeometry();
    void LogImageInfo();
    void ReadFlashData(u64 offset, u8 *data, u64 size); // data view (no spare), as the memory window sees it

    // Register access, under mutex. Handlers set raisePending when the interrupt must be (re)asserted, the caller
    // routes it after dropping the lock.
    u32 ReadRegister(u32 reg);
    void WriteRegister(u32 reg, u32 value);

    // NAND controller (SFCX)
    u32 SfcxRead(u32 reg);
    void SfcxWrite(u32 reg, u32 value);
    // Runs an interrupting command and latches its status (INT_CP + error bits).
    void SfcxExecute(u8 command);
    // Reads one 512 + 16 byte sector. 'updateTranslation' sets LOGICAL/PHYSICAL for logical reads.
    u32 SfcxReadSector(u64 sector, bool logical, u8 *raw, bool updateTranslation);
    u32 SfcxProgramSector(u64 sector, const u8 *raw);
    u32 SfcxEraseBlock(u64 sector);
    u32 SfcxDma(bool toFlash, bool logical);
    u32 SfcxPartial(u8 command);
    bool SfcxUnlocked();
    // Logical sector -> physical sector through the bad-block replacement scheme, false sets status.
    bool SfcxTranslate(u64 logicalSector, u64 &physicalSector, u32 &status);
    s64 SfcxTranslateBlock(u32 logicalBlock);
    void SfcxInvalidateRemap(u64 sector);
    bool BlockIsGood(u32 block);
    u32 BlockId(const u8 *spare) const;

    struct SFCX_STATE {
      u32 config = 0;
      u32 status = Sfcx::StatusPinNotBusy | Sfcx::StatusPinNotWriteProtected;
      u32 command = 0;
      u32 address = 0;
      u32 data = 0;
      u32 logical = 0;
      u32 physical = 0;
      u32 dataPhysAddr = 0;
      u32 metaPhysAddr = 0;
      // Page buffer: one 512 + 16 byte sector.
      std::array<u8, Sfcx::RawSectorSize> pageBuffer{};
      // Unlock bytes seen since the last command.
      bool unlock55 = false;
      bool unlockAA = false;
      // Big page program assembly (commands 0x10/0x11/0x12).
      std::array<u8, Sfcx::RawSectorSize * Sfcx::MaxSectorsPerPage> partialPage{};
      u32 partialSlots = 0;
      u64 partialPageSector = 0;
      bool partialActive = false;
    } sfcx{};
    // Logical block -> physical block (-1 not found, -2 not computed yet).
    std::vector<s64> remapCache{};

    // eMMC host (MMCX, SDHCI-style), Corona 4 GB
    u32 MmcRead(u32 reg);
    void MmcWrite(u32 reg, u32 value);
    void MmcCommand(u32 transferModeCommand);
    void MmcSetStatus(u32 bits);
    void MmcSoftwareReset(u32 bits);
    // Scatter/gather length bytes between buffer and guest RAM through the ADMA2 descriptor table.
    bool MmcAdmaTransfer(u8 *buffer, u64 length, bool toHost);
    u32 MmcPresentState() const;
    void MmcBuildExtCsd();

    struct MMC_STATE {
      u32 sdmaAddress = 0;
      u32 blockSizeCount = 0;
      u32 argument = 0;
      u32 transferModeCommand = 0;
      u32 response[4] = {};
      u32 hostControl = 0;
      u32 clockTimeoutReset = 0;
      u32 interruptStatus = 0;
      u32 interruptStatusEnable = 0;
      u32 interruptSignalEnable = 0;
      u32 hostControl2 = 0;
      u32 admaAddressLow = 0;
      u32 admaAddressHigh = 0;
      u32 vendorControl = 0;
      bool sectorAddressing = false;
      u32 blockCountFromCmd23 = 0;
      // PIO data phase
      bool pioActive = false;
      bool pioRead = false;
      u64 pioByteOffset = 0; // flash byte offset of the transfer (writes)
      u32 pioBlockSize = 0;
      u32 pioPosition = 0;
      std::vector<u8> pioBuffer{};
      std::array<u8, 512> extCsd{};
    } mmc{};

    // Raises the SFC interrupt (outside mutex).
    void RaiseInterrupt();

    FLASH_GEOMETRY geometry{};
    FlashImage image{};
    NandLayout::Header nandHeader{};
    PCIBridge *parentBus = nullptr;
    Xe::PCI::DmaEngine dma;
    std::mutex mutex;
    bool raisePending = false;
    bool windowWriteLogged = false;
  };

} // namespace Xe::PCIDev
