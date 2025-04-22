// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Core/RootBus/HostBridge/PCIBridge/PCIDevice.h"
#include "Core/RootBus/HostBridge/PCIBridge/OHCI/OHCI.h"

#define OHCI1_DEV_SIZE 0x1000

namespace Xe {
namespace PCIDev {
namespace OHCI1 {

class OHCI1 : public OHCI {
public:
  OHCI1(const std::string &deviceName, u64 size);

private:
};

} // namespace OHCI1
} // namespace PCIDev
} // namespace Xe
