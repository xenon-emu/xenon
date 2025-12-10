/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "OHCI.h"

namespace Xe {
namespace PCIDev {

// OHCI Controller 0 - 4 ports
// This controller is typically used for Xbox 360 controllers and accessories
class OHCI0 : public OHCI {
public:
  OHCI0(const std::string &deviceName, u64 size);
  ~OHCI0() override = default;
};

} // namespace PCIDev
} // namespace Xe
