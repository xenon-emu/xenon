// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Core/RootBus/HostBridge/PCIBridge/PCIDevice.h"
#include "Core/RootBus/HostBridge/PCIBridge/OHCI/OHCI.h"

#define OHCI0_DEV_SIZE 0x1000

namespace Xe {
namespace PCIDev {
namespace OHCI0 {

class OHCI0 : public OHCI {
public:
  OHCI0(const std::string &deviceName, u64 size);

private:
};

} // namespace OHCI0
} // namespace PCIDev
} // namespace Xe
