// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include <memory>
#include <sstream>

#include "Base/Types.h"
#ifndef NO_GFX
#include <sirit/sirit.h>
#endif

namespace Xe::Microcode::AST {

struct Chunk {
#ifndef NO_GFX
  Sirit::Id id = {};   // SSA value (result of OpLoad, etc.)
  Sirit::Id ptr = {};  // Pointer to value (from AddLocalVariable, OpAccessChain, etc.)

  Chunk() = default;
  explicit Chunk(Sirit::Id v) : id(v) {}
  Chunk(Sirit::Id id_, Sirit::Id ptr_) : id(id_), ptr(ptr_) {}
#else
  struct Id {
    u32 value;
  };
  Id id = {};
  Id ptr = {};
  Chunk() = default;
  explicit Chunk(Id v) : id(v) {}
  Chunk(Id id_, Id ptr_) : id(id_), ptr(ptr_) {}
#endif

  operator u32() const { return id.value; }
#ifndef NO_GFX
  operator Sirit::Id() const { return id; }
#else
  operator Id() const { return id; }
#endif

#ifndef NO_GFX
  Chunk& operator=(Sirit::Id v) {
#else
  Chunk &operator=(Id v) {
#endif
    id = v;
    return *this;
  }

  bool IsValid() const {
    return id.value != 0;
  }

  bool HasPointer() const {
    return ptr.value != 0;
  }
};

// Node base
class NodeBase {
public:
  virtual ~NodeBase() = default;
  virtual std::shared_ptr<NodeBase> Clone() const = 0;

  template <typename T>
  std::shared_ptr<T> CloneAs() const {
    return std::dynamic_pointer_cast<T>(Clone());
  }
};

// General expression type
enum class eExprType : u8 {
  ALU,
  VFETCH,
  TFETCH,
  EXPORT
};

// Export register type
enum class eExportReg : u8 {
  POSITION,
  POINTSIZE,
  COLOR0,
  COLOR1,
  COLOR2,
  COLOR3,
  INTERP0,
  INTERP1,
  INTERP2,
  INTERP3,
  INTERP4,
  INTERP5,
  INTERP6,
  INTERP7,
  INTERP8
};

} // namespace Xe::Microcode::AST
