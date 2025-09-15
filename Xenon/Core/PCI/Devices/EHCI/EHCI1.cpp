/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "EHCI1.h"

Xe::PCIDev::EHCI1::EHCI1(const std::string &deviceName, u64 size) :
  EHCI(deviceName, size, 1, 5)
{}
