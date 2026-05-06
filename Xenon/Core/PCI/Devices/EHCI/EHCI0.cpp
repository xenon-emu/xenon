/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "EHCI0.h"

Xe::PCIDev::EHCI0::EHCI0(u64 size) :
  EHCI(__func__, size, 0, 4)
{}
