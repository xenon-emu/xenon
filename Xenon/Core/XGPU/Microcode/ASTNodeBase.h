// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Base/Types.h"

namespace Xe::Microcode::AST {
  
// Node base
class NodeBase {
protected:
  virtual ~NodeBase() = default;
public:
  virtual NodeBase* Copy() = 0;
  virtual void Release() = 0;
  virtual std::string ToString() const = 0;

  template <typename T>
  T* CopyWithType() {
    return static_cast<T*>(Copy());
  }
};

} // namespace Xe::Microcode