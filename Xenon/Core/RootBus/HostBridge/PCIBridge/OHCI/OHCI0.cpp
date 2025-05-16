// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "OHCI0.h"

Xe::PCIDev::OHCI0::OHCI0(const std::string &deviceName, u64 size) :
  OHCI(deviceName, size, 0, 4)
{}
