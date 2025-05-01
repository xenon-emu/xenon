// Copyright 2025 Xenon Emulator Project

#pragma once

#include <chrono>

#include "Log_types.h"

namespace Base {
namespace Log {

/*
 * A log entry. Log entries are store in a structured format to permit more varied output
 * formatting on different frontends, as well as facilitating filtering and aggregation.
 */
struct Entry {
  std::chrono::microseconds timestamp = {};
  Class logClass = {};
  Level logLevel = {};
  const char *filename = nullptr;
  u32 lineNum = 0;
  std::string function = {};
  std::string message = {};
};

} // namespace Log
} // namespace Base
