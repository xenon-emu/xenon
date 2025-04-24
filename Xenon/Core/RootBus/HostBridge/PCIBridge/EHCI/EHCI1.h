// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Core/RootBus/HostBridge/PCIBridge/PCIDevice.h"
#include "Core/RootBus/HostBridge/PCIBridge/EHCI/EHCI.h"

namespace Xe {
namespace PCIDev {

class EHCI1 : public EHCI {
public:
  EHCI1(const std::string &deviceName, u64 size);

private:
};

} // namespace PCIDev
} // namespace Xe
