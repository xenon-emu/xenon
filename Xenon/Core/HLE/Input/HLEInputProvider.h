/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <memory>

#include "Core/HLE/HLEFunctionProvider.h"
#include "Core/HLE/Input/HLEInputSystem.h"

namespace Xe::Core::HLE {

// HLE Input provider
class HLEInputProvider : public HLEFunctionProvider {
public:
  explicit HLEInputProvider(std::unique_ptr<HLEInputSystem> backend);
  ~HLEInputProvider() override = default;

  bool Setup() override;
  eHLECategory GetCategory() const override { return eHLECategory::Input; }
  std::string GetName() const override;
  void CollectFunctions(std::vector<sHLEFunction> &table) override;

  // Access the underlying input backend
  HLEInputSystem *GetBackend() { return activeBackend.get(); }

  // Static trampolines for the function table
  static void HLE_XInputGetState(sPPEState *ppeState);
  static void HLE_XInputSetState(sPPEState *ppeState);
  static void HLE_XInputGetCapabilities(sPPEState *ppeState);
  static void HLE_XInputGetKeystroke(sPPEState *ppeState);

private:
  std::unique_ptr<HLEInputSystem> activeBackend;
};

} // namespace Xe::Core::HLE
