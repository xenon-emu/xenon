/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Core/PCI/Devices/OHCI/OHCI.h"

namespace Xe {
namespace PCIDev {

// OHCI Controller 1 - 5 ports
// This controller handles additional USB ports on the Xbox 360
class OHCI1 : public OHCI {
public:
  OHCI1(const std::string &deviceName, u64 size);
  ~OHCI1() override = default;
};

} // namespace PCIDev
} // namespace Xe
