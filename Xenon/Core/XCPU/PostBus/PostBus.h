// Copyright 2025 Xenon Emulator Project

#pragma once

#ifdef _WIN32
#include <windows.h>
#endif

#include "Base/Types.h"

#define POST_BUS_ADDR 0x61010ULL

namespace Xe {
namespace XCPU {
namespace POSTBUS {
void POST(u64 postCode);
}
} // namespace XCPU
} // namespace Xe
