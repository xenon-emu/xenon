/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "HLEFunctionManager.h"

#include "Base/Logging/Log.h"

namespace Xe::Core::HLE {

HLEFunctionManager &HLEFunctionManager::Get() {
  static HLEFunctionManager instance;
  return instance;
}

void HLEFunctionManager::AddProvider(std::unique_ptr<HLEFunctionProvider> provider) {
  LOG_INFO(HLE, "Registered provider '{}' (category {})",
    provider->GetName(), static_cast<u32>(provider->GetCategory()));
  providers.push_back(std::move(provider));
}

HLEFunctionProvider *HLEFunctionManager::GetProvider(eHLECategory category) const {
  for (auto &p : providers) {
    if (p->GetCategory() == category)
      return p.get();
  }
  return nullptr;
}

void HLEFunctionManager::RegisterAllFunctions(std::vector<sHLEFunction> &table) {
  for (auto &provider : providers) {
    provider->CollectFunctions(table);
    LOG_INFO(HLE, "Collected functions from '{}'", provider->GetName());
  }
}

void HLEFunctionManager::Clear() {
  providers.clear();
}

} // namespace Xe::Core::HLE
