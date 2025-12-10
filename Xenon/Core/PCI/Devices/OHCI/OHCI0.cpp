/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "OHCI0.h"
#include "Base/Logging/Log.h"

namespace Xe {
namespace PCIDev {

OHCI0::OHCI0(const std::string &deviceName, u64 size) :
  OHCI(deviceName, size, 0, 4)// Instance 0, 4 USB ports
{
  LOG_INFO(OHCI, "OHCI0 controller initialized with {} ports", 4);
}

} // namespace PCIDev
} // namespace Xe
