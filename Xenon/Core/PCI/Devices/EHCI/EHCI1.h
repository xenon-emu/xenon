/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Core/PCI/Devices/EHCI/EHCI.h"
#include "Core/PCI/PCIDevice.h"

namespace Xe::PCIDev {

  class EHCI1 : public EHCI {
  public:
    EHCI1(const std::string &deviceName, u64 size, PCIBridge *parentPCIBridge, RAM *ram,
          std::shared_ptr<USB::PortGroup> ports);
  };

} // namespace Xe::PCIDev
