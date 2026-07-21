/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"
#include "Core/XCPU/PPU/PowerPC.h"

namespace Xe::Core::HLE {

// High-level emulated input system base class
// Provides mappings to the needed system functions for XInput
class HLEInputSystem {
public:
  // Destructor
  virtual ~HLEInputSystem() {};

  // Returns true if the input system is present
  bool IsPresent() { return isPresent; }
  // Sets wheter the input system is present
  void SetPresent(bool present) { isPresent = present; }

  // Input system name getters/setters
  std::string GetName() { return inputSystemName; }
  void SetName(std::string name) { inputSystemName = name; }

  // Setup Input system
  virtual bool Setup() { return true; }

  // XInput Methods

  // GetState -> XInputGetState
  virtual void GetState(sPPEState *ppeState) {};
  // SetState -> XInputSetState
  virtual void SetState(sPPEState *ppeState) {};
  // GetCapabilities -> XInputGetCapabilities
  virtual void GetCapabilities(sPPEState *ppeState) {};
  // GetKeystroke -> XInputGetKeystroke
  virtual void GetKeystroke(sPPEState *ppeState) {};

private:
  // Returns true if this input system is present
  bool isPresent = false;
  // This input system name
  std::string inputSystemName = "";
};

} // namespace Xe::Core::HLE
