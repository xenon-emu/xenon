// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Base/Types.h"

// IBM's eFuse technology. Used on the Xbox 360 Xenon CPU.
// Contains console-specific information such as CPU Key, CB and lockdown
// values
// * FuseLine 00     Disable CPU J-TAG port
// * FuseLine 01     Used to determine Console type
// * FuseLine 02     Lockdown counter for 2BL/CB
// * FuseLine 03     CPU Key - First 32 bits
// * FuseLine 04     CPU Key - First 32 bits copy
// * FuseLine 05     CPU Key - Last 32 bits
// * FuseLine 06     CPU Key - Last 32 bits copy
// * FuseLine 07-11  System LockDown Value (LDV)

struct eFuses {   // Example FuseSet (Retail):
  u64 fuseLine00; // 0xC0FFFFFFFFFFFFFF
  u64 fuseLine01; // 0x0f0f0f0f0f0f0ff0
  u64 fuseLine02; // 0x0000000000000000	CB LDV set to 0, allowing any CB to run.
  u64 fuseLine03; // 0x8CBA33C6B70BF641
  u64 fuseLine04; // 0x8CBA33C6B70BF641
  u64 fuseLine05; // 0x2AC5A81E6B41BFE6
  u64 fuseLine06; // 0x2AC5A81E6B41BFE6
  u64 fuseLine07; // 0x0000000000000000	LDV Set to 0, effectively allowing any system update
  u64 fuseLine08; // 0x0000000000000000
  u64 fuseLine09; // 0x0000000000000000
  u64 fuseLine10; // 0x0000000000000000
  u64 fuseLine11; // 0x0000000000000000
};
