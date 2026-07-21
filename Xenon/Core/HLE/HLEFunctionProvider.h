/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <string>
#include <vector>

#include "Base/Types.h"
#include "Core/HLE/HLEFunction.h"

namespace Xe::Core::HLE {

// Category of HLE function providers
enum class eHLECategory : u32 {
  Input,
  Video,
  Audio,
};

// Base class for all HLE function providers.
// Each provider owns the logic for a specific subsystem (Input, Video, Audio, etc.)
// and is responsible for populating its own HLE function entries.
class HLEFunctionProvider {
public:
  virtual ~HLEFunctionProvider() = default;

  // Setup the provider, returns true if the underlying system is available
  virtual bool Setup() = 0;

  // Category this provider belongs to
  virtual eHLECategory GetCategory() const = 0;

  // Provider name for logging
  virtual std::string GetName() const = 0;

  // Populate the given vector with HLE function entries this provider handles.
  // Called by HLEFunctionManager during registration.
  virtual void CollectFunctions(std::vector<sHLEFunction> &table) = 0;
};

} // namespace Xe::Core::HLE
