/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Core/HLE/Input/HLEInputSystem.h"

namespace Xe::Core::HLE {

// XInput backend for HLE Input routines
class XInputBackend : public HLEInputSystem {
public:
  XInputBackend(RAM *inRamPtr);
  ~XInputBackend();
  // Backend Setup
  bool Setup() override;
  // XInput HLE functions
  void GetState(sPPEState *ppeState) override;
  void SetState(sPPEState *ppeState) override;
  void GetCapabilities(sPPEState *ppeState) override;
  void GetKeystroke(sPPEState *ppeState) override;

private:
  // RAM Pointer
  RAM *ramPtr = nullptr;
  // Loaded XInput library module handle
  void *moduleHandle = nullptr;
  // Exporter library functions addresses
  void *XInputGetCapabilitiesPtr = nullptr;
  void *XInputGetStatePtr = nullptr;
  void *XInputGetKeystrokePtr = nullptr;
  void *XInputSetStatePtr = nullptr;
  void *XInputEnablePtr = nullptr;

};

} // namespace Xe::Core::HLE