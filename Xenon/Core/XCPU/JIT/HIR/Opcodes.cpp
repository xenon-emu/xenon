/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "Core/XCPU/JIT/HIR/Opcodes.h"

namespace Xe::XCPU::HIR {

#define DEFINE_OPCODE(num, name, sig, flags) \
  const OpcodeInfo num##_info = { flags, sig, name, num, };
#include "Core/XCPU/JIT/HIR/opcodes.inl"
#undef DEFINE_OPCODE

}  // namespace Xe::XCPU::HIR