// Copyright 2025 Xenon Emulator Project

#include "EHCI0.h"

Xe::PCIDev::EHCI0::EHCI0(const std::string &deviceName, u64 size) :
  EHCI(deviceName, size, 0, 4)
{}
