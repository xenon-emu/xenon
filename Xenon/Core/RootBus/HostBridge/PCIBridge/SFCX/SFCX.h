// Copyright 2025 Xenon Emulator Project

#pragma once

#include <thread>
#include <fstream>
#include <filesystem>

#include "Core/RAM/RAM.h"
#include "Core/RootBus/HostBridge/PCIBridge/PCIBridge.h"
#include "Core/RootBus/HostBridge/PCIBridge/PCIDevice.h"

// Device Size (at address 0xEA00C000)
#define SFCX_DEV_SIZE 0x400

namespace Xe {
namespace PCIDev {

// Device true size is 0xFF, then it just repeats itself.

// Memory Mapped NAND Flash, 1:1. Sofware access this via 0x200 MMU flag wich
// is SOC.
#define NAND_MEMORY_MAPPED_ADDR 0xC8000000
#define NAND_MEMORY_MAPPED_SIZE 0x4000000

// Direct Memory Access Space Size
#define SFCX_DMA_SIZE 0x10000

//
// SFCX Registers
//
#define SFCX_CONFIG_REG 0x00
#define SFCX_STATUS_REG 0x04
#define SFCX_COMMAND_REG 0x08
#define SFCX_ADDRESS_REG 0x0C
#define SFCX_DATA_REG 0x10
#define SFCX_LOGICAL_REG 0x14
#define SFCX_PHYSICAL_REG 0x18
#define SFCX_DATAPHYADDR_REG 0x1C  // DMA Address
#define SFCX_SPAREPHYADDR_REG 0x20 // Spare DMA Address
#define SFCX_MMC_ID_REG 0xFC

//
// Commands for Command Register
//
#define PAGE_BUF_TO_REG 0x00   // Read page buffer to data register
#define REG_TO_PAGE_BUF 0x01   // Write data register to page buffer
#define LOG_PAGE_TO_BUF 0x02   // Read logical page into page buffer
#define PHY_PAGE_TO_BUF 0x03   // Read physical page into page buffer
#define WRITE_PAGE_TO_PHY 0x04 // Write page buffer to physical page
#define BLOCK_ERASE 0x05       // Block Erase
#define DMA_LOG_TO_RAM 0x06    // DMA logical flash to main memory
#define DMA_PHY_TO_RAM 0x07    // DMA physical flash to main memory
#define DMA_RAM_TO_PHY 0x08    // DMA main memory to physical flash
#define UNLOCK_CMD_0 0x55      // Unlock command 0
#define UNLOCK_CMD_1 0xAA      // Unlock command 1
#define NO_CMD 0xFF
//
// Config Register Bitmasks
//
#define CONFIG_DBG_MUX_SEL 0x7C000000 // Debug MUX Select
#define CONFIG_DIS_EXT_ER 0x2000000   // Disable External Error (Pre Jasper?)
#define CONFIG_CSR_DLY 0x1FE0000      // Chip Select to Timing Delay
#define CONFIG_ULT_DLY 0x1F800        // Unlocking Timing Delay
#define CONFIG_BYPASS 0x400           // Debug Bypass
#define CONFIG_DMA_LEN 0x3C0          // DMA Length in Pages
#define CONFIG_FLSH_SIZE 0x30         // Flash Size (Pre Jasper)
#define CONFIG_WP_EN 0x8              // Write Protect Enable
#define CONFIG_INT_EN 0x4             // Interrupt Enable
#define CONFIG_ECC_DIS 0x2            // ECC Decode Disable
#define CONFIG_SW_RST 0x1             // Software reset

#define CONFIG_DMA_PAGES(x) (((x - 1) << 6) & CONFIG_DMA_LEN)

//
// Status Register Bitmasks
//
#define STATUS_MASTER_ABOR 0x4000 // DMA master aborted if not zero
#define STATUS_TARGET_ABOR 0x2000 // DMA target aborted if not zero
#define STATUS_ILL_LOG 0x800      // Illegal Logical Access
#define STATUS_PIN_WP_N 0x400     // NAND Not Write Protected
#define STATUS_PIN_BY_N 0x200     // NAND Not Busy
#define STATUS_INT_CP 0x100       // Interrupt
#define STATUS_ADDR_ER 0x80       // Address Alignment Error
#define STATUS_BB_ER 0x40         // Bad Block Error
#define STATUS_RNP_ER 0x20        // Logical Replacement not found
#define STATUS_ECC_ER 0x1C        // ECC Error, 3 bits, need to determine each
#define STATUS_WR_ER 0x2          // Write or Erase Error
#define STATUS_BUSY 0x1           // Busy
#define STATUS_ECC_ERROR                                                       \
  0x10 // Controller signals unrecoverable ECC error when (!((stat&0x1c) <
       // 0x10))
#define STATUS_DMA_ERROR (STATUS_MASTER_ABOR | STATUS_TARGET_ABOR)
#define STATUS_ERROR			(STATUS_ILL_LOG|STATUS_ADDR_ER|STATUS_BB_ER|STATUS_RNP_ER|STATUS_ECC_ERROR|STATUS_WR_ER|STATUS_MASTER_ABOR|STATUS_TARGET_ABOR
#define STSCHK_WRIERA_ERR(sta) ((sta & STATUS_WR_ER) != 0)
#define STSCHK_ECC_ERR(sta) (!((sta & STATUS_ECC_ER) < 0x10))
#define STSCHK_DMA_ERR(sta)		((sta & (STATUS_DMA_ERROR) != 0)

// Page bitmasks
#define PAGE_VALID (0x4000000)
#define PAGE_PID (0x3FFFE00)

// Meta Types
#define META_TYPE_0 0x00 // Pre Jasper
#define META_TYPE_1 0x01 // Jasper 16MB
#define META_TYPE_2 0x02 // Jasper 256MB and 512MB (Large Block)

// Raw NAND Sizes
#define NAND_16MB_SIZE 0x1080000
#define NAND_64MB_SIZE 0x4200000

//
// NAND Header
//
struct NAND_HEADER {
  u16 nandMagic;
  u16 build;
  u16 qfe;
  u16 flags;
  u32 entry;
  u32 size;
  char msCopyright[64];
  u8 reserved[16];
  u32 keyvaultSize;
  u32 sysUpdateAddr;
  u16 sysUpdateCount;
  u16 keyvaultVer;
  u32 keyvaultAddr;
  u32 sysUpdateSize;
  u32 smcConfigAddr;
  u32 smcBootSize;
  u32 smcBootAddr;
};

// Bootloader Header
struct BL_HEADER {
  u8 name[2];
  u16 buildNumber;
  u16 pairingData;
  u16 flags;
  u32 entryPoint;
  u32 lenght;
};

struct SFCX_STATE {
  // Original SFCX Registers
  u32 configReg;
  u32 statusReg;
  u32 commandReg;
  u32 addressReg;
  u32 dataReg;
  u32 logicalReg;
  u32 physicalReg;
  u32 dataPhysAddrReg;
  u32 sparePhysAddrReg;
  u32 mmcIDReg;

