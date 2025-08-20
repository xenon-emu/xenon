// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include <memory>
#include <sstream>

#include "Base/Types.h"
#ifndef NO_GFX
#include <sirit/sirit.h>
#endif

namespace Xe::Microcode::AST {

class Shader;

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

enum class eChunkType : u8 {
  Fetch,
  Vector,
  Scalar,
  Boolean,
  Unknown
};

class Chunk {
public:
  eChunkType type = {};
  // Register index
  u32 regIndex = 0;
  // Is it a constant register?
  bool isConst = false;
#ifndef NO_GFX
  // SSA value
  Sirit::Id id = {};
  // Pointer to value
  Sirit::Id ptr = {};

  Chunk() = default;
  explicit Chunk(Sirit::Id v, eChunkType type = eChunkType::Unknown, u32 regIndex = u32(-1), bool isConst = false) : id(v), type(type), regIndex(regIndex), isConst(isConst) {}
  Chunk(Sirit::Id id_, Sirit::Id ptr_, u32 regIndex = u32(-1), eChunkType type = eChunkType::Unknown, bool isConst = false) : id(id_), ptr(ptr_), type(type), regIndex(regIndex), isConst(isConst) {}
  Chunk(const Chunk &c, eChunkType type = eChunkType::Unknown) : regIndex(c.regIndex), id(c.id), ptr(c.ptr), type(type), isConst(c.isConst) {}
#else
  struct Id {
    u32 value;
  };
  // SSA value
  Id id = {};
  // Pointer to value
  Id ptr = {};
  explicit Chunk(Id v, u32 regIndex = u32(-1), eChunkType type = eChunkType::Unknown, bool isConst = false) : id(v), type(type), regIndex(regIndex), isConst(isConst) {}
  Chunk(Id id_, Id ptr_, u32 regIndex = u32(-1), eChunkType type = eChunkType::Unknown, bool isConst = false) : id(id_), ptr(ptr_), type(type), regIndex(regIndex), isConst(isConst) {}
  Chunk(const Chunk &c, eChunkType type = eChunkType::Unknown) : regIndex(c.regIndex), id(c.id), ptr(c.ptr), type(type), isConst(c.isConst) {}
#endif

  operator u32() const { return id.value; }
#ifndef NO_GFX
  operator Sirit::Id() const { return id; }
#else
  operator Id() const { return id; }
#endif

#ifndef NO_GFX
  Chunk &operator=(Sirit::Id v) {
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

} // namespace Xe::Microcode::AST
