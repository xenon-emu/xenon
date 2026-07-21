/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"
#include "Core/XCPU/PPU/PowerPC.h"

namespace Xe::Core::HLE {

// High-Level Emulated function call
// Any given kernel/XAM function can be HLE'd, we only need to setup a call at a given address (using a branch table)
// and the sPPEState ptr for the caller thread. Parameters are always pre setup by the callee.

// Call flow is as it follows:

// XInput Example
// Game/App calls XAMInputGetState(params in r3-r7)
// XAM calls XInputGetState()
// Kernel routines execute internally, setup state pointers, and set return value (Status) in r3
// Kernel returns using blr
// Game checks state pointers, sees data, and acts upon it

// What we do:
// Setup a redirection (branch) table before exeecution
// The table contains kernel ordinal address -> HLE Function pointer, This can be done automatically after xboxkrnl loads, thus making it work on any kernel.
// When XAM calls XInputGetState(), we intercept that call, re route it to our HLE Handler via an invoke, and execute a blr afterwards.
// Game checks state pointers, sees data, and acts upon it


// Type alias for all HLE Functions
using HLEFunctionPtr = fptr<int(sPPEState *ppeState)>;

// Executables an ordinal may belong to, for HLE functions
enum class eSystemExecutables : u32 {
  invalidExecutable = 0,
  xboxkrnl,
  xam,
};

// Invalid ordinal index
#define INVALID_ORDINAL 0xFFFFFFFF

// High Level Emulated Function
// Represents an HLE function within the system
struct sHLEFunction {
  // Function valid
  bool valid = false;
  // Ordinal for this function
  u32 ordinal = INVALID_ORDINAL;
  // Guest executable this function belongs to
  eSystemExecutables guestExecutable = eSystemExecutables::invalidExecutable;
  // Guest address of this function's entry point
  u64 guestAddress = 0;
  // Name of the HLE function
  std::string functionName = "";
  // Host function pointer
  HLEFunctionPtr functionPtr = nullptr;
};

} //namespace Xe::Core::HLE