  // Helpers
  u8 metaType = 0;
  u16 pageSize = 0x200;
  u32 blockSize = 0x4000;
  u32 blockSizePhys = 0x4000;
  u8 spareSize = 0x10;
  u16 pageSizePhys = static_cast<u16>(pageSize + spareSize);
  u8 pageBuffer[0x210] = {};
  u16 currentPageBufferPos = 0;
  u8 currentDataReadPos = 0;

  // Loaded Nand Header
  NAND_HEADER nandHeader = {};
};

// Secure Flash Controller for Xbox Device.
class SFCX : public PCIDevice {
public:
  SFCX(const std::string &deviceName, u64 size, const std::string &nandLoadPath,
    PCIBridge *parentPCIBridge, RAM *ram);
  ~SFCX();

  // PCI Read/Write methods to the SFCX device.
  void Read(u64 readAddress, u8* data, u64 size) override;
  void Write(u64 writeAddress, const u8* data, u64 size) override;
  void MemSet(u64 writeAddress, s32 data, u64 size) override;

  // RAW NAND data R/W. Used by Memory-Mapped 1:1 access.
  void ReadRaw(u64 readAddress, u8* data, u64 size);
  void WriteRaw(u64 writeAddress, const u8* data, u64 size);
  void MemSetRaw(u64 writeAddress, s32 data, u64 size);

  // Config space read/write.
  void ConfigRead(u64 readAddress, u8* data, u64 size) override;
  void ConfigWrite(u64 writeAddress, const u8* data, u64 size) override;

private:
  // Secure Flash Controller for Xbox main loop.
  void sfcxMainLoop();
  // Magic check
  bool checkMagic();
  // Thread object
  std::thread sfcxThread;
  // Thread running
  volatile bool sfcxThreadRunning = false;
  // SFCX State
  SFCX_STATE sfcxState{};
  // I/O File stream.
  std::ifstream nandFile;
  // PCI Bridge pointer. Used for Interrupts.
  PCIBridge *parentBus = nullptr;
  // Mutex for thread-safe behavior.
  std::recursive_mutex mutex;
  // RAM pointer. Used for DMA.
  RAM* mainMemory;
  // Read a page from memory to page buffer.
  void sfcxReadPageFromNAND(bool physical);
  // Erase NAND Block
  void sfcxEraseBlock();
  // Does a DMA operation from NAND to physical memory.
  void sfcxDoDMAfromNAND();
  // Does a DMA operation from physical memory to NAND.
  void sfcxDoDMAtoNAND();
  // RAW NAND Data from loaded image.
  std::vector<u8> rawImageData{};
};

} // namespace PCIDev
} // namespace Xe
