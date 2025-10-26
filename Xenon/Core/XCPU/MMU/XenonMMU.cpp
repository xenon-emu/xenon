/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Base/Global.h"

#include "XenonMMU.h"

namespace Xe::XCPU::MMU {
  ePageSize XenonMMU::GetCurrentPageSize(sPPEState* ppeState, bool L, u8 LP) {
    MICROPROFILE_SCOPEI("[Xe::PPCInterpreter]", "MMUGetPageSize", MP_AUTO);

    // Large page selection works the following way:
    // First check if pages are large (L)
    // if (L) the page size can be one of two defined pages. On the Xbox 360,
    // MS decided to use two of the three page sizes, 64Kb and 16Mb.
    // Selection between them is made using bits 16 - 19 of HID6 SPR.

    // HID6 16-17 bits select Large Page size 1.
    // HID6 18-19 bits select Large Page size 2.
    const u8 lb16to17 = (ppeState->SPR.HID6.LB & 0b1100) >> 2;
    const u8 lb18to19 = ppeState->SPR.HID6.LB & 0b11;

    // Page size.
    ePageSize pSize = pSize4Kb; // We always default to a 4Kb page size.

    // Large page?
    if (L) {
      // Large Page Selector
      if (LP == 0) {
        switch (lb16to17) {
        case 0b0000: pSize = pSize16Mb; break;
        case 0b0001: pSize = pSizeUnsupported; break;
        case 0b0010: pSize = pSize64Kb; break;
        }
      }
      else if (LP == 1) {
        switch (lb18to19) {
        case 0b0000: pSize = pSize16Mb; break;
        case 0b0001: pSize = pSizeUnsupported; break;
        case 0b0010: pSize = pSize64Kb; break;
        }
      }
    }
    return pSize;
  }
}