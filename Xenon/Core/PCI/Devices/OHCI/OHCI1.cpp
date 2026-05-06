/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "OHCI1.h"

Xe::PCIDev::OHCI1::OHCI1(u64 size)
  : OHCI(__func__, size, 1, 5)
{}