// Copyright 2025 Xenon Emulator Project

#pragma once

#include <memory>
#include <sstream>

#include "Base/Types.h"
#include <sirit/sirit.h>

namespace Xe::Microcode::AST {

struct Chunk {
  Sirit::Id id = {};

  Chunk() = default;
  explicit Chunk(Sirit::Id v) : id(v) {}

  operator u32() const { return id.value; }

  operator Sirit::Id() const { return id; }

  Chunk& operator=(Sirit::Id v) {
    id.value = v.value;
    return *this;
  }

  bool IsValid() const {
    return id.value != 0;
  }
};

// Node base
class NodeBase {
public:
  virtual ~NodeBase() = default;
  virtual std::unique_ptr<NodeBase> Clone() const = 0;

  template <typename T>
  std::unique_ptr<T> CloneAs() const {
    return std::unique_ptr<T>(static_cast<T*>(Clone().release()));
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
