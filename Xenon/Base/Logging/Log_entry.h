// Copyright 2025 Xenon Emulator Project

#pragma once

#include <chrono>

#include "Types.h"

namespace Base::Log {

/*
 * A log entry. Log entries are store in a structured format to permit more varied output
 * formatting on different frontends, as well as facilitating filtering and aggregation.
 */
struct Entry {
  std::chrono::microseconds timestamp;
  Class log_class{};
  Level log_level{};
  const char* filename = nullptr;
  u32 line_num = 0;
  std::string function;
  std::string message;
};

} // namespace Base::Log
