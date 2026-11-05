/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Core/PCI/Devices/OHCI/OHCI.h"
#include "Core/PCI/PCIDevice.h"

namespace Xe::PCIDev {

  class OHCI1 : public OHCI {
  public:
    OHCI1(const std::string &deviceName, u64 size, PCIBridge *parentPCIBridge, RAM *ram,
          std::shared_ptr<USB::PortGroup> ports);
  };

} // namespace Xe::PCIDev
