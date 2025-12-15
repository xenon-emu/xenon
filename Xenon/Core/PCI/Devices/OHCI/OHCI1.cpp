/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "OHCI1.h"

Xe::PCIDev::OHCI1::OHCI1(const std::string &deviceName, u64 size) :
  OHCI(deviceName, size, 1, 5)
{}
