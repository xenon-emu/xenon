/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "EHCI1.h"

Xe::PCIDev::EHCI1::EHCI1(u64 size) :
  EHCI(__func__, size, 1, 5)
{}
