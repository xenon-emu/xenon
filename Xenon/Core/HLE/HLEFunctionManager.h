/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <memory>
#include <vector>

#include "Core/HLE/HLEFunction.h"
#include "Core/HLE/HLEFunctionProvider.h"

namespace Xe::Core::HLE {

// Central manager for all HLE function providers.
// Holds providers for each category and processes their functions
// into the CPU's HLE function table in a single pass.
class HLEFunctionManager {
public:
  // Singleton access
  static HLEFunctionManager &Get();

  // Register a provider. Takes ownership.
  // The provider must already have been Setup()'d successfully.
  void AddProvider(std::unique_ptr<HLEFunctionProvider> provider);

  // Get the first provider matching a category, or nullptr
  HLEFunctionProvider *GetProvider(eHLECategory category) const;

  // Get a provider by category, cast to a specific type
  template <typename T>
  T *GetProviderAs(eHLECategory category) const {
    return static_cast<T *>(GetProvider(category));
  }

  // Collect functions from all registered providers and
  // append them into the CPU's HLE function table.
  void RegisterAllFunctions(std::vector<sHLEFunction> &table);

  // Remove all providers (used on shutdown / CPU reset)
  void Clear();

private:
  HLEFunctionManager() = default;
  std::vector<std::unique_ptr<HLEFunctionProvider>> providers;
};

} // namespace Xe::Core::HLE
