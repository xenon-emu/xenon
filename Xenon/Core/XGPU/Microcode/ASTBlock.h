// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Base/Types.h"

namespace Xe::Microcode {

enum class eBlockType : u8 {
  EXEC,
  JUMP,
  CALL,
  END, // Shader end
  RET  // Function return
};

// Generalized block in the shader
class Block {
public:

private:

};

} // namespace Xe::Microcode
