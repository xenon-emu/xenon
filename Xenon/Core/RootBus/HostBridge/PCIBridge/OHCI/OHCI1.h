// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include "Core/RootBus/HostBridge/PCIBridge/PCIDevice.h"
#include "Core/RootBus/HostBridge/PCIBridge/OHCI/OHCI.h"

namespace Xe {
namespace PCIDev {

class OHCI1 : public OHCI {
public:
  OHCI1(const std::string &deviceName, u64 size);

private:
};

} // namespace PCIDev
} // namespace Xe
