/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#ifdef _WIN32
#include <Windows.h>
#endif

#define POST_BUS_ADDR 0x61010ULL

namespace Xe {
namespace XCPU {
namespace POSTBUS {
void POST(u64 postCode);
}
} // namespace XCPU
} // namespace Xe
