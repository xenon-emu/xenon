/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <memory>
#include <functional>

#include "Base/SystemDevice.h"

#define RAM_START_ADDR 0

class RAM : public SystemDevice {
public:
  RAM(const std::string &deviceName, u64 startAddress, std::string size,
    bool isSOCDevice);
  ~RAM();
  void Reset();
  void Resize(u64 size);
  void Read(u64 readAddress, u8 *data, u64 size) override;
  void Write(u64 writeAddress, const u8 *data, u64 size) override;
  void MemSet(u64 writeAddress, s32 data, u64 size) override;

  // Returns a host pointer to a given address belonging to RAM.
  u8 *GetPointerToAddress(u32 address);
  // Returns a pointer to the RAM base in host memory.
  u8 *GetRamBase() { return ramData.get(); }
  // Returns current RAM size.
  u64 GetSize() { return ramSize; }

  // Physical memory write notification callback
  // Called after every write to RAM so that observers (such as the GPU's
  // shared memory) can track which pages have been modified by the CPU.
  // TODO: Expand further for JIT page invalidation inside RAM. 
  // NOTE: Callers must use a page system to track writes or at least do a full Cache Line
  // invalidation by aligning the address to 128 bytes.
  using WriteNotifyCallback = std::function<void(u32 physical_address, u32 length)>;
  void SetWriteNotifyCallback(WriteNotifyCallback callback) {
    writeNotifyCallback_ = std::move(callback);
  }

  // Used by methods like the JIT fast path accessor to notify a write to main memory.
  void NotifyWrite(u32 physicalAddress, u32 byteCount) {
    if (writeNotifyCallback_) { writeNotifyCallback_(physicalAddress, byteCount); }
  }

private:
  u64 ramSize = 0;
  std::unique_ptr<u8[]> ramData{};
  WriteNotifyCallback writeNotifyCallback_;
};
