/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "OHCI1.h"
#include "Base/Logging/Log.h"

namespace Xe {
namespace PCIDev {

OHCI1::OHCI1(const std::string &deviceName, u64 size) :
  OHCI(deviceName, size, 1, 5)  // Instance 1, 5 USB ports
{
  LOG_INFO(OHCI, "OHCI1 controller initialized with {} ports", 5);
}

} // namespace PCIDev
} // namespace Xe
