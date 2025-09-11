// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include "Core/PCI/PCIDevice.h"
#include "Core/PCI/Devices/OHCI/OHCI.h"

namespace Xe {
namespace PCIDev {

class OHCI0 : public Xe::PCIDev::OHCI {
public:
  OHCI0(const std::string &deviceName, u64 size);

private:
};

} // namespace PCIDev
} // namespace Xe
