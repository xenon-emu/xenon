// Copyright 2025 Xenon Emulator Project

#pragma once

#include <fmt/format.h>

#include "CommitCount.h"

namespace Base {

// Disable version display on Github artifacts because it's broken
const std::string Version = fmt::format("Experimental v0.0.1{}", Base::commit_number != 1 ? fmt::format("-{}", Base::commit_number) : "");

}
