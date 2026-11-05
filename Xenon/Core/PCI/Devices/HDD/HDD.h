/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/RWStorage.h"
#include "Core/PCI/Bridge/PCIBridge.h"
#include "Core/PCI/DmaEngine.h"
#include "Core/PCI/PrdTable.h"
#include "Core/PCI/SATA.h"
#include "Core/PCI/SataScr.h"
#include "Core/PCI/Worker.h"
#include "Core/RAM/RAM.h"

#include <array>
#include <atomic>
#include <bitset>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Xe::PCIDev {

  // SATA hard drive on the southbridge (PCI dev 2, registers at 0x7FEA1300 under the kernel).
  //
  // The kernel's SATA disk driver gets BAR0 (0x20 bytes) with the ATA shadow task file at 0x00-0x07, Device
  // Control/AltStatus at 0x0A, SStatus/SError/SControl at 0x10/0x14/0x18 and the hotplug register at 0x1C, and BAR1
  // (0x10 bytes) with the bus master DMA block (command 0x00, status 0x02, PRD table 0x04). Reading Status acknowledges
  // every INTRQ. The bus master INTERRUPT bit latches every INTRQ, and ACTIVE clears when the PRD table is used up. The
  // data port moves 16 bits at a time in 512 byte DRQ blocks.
  //
  // COMRESET (SControl DET 1 -> 0) and SRST end with Status 0x50, Error 0x01 and the ATA signature. The hotplug change
  // bits (0x04/0x08) are never latched on this channel, the kernel resets the console when it sees them.

  class HDD : public PCIDevice {
  public:
    // Register window size handed to the PCI device.
    static constexpr u32 DevSize = 0x30;

    HDD(const std::string &deviceName, u64 size, PCIBridge *parentPCIBridge, RAM *ram);
    ~HDD();
    void BarRead(u32 bar, u64 barOffset, u8 *data, u64 size) override;
    void BarWrite(u32 bar, u64 barOffset, const u8 *data, u64 size) override;
    void ConfigRead(u64 readAddress, u8 *data, u64 size) override;
    void ConfigWrite(u64 writeAddress, const u8 *data, u64 size) override;
    void PlatformReset() override;
    // When the drive last moved data to or from the image, in milliseconds on the steady clock. Used by the activity
    // marker in the status strip, 0 until the first transfer.
    u64 LastActivityMs() const { return lastActivityMs.load(std::memory_order_relaxed); }

  private:
    void NoteActivity() {
      lastActivityMs.store(static_cast<u64>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                              std::chrono::steady_clock::now().time_since_epoch())
                                              .count()),
                           std::memory_order_relaxed);
    }
    std::atomic<u64> lastActivityMs{0};

    // What the drive is doing with the current command.
    enum class Phase : u8 {
      Idle,   // No command, or the last one completed.
      PioIn,  // DRQ set, the host reads the current block from the data port.
      PioOut, // DRQ set, the host writes the current block to the data port.
      Dma,    // DMA command accepted, waiting for (or running) the bus master transfer.
      Busy,   // A worker job (cache flush) runs. BSY is set.
    };

    // Where PIO data comes from / goes to.
    enum class PioTarget : u8 {
      Buffer, // Generated data (IDENTIFY, SMART, logs) or the READ/WRITE BUFFER sector.
      Disk,   // Sectors of the mounted image.
    };

    // Work handed to the worker thread.
    enum class JobKind : u8 {
      None,
      Dma,
      Flush
    };

    // Shadow task file. 48-bit commands write each register twice, the *Prev copy holds the first (high order) byte
    // and is what reads return while Device Control HOB is set.
    struct TaskFile {
      u8 features = 0;
      u8 featuresPrev = 0;
      u8 sectorCount = 0;
      u8 sectorCountPrev = 0;
      u8 lbaLow = 0;
      u8 lbaLowPrev = 0;
      u8 lbaMid = 0;
      u8 lbaMidPrev = 0;
      u8 lbaHigh = 0;
      u8 lbaHighPrev = 0;
      u8 device = 0;
      u8 error = 0;
      u8 status = 0;
      u8 deviceControl = 0;
    };

    // Disk transfer of the current command (PIO or DMA).
    struct DiskTransfer {
      u64 lba = 0;     // Next sector to move.
      u32 sectors = 0; // Sectors left.
      bool toHost = false;
      bool forceUnitAccess = false;
    };

    // DMA job parameters, copied out of the locked state for the worker.
    struct DmaJob {
      u64 lba = 0;
      u32 sectors = 0;
      bool toHost = false;
      bool forceUnitAccess = false;
      u32 prdTable = 0;
    };

    // PCI Bridge pointer. Used for Interrupts.
    PCIBridge *parentBus = nullptr;
    // Bus-master access to guest RAM for DMA ops.
    Xe::PCI::DmaEngine dma;

    // Guards everything.
    std::mutex ataMutex;
    std::condition_variable workCV;
    Xe::PCI::Worker hddWorkerThread;
    bool workerStop = false;
    // A job is running outside the lock (PlatformReset waits for it before guest memory is cleared).
    bool workerActive = false;
    std::condition_variable idleCV;

    // Mounted image.
    std::unique_ptr<Base::ReadWriteStorage> image{};
    bool imageAttached = false;
    u64 imageSizeInBytes = 0;
    u64 sectorCount = 0;
    // Image written since the last cache flush.
    bool imageDirty = false;

    // IDENTIFY DEVICE words. SET FEATURES and friends keep them current.
    std::array<u16, 256> identify{};

    // Registers.
    TaskFile tf{};
    Xe::PCI::SataScr scr;
    u8 dmaCommand = 0;
    u8 dmaStatus = 0;
    u32 dmaPrdTable = 0;

    // INTRQ pending (cleared by a Status read or a command write).
    bool intrqPending = false;

    // Current command.
    Phase phase = Phase::Idle;
    u8 command = 0;
    PioTarget pioTarget = PioTarget::Buffer;
    DiskTransfer xfer{};
    // The DMA transfer of the current command was handed to the worker.
    bool dmaQueued = false;
    // Sectors per DRQ block for disk PIO (1, or the multiple count for READ/WRITE MULTIPLE).
    u32 drqSectors = 1;
    // PIO data of the current DRQ block and the position in it.
    std::vector<u8> pioBlock;
    u32 pioPos = 0;
    // Generated data still to come after the current block (Buffer target).
    std::vector<u8> pioPending;
    // 512 byte sector for READ BUFFER / WRITE BUFFER.
    std::array<u8, 512> sectorBuffer{};

    // Worker job. Resets and aborted commands bump generation, so a job that finishes after its command was abandoned
    // leaves the new state alone.
    JobKind job = JobKind::None;
    DmaJob dmaJob{};
    u64 generation = 0;

    // Drive settings changed by commands.
    u8 multipleCount = 0;
    bool smartEnabled = true;
    bool standby = false;

    // Power-on time for the SMART power-on hours attribute.
    std::chrono::steady_clock::time_point powerOnTime = std::chrono::steady_clock::now();

    // Unsupported commands we already warned about.
    std::bitset<256> warnedCommands{};

    // Worker thread.
    void WorkerLoop();
    void RunDmaJob(const DmaJob &params, u64 jobGeneration);
    void RunFlushJob(u64 jobGeneration, bool flushNeeded);

    // Register access.
    u8 ReadTaskFileRegister(u32 reg);
    void WriteTaskFileRegister(u32 reg, u8 value);
    void ReadDataPort(u8 *data, u64 size);
    void WriteDataPort(const u8 *data, u64 size);
    void WriteDeviceControl(u8 value);
    void WriteDmaCommand(u8 value);
    // A write to the SCR block (MMIO 0x10, config 0xC0), offset 0..15 from SStatus, and what it did to the link.
    void WriteScr(u32 offset, const u8 *data, u64 size);

    // Command execution.
    void ExecuteCommand(u8 cmd);
    void ExecuteSetFeatures();
    void ExecuteSmart();
    void ExecuteReadLog(bool extended);
    void BeginDiskPio(bool toHost, bool lba48, bool multiple, bool forceUnitAccess = false);
    void BeginDma(bool toHost, bool lba48, bool forceUnitAccess);
    void BeginPioIn(std::vector<u8> data);
    void BeginPioOut(u32 bytes);
    bool LoadNextPioInBlock();
    void FinishPioOutBlock();
    void StartDmaIfReady();
    void QueueFlush();

    // Completion and reset helpers.
    void CompleteOk();
    void CompleteError(u8 error);
    void Abort();
    void RaiseInterrupt();
    void AcknowledgeInterrupt();
    void SetSignature();
    void DeviceReset();
    void AbandonCommand();

    // Addressing helpers.
    u64 Lba28() const;
    u64 Lba48() const;
    u32 Count28() const;
    u32 Count48() const;
    bool RangeValid(u64 lba, u64 count, bool lba48) const;

    // IDENTIFY and SMART data.
    void BuildIdentify();
    void UpdateIdentifyChecksum();
    std::vector<u8> IdentifyBytes() const;
    std::vector<u8> SmartData() const;
    std::vector<u8> SmartThresholds() const;

    static const char *CommandName(u8 cmd);
  };

} // namespace Xe::PCIDev
