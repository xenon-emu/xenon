/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <string>

#include "Base/Hash.h"
#include "Base/LifetimeGuard.h"
#include "Base/Types.h"

class SystemDevice {
public:
  SystemDevice(const char *name, u64 start, u64 end, bool socDevice)
    : name(name), startAddress(start), endAddress(end), socDevice(socDevice)
  {
    hash = Base::JoaatStringHash(name, false);
  }

  virtual void Read(u64 address, u8 *data, u64 byteCount)
  {}
  virtual void Write(u64 address, const u8 *data, u64 byteCount)
  {}
  virtual void MemSet(u64 address, s32 data, u64 byteCount)
  {}

  std::string GetDeviceName() {
    return name;
  }

  constexpr u64 GetHash() {
    return hash;
  }

  u64 GetSize() {
    return startAddress - endAddress;
  }

  bool IsSOCDevice() {
    return socDevice;
  }

  void UpdateEndAddress(u64 address) {
    endAddress = address;
  }

  u64 GetEndAddress() {
    return endAddress;
  }

  void UpdateStartAddress(u64 address) {
    startAddress = address;
  }

  u64 GetStartAddress() {
    return startAddress;
  }

  // Acquire a lease guarding this device against concurrent teardown/
  // replacement (RootBus::ResetDevice). See Base::LifetimeGuard.
  Base::LifetimeGuard::Lease GetLease() {
    return lifetimeGuard.TryAcquire();
  }

  // Teardown-path helpers, used by RootBus::ResetDevice and this device's
  // own destructor/Reset()/Resize() before mutating shared state.
  bool RetireAndWait(u32 timeoutMs = 250) {
    return lifetimeGuard.RetireAndWait(timeoutMs);
  }
  void Reopen() {
    lifetimeGuard.Reopen();
  }
private:
  // Guards Read/Write/MemSet against destruction/replacement/resize racing
  // an in-flight call from another thread.
  Base::LifetimeGuard lifetimeGuard{};

  u64 hash = 0;
  const char *name = "";
  u64 startAddress = 0, endAddress = 0;
  bool socDevice = false;
};