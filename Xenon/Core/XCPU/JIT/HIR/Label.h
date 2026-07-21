/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#pragma once

#include "Base/Types.h"

namespace Xe::XCPU::HIR {

class Block;

class Label {
public:
  Block *block;
  Label *next;
  Label *prev;

  u32 id;
  char *name;

  void *tag;
};

}  // namespace Xe::XCPU::HIR