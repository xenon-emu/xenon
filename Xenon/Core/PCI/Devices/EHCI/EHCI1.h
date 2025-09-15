/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Core/PCI/PCIDevice.h"
#include "Core/PCI/Devices/EHCI/EHCI.h"

namespace Xe {
namespace PCIDev {

class EHCI1 : public EHCI {
public:
  EHCI1(const std::string &deviceName, u64 size);

private:
};

} // namespace PCIDev
} // namespace Xe
