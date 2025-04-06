// Copyright 2025 Xenon Emulator Project

#pragma once

#include <fmt/format.h>

namespace Base {

const std::string Version = "Experimental v0.0.1"
#ifdef COMMIT_COUNT
"-" COMMIT_COUNT
#endif
;

}
