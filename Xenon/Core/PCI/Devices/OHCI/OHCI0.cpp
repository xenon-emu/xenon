/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "OHCI0.h"

Xe::PCIDev::OHCI0::OHCI0(u64 size)
  : OHCI(__func__, size, 1, 5)
{}