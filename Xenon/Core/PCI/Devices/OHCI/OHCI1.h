/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Core/PCI/PCIDevice.h"
#include "Core/PCI/Devices/OHCI/OHCI.h"

namespace Xe::PCIDev {

class OHCI1 : public OHCI {
public:
  OHCI1(u64 size);
};

} // namespace Xe::PCIDev