// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include "PPCInternal.h"

#include "PPC_Instruction.h"
#include "PPCOpcodes.h"

#include "Core/XCPU/XenonContext.h"
#include "Core/RootBus/RootBus.h"
#include "Core/XCPU/PPU/PowerPC.h"

namespace PPCInterpreter {

extern PPCInterpreter::PPCDecoder ppcDecoder;
extern Xe::XCPU::XenonContext *xenonContext;

//
//  Helper macros for instructions
//
#define curThreadId   ppeState->currentThread
#define curThread     ppeState->ppuThread[curThreadId]
#define _previnstr    curThread.PI
#define _instr        curThread.CI
#define _nextinstr    curThread.NI
#define _ex           curThread.exceptReg
#define GPR(x)        curThread.GPR[x]
#define GPRi(x)       GPR(_instr.x)
#define XER_SET_CA(v) curThread.SPR.XER.CA = v
#define XER_GET_CA    curThread.SPR.XER.CA

//
// Floating Point helpers
//

#define FPR(x)        curThread.FPR[x]
#define FPRi(x)       curThread.FPR[_instr.x]
#define GET_FPSCR     curThread.FPSCR.FPSCR_Hex
#define SET_FPSCR(x)  curThread.FPSCR.FPSCR_Hex = x
// Check for Enabled FPU.
#define CHECK_FPU     if (!checkFpuAvailable(ppeState)) { return; }
// Converts a given number into an integer.
void ConvertToInteger(sPPEState* ppeState, eFPRoundMode roundingMode);
void FPCompareOrdered(sPPEState* ppeState, double fra, double frb);
void FPCompareUnordered(sPPEState* ppeState, double fra, double frb);
//
// VXU Helpers
//
#define VR(x)         curThread.VR[x]
#define VRi(x)        curThread.VR[_instr.x]
// Check for Enabled VXU.
#define CHECK_VXU     if (!checkVxuAvailable(ppeState)) { return; }


static inline bool checkFpuAvailable(sPPEState *ppeState) {
  if (curThread.SPR.MSR.FP != 1) {
    _ex |= PPU_EX_FPU;
    return false;
  }
  return true;
}

static inline bool checkVxuAvailable(sPPEState *ppeState) {
  if (curThread.SPR.MSR.VXU != 1) {
    _ex |= PPU_EX_VXU;
    return false;
  }
  return true;
}

//
//  Basic Block Loading, debug symbols and stuff.
//
struct KD_SYMBOLS_INFO {
  u32 BaseOfDll;
  u32 ProcessId;
  u32 CheckSum;
  u32 SizeOfImage;
};

void ppcDebugLoadImageSymbols(sPPEState *ppeState, u64 moduleNameAddress,
                              u64 moduleInfoAddress);
void ppcDebugUnloadImageSymbols(sPPEState *ppeState, u64 moduleNameAddress,
                                u64 moduleInfoAddress);

//
// Condition Register
//

#define CR_CASE(x) \
case x: \
  curThread.CR.CR##x = crValue; \
  break;

// Condition register Update
inline void ppcUpdateCR(sPPEState *ppeState, s8 crNum, u32 crValue) {
switch (crNum) {
  CR_CASE(0)
  CR_CASE(1)
  CR_CASE(2)
  CR_CASE(3)
  CR_CASE(4)
  CR_CASE(5)
  CR_CASE(6)
  CR_CASE(7)
  }
}

// Write values to CR field
inline void ppuSetCR(sPPEState *ppeState, u32 crField, bool le, bool gt, bool eq, bool so) {
  u32 crValue = 0;
  le ? BSET(crValue, 4, CR_BIT_LT) : BCLR(crValue, 4, CR_BIT_LT);
  gt ? BSET(crValue, 4, CR_BIT_GT) : BCLR(crValue, 4, CR_BIT_GT);
  eq ? BSET(crValue, 4, CR_BIT_EQ) : BCLR(crValue, 4, CR_BIT_EQ);
  so ? BSET(crValue, 4, CR_BIT_SO) : BCLR(crValue, 4, CR_BIT_SO);
  ppcUpdateCR(ppeState, crField, crValue);
}

// Perform a comparison and write results to the specified CR field.
template <typename T>
inline void ppuSetCR(sPPEState *ppeState, u32 crField, const T& a, const T& b) {
  ppuSetCR(ppeState, crField, a < b, a > b, a == b, curThread.SPR.XER.SO);
}

// // Updates CR1 field based on the contents of FPSCR.
void ppuSetCR1(sPPEState* ppeState);

// Update FPSCR FPCC bits and CR if requested. Default CR to be updated is 1.
void ppuUpdateFPSCR(sPPEState *ppeState, f64 op0, f64 op1, bool updateCR, u8 CR = 1);

// Compare Unsigned
u32 CRCompU(sPPEState *ppeState, u64 num1, u64 num2);
// Compare Signed 32 bits
u32 CRCompS32(sPPEState *ppeState, u32 num1, u32 num2);
// Compare Signed 64 bits
u32 CRCompS64(sPPEState *ppeState, u64 num1, u64 num2);
// Compare Signed
u32 CRCompS(sPPEState *ppeState, u64 num1, u64 num2);

// Single instruction execution
void ppcExecuteSingleInstruction(sPPEState *ppeState);

void ppcInterpreterTrap(sPPEState* ppeState, u32 trapNumber);

//
// MMU
//

bool MMUTranslateAddress(u64 *EA, sPPEState *ppeState, bool memWrite, ePPUThreadID thr = ePPUThread_None);
u8 mmuGetPageSize(sPPEState *ppeState, bool L, u8 LP);
void mmuAddTlbEntry(sPPEState *ppeState);
bool mmuSearchTlbEntry(sPPEState *ppeState, u64 *RPN, u64 VA, u8 p, bool L, bool LP);
void mmuReadString(sPPEState *ppeState, u64 stringAddress, char *string, u32 maxLength);

// Security Engine Related
SECENG_ADDRESS_INFO mmuGetSecEngInfoFromAddress(u64 inputAddress);
u64 mmuContructEndAddressFromSecEngAddr(u64 inputAddress, bool *socAccess);

// Main R/W Routines.
void MMURead(Xe::XCPU::XenonContext *cpuContext, sPPEState *ppeState,
            u64 EA, u64 byteCount, u8 *outData, ePPUThreadID thr = ePPUThread_None);
void MMUWrite(Xe::XCPU::XenonContext *cpuContext, sPPEState *ppeState,
              const u8* data, u64 EA, u64 byteCount, ePPUThreadID thr = ePPUThread_None);

void MMUMemCpyFromHost(sPPEState *ppeState, u64 EA, const void *source, u64 size, ePPUThreadID thr = ePPUThread_None);

void MMUMemCpy(sPPEState *ppeState, u64 EA, u32 source, u64 size, ePPUThreadID thr = ePPUThread_None);

void MMUMemSet(sPPEState *ppeState, u64 EA, s32 data, u64 size, ePPUThreadID thr = ePPUThread_None);

u8 *MMUGetPointerFromRAM(u64 EA);

// Helper Read Routines.
u8 MMURead8(sPPEState *ppeState, u64 EA, ePPUThreadID thr = ePPUThread_None);
u16 MMURead16(sPPEState *ppeState, u64 EA, ePPUThreadID thr = ePPUThread_None);
u32 MMURead32(sPPEState *ppeState, u64 EA, ePPUThreadID thr = ePPUThread_None);
u64 MMURead64(sPPEState *ppeState, u64 EA, ePPUThreadID thr = ePPUThread_None);
// Helper Write Routines.
void MMUWrite8(sPPEState *ppeState, u64 EA, u8 data, ePPUThreadID thr = ePPUThread_None);
void MMUWrite16(sPPEState *ppeState, u64 EA, u16 data, ePPUThreadID thr = ePPUThread_None);
void MMUWrite32(sPPEState *ppeState, u64 EA, u32 data, ePPUThreadID thr = ePPUThread_None);
void MMUWrite64(sPPEState *ppeState, u64 EA, u64 data, ePPUThreadID thr = ePPUThread_None);

} // namespace PPCInterpreter
