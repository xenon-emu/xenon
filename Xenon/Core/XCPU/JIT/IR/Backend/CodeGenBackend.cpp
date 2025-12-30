/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Base/Arch.h"
#include "CodeGenBackend.h"

#if defined(ARCH_X86) || defined(ARCH_X86_64)
#include "X86/X86Backend.h"
#endif

namespace Xe::XCPU::JIT::IR {

 // i think this can maybe made a constexpr !? 

std::unique_ptr<CodeGenBackend> CreateCodeGenBackend() {
  
#if defined(ARCH_X86) || defined(ARCH_X86_64)
  return std::make_unique<X86Backend>();
#endif
  
  // Return nullptr if no supported backend is available
  return nullptr;
}

} // namespace Xe::XCPU::JIT::IR