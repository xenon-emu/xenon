/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "HLEInputProvider.h"

#include "Base/Logging/Log.h"
#include "Core/HLE/HLEFunctionManager.h"

namespace Xe::Core::HLE {

HLEInputProvider::HLEInputProvider(std::unique_ptr<HLEInputSystem> backend)
  : activeBackend(std::move(backend))
{}

bool HLEInputProvider::Setup() {
  if (!activeBackend)
    return false;
  return activeBackend->Setup();
}

std::string HLEInputProvider::GetName() const {
  if (activeBackend)
    return activeBackend->GetName();
  return "Input";
}

void HLEInputProvider::CollectFunctions(std::vector<sHLEFunction> &table) {
  // XAM ordinals for XInput (taken from XAM 2.0.17489 dev)
  // These ordinals resolve to the actual executable export VA via PEImageParser
  constexpr u32 ordXamInputGetState = 401;
  constexpr u32 ordXamInputSetState = 402;
  constexpr u32 ordXamInputGetCapabilities = 400;
  constexpr u32 ordXamInputGetKeystroke = 403;
  constexpr u32 ordXamInputGetKeystrokeEx = 488;

  table.emplace_back(sHLEFunction{ true, ordXamInputGetState, eSystemExecutables::xam, 0x817C3090, "XamInputGetState",
    reinterpret_cast<HLEFunctionPtr>(HLE_XInputGetState) });
  table.emplace_back(sHLEFunction{ true, ordXamInputSetState, eSystemExecutables::xam, 0x817C4FA8, "XamInputSetState",
    reinterpret_cast<HLEFunctionPtr>(HLE_XInputSetState) });
  table.emplace_back(sHLEFunction{ true, ordXamInputGetCapabilities, eSystemExecutables::xam, 0x817C63A0, "XamInputGetCapabilities",
    reinterpret_cast<HLEFunctionPtr>(HLE_XInputGetCapabilities) });
  table.emplace_back(sHLEFunction{ true, ordXamInputGetKeystroke, eSystemExecutables::xam, 0x817C6D90, "XamInputGetKeystroke",
    reinterpret_cast<HLEFunctionPtr>(HLE_XInputGetKeystroke) });
  table.emplace_back(sHLEFunction{ true, ordXamInputGetKeystrokeEx, eSystemExecutables::xam, 0x817C69D0, "XamInputGetKeystrokeEx",
    reinterpret_cast<HLEFunctionPtr>(HLE_XInputGetKeystroke) });

  LOG_INFO(HLE, "[Input]: Registered {} XInput HLE functions.", 5);
}

// Static trampolines

void HLEInputProvider::HLE_XInputGetState(sPPEState *ppeState) {
  auto *provider = HLEFunctionManager::Get().GetProviderAs<HLEInputProvider>(eHLECategory::Input);
  if (!provider || !provider->GetBackend() || !provider->GetBackend()->IsPresent())
    return;
  provider->GetBackend()->GetState(ppeState);
}

void HLEInputProvider::HLE_XInputSetState(sPPEState *ppeState) {
  auto *provider = HLEFunctionManager::Get().GetProviderAs<HLEInputProvider>(eHLECategory::Input);
  if (!provider || !provider->GetBackend() || !provider->GetBackend()->IsPresent())
    return;
  provider->GetBackend()->SetState(ppeState);
}

void HLEInputProvider::HLE_XInputGetCapabilities(sPPEState *ppeState) {
  auto *provider = HLEFunctionManager::Get().GetProviderAs<HLEInputProvider>(eHLECategory::Input);
  if (!provider || !provider->GetBackend() || !provider->GetBackend()->IsPresent())
    return;
  provider->GetBackend()->GetCapabilities(ppeState);
}

void HLEInputProvider::HLE_XInputGetKeystroke(sPPEState *ppeState) {
  auto *provider = HLEFunctionManager::Get().GetProviderAs<HLEInputProvider>(eHLECategory::Input);
  if (!provider || !provider->GetBackend() || !provider->GetBackend()->IsPresent())
    return;
  provider->GetBackend()->GetKeystroke(ppeState);
}

} // namespace Xe::Core::HLE