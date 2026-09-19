/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/SystemDevice.h"

#include <functional>
#include <memory>

#define RAM_START_ADDR 0

// Random Access Memory
class RAM : public SystemDevice {
public:
  RAM(const std::string& deviceName, u64 startAddress, std::string size, bool isSOCDevice);
  ~RAM();
  void Reset();
  void Resize(u64 size);
  void Read(u64 readAddress, u8* data, u64 size) override;
  void Write(u64 writeAddress, const u8* data, u64 size) override;
  void MemSet(u64 writeAddress, s32 data, u64 size) override;

  // Returns a host pointer to a given address belonging to RAM.
  u8* GetPointerToAddress(u32 address);
  // Returns a pointer to the RAM base in host memory.
  u8* GetRamBase() { return ramBasePtr; }
  // Returns current RAM size.
  u64 GetSize() { return ramSize; }

  // Physical memory write notification callback
  // Called after every write to RAM so that observers (such as the GPU) can track which pages have been modified by the
  // CPU. NOTE: Callers must use a page system to track writes or at least do a full Cache Line invalidation by aligning
  // the address to 128 bytes.
  using WriteNotifyCallback = std::function<void(u32 physical_address, u32 length)>;
  void SetWriteNotifyCallback(WriteNotifyCallback callback) { writeNotifyCallback = std::move(callback); }

  // Used by accessors to notify a 'direct' write to RAM has ocurred without going through the MMIO interface.
  void NotifyWrite(u32 physicalAddress, u32 byteCount) {
    if (writeNotifyCallback) { writeNotifyCallback(physicalAddress, byteCount); }
  }

private:
  // Current RAM size in bytes.
  u64 ramSize = 0;
  // Current RAM base pointer.
  u8* ramBasePtr = nullptr;
  // Dynamic allocation of RAM data.
  std::unique_ptr<u8[]> ramData{};
  // Write notification Callback for observers to track writes to RAM.
  WriteNotifyCallback writeNotifyCallback;
};
