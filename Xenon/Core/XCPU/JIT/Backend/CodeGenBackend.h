/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"
#include "Core/XCPU/PPU/PowerPC.h"
#include "Core/XCPU/JIT/HIR/Block.h"

namespace Xe::XCPU::JIT {

// Native code generation backend
class CodeGenBackend {
public:
  virtual ~CodeGenBackend() = default;

  // One-time initialization (build dispatch tables, etc.).
  virtual bool Initialize() = 0;

  // Emit native code for an entire HIR block.
  // NOTE: The caller owns the pointer and is responsible for releasing it via ReleaseCode().
  virtual bool EmitBlock(HIR::HIRBlock *block, void **outCode, u64 *outCodeSize,
                         ePPUThreadID threadId = ePPUThread_Zero) = 0;

  // Release a previously compiled code pointer.
  virtual void ReleaseCode(void *codePtr) = 0;

  // Reset transient state between block compilations.
  virtual void Reset() = 0;
};

} // namespace Xe::XCPU::JIT