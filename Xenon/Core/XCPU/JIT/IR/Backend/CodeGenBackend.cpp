/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Base/Arch.h"
#include "CodeGenBackend.h"

namespace Xe::XCPU::JIT::IR {

std::unique_ptr<CodeGenBackend> CreateCodeGenBackend() {
  // Return nullptr if no supported backend is available
  return nullptr;
}

} // namespace Xe::XCPU::JIT::IR