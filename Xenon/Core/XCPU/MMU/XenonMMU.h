/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Core/XCPU/Context/XenonContext.h"
#include "Core/XCPU/PPU/PowerPC.h"

namespace Xe::XCPU::MMU {
  // The CELL B/E manual states that any CELL arch based PPE's (like the ones inside the Xenon CPU) can have the
  // 'small' 4KB Page size and any two out of the three 'large' page sizes described therein. The PPU's inside the
  // Xenon use the 'large' 64Kb and 16Mb page sizes.
  enum ePageSize : u8 {
    pSize4Kb = 12,       // 4Kb 'small' page size. p = 12 bits.
    pSize64Kb = 16,      // 64Kb 'large' page size. p = 16 bits.
    pSize16Mb = 24,      // 16Mb 'large' page size. p = 24 bits.
    pSizeUnsupported = 0 // Unknown or unsupported (1Mb) page size.
  };

  // Xenon Memory Management Unit.
  // Performs address translation and the physical routing of guest PPU Thread's accesses to System Devices and Busses.
  class XenonMMU {
  public:
    XenonMMU(XenonContext* inXenonContext, sPPEState* inPPEState)
        : xenonContext(inXenonContext)
        , ppeState(inPPEState) { }

    //
    // Address translation
    //

    // Main effective->real address translation. Returns false and raises the
    // appropriate storage/segment exception on the addressed thread on failure.
    bool MMUTranslateAddress(u64* EA, bool memWrite, ePPUThreadID thr = ePPUThread_None);

    //
    // Guest memory access (translate + physical routing)
    //

    void MMURead(u64 EA, u64 byteCount, u8* outData, ePPUThreadID thr = ePPUThread_None);
    void MMUWrite(const u8* data, u64 EA, u64 byteCount, ePPUThreadID thr = ePPUThread_None);

    void MMUMemCpyFromHost(u64 EA, const void* source, u64 size, ePPUThreadID thr = ePPUThread_None);
    void MMUMemCpy(u64 EA, u32 source, u64 size, ePPUThreadID thr = ePPUThread_None);
    void MMUMemSet(u64 EA, s32 data, u64 size, ePPUThreadID thr = ePPUThread_None);

    // Raw host pointer into guest RAM (no translation, no bus, no SoC handling).
    u8* MMUGetPointerFromRAM(u64 EA);

    // Sized helpers (big-endian guest <-> host byteswap included).
    u8 MMURead8(u64 EA, ePPUThreadID thr = ePPUThread_None);
    u16 MMURead16(u64 EA, ePPUThreadID thr = ePPUThread_None);
    u32 MMURead32(u64 EA, ePPUThreadID thr = ePPUThread_None);
    u64 MMURead64(u64 EA, ePPUThreadID thr = ePPUThread_None);
    void MMUWrite8(u64 EA, u8 data, ePPUThreadID thr = ePPUThread_None);
    void MMUWrite16(u64 EA, u16 data, ePPUThreadID thr = ePPUThread_None);
    void MMUWrite32(u64 EA, u32 data, ePPUThreadID thr = ePPUThread_None);
    void MMUWrite64(u64 EA, u64 data, ePPUThreadID thr = ePPUThread_None);

    // Reads a kernel PSTRING from guest memory into a host buffer.
    void ReadString(u64 stringAddress, char* string, u32 maxLength);

    //
    // TLB / page-size services used by the interpreter's SLB/TLB opcode handlers
    //

    // Page size (p, log2 bytes) for the current HID6 large-page config.
    u8 GetPageSize(bool L, u8 LP);
    // Software-managed TLB insert from the PPE_TLB_* SPR image.
    void AddTlbEntry();
    // Selective (IS=0) TLB invalidation: clear ways whose tag matches VA at page size p.
    void TlbInvalidateSelective(u64 VA, u8 p, bool L);

  private:
    // Xenon CPU Context.
    XenonContext* xenonContext = nullptr;
    // The PPE state this MMU translates for.
    sPPEState* ppeState = nullptr;

    // Hardware-managed TLB insert (HTAB walk path).
    void AddTlbEntryHardware(u64 VA, u64 pte0, u64 pte1, u8 p, bool L, bool LP);
    // TLB lookup. Returns true and sets *RPN on hit.
    bool SearchTlbEntry(u64* RPN, u64 VA, u8 p, bool L, bool LP);

    // Hardware page-table (HTAB) walk. Searches the primary then secondary PTEG for a PTE
    // matching (vsid, EA) at page size p. On a hit it updates the PTE Reference/Change bits,
    // writes them back to guest memory, reloads the TLB, and returns the RPN via *RPN.
    bool WalkPageTable(u64* RPN, u64 EA, u64 VA, u64 vsid, u8 p, bool L, bool LP, bool memWrite);

    // Security-engine address decoding (Xbox 360 specific).
    SECENG_ADDRESS_INFO GetSecEngInfoFromAddress(u64 inputAddress);
    u64 ContructEndAddressFromSecEngAddr(u64 inputAddress, bool* socAccess);
  };
} // namespace Xe::XCPU::MMU
