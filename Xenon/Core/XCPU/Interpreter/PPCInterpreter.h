// Copyright 2025 Xenon Emulator Project

#pragma once

#include "PPCInternal.h"

#include "PPC_Instruction.h"
#include "PPCOpcodes.h"

#include "Core/RootBus/RootBus.h"
#include "Core/XCPU/PPU/PowerPC.h"

namespace PPCInterpreter {
extern PPCInterpreter::PPCDecoder ppcDecoder;
extern RootBus *sysBus;
extern XENON_CONTEXT *intXCPUContext;

//
//  Helper macros for instructions
//
#define curThread     ppuState->ppuThread[ppuState->currentThread]
#define _instr        curThread.CI
#define _ex           curThread.exceptReg
#define GPR(x)        curThread.GPR[x]
#define GPRi(x)       GPR(_instr.x)
#define XER_SET_CA(v) curThread.SPR.XER.CA = v
#define XER_GET_CA    curThread.SPR.XER.CA
//
// Floating Point helpers
//
#define FPR(x)        curThread.FPR[x]
#define FPRi(x)        curThread.FPR[_instr.x]
#define GET_FPSCR     curThread.FPSCR.FPSCR_Hex
#define SET_FPSCR(x)  curThread.FPSCR.FPSCR_Hex = x

//
//  Basic Block Loading, debug symbols and stuff.
//
struct KD_SYMBOLS_INFO {
  u32 BaseOfDll;
  u32 ProcessId;
  u32 CheckSum;
  u32 SizeOfImage;
};

void ppcDebugLoadImageSymbols(PPU_STATE *ppuState, u64 moduleNameAddress,
                              u64 moduleInfoAddress);
void ppcDebugUnloadImageSymbols(PPU_STATE *ppuState, u64 moduleNameAddress,
                                u64 moduleInfoAddress);

//
// Condition Register
//

// Compare Unsigned
u32 CRCompU(PPU_STATE *ppuState, u64 num1, u64 num2);
// Compare Signed 32 bits
u32 CRCompS32(PPU_STATE *ppuState, u32 num1, u32 num2);
// Compare Signed 64 bits
u32 CRCompS64(PPU_STATE *ppuState, u64 num1, u64 num2);
// Compare Unsigned
u32 CRCompS(PPU_STATE *ppuState, u64 num1, u64 num2);
// Condition register Update
void ppcUpdateCR(PPU_STATE *ppuState, s8 crNum, u32 crValue);

// Single instruction execution
void ppcExecuteSingleInstruction(PPU_STATE *ppuState);

//
// Exceptions
//

#define TRAP_TYPE_SRR1_TRAP_FPU 43
#define TRAP_TYPE_SRR1_TRAP_ILL 44
#define TRAP_TYPE_SRR1_TRAP_PRIV 45
#define TRAP_TYPE_SRR1_TRAP_TRAP 46

void ppcResetException(PPU_STATE *ppuState);
void ppcInterpreterTrap(PPU_STATE *ppuState, u32 trapNumber);
void ppcInstStorageException(PPU_STATE *ppuState);
void ppcDataStorageException(PPU_STATE *ppuState);
void ppcDataSegmentException(PPU_STATE *ppuState);
void ppcInstSegmentException(PPU_STATE *ppuState);
void ppcSystemCallException(PPU_STATE *ppuState);
void ppcDecrementerException(PPU_STATE *ppuState);
void ppcProgramException(PPU_STATE *ppuState);
void ppcExternalException(PPU_STATE *ppuState);

//
// MMU
//

bool MMUTranslateAddress(u64 *EA, PPU_STATE *ppuState, bool memWrite, bool speculativeLoad = false);
u8 mmuGetPageSize(PPU_STATE *ppuState, bool L, u8 LP);
void mmuAddTlbEntry(PPU_STATE *ppuState);
bool mmuSearchTlbEntry(PPU_STATE *ppuState, u64 *RPN, u64 VA, u64 VPN, u8 p,
                       bool LP);
void mmuReadString(PPU_STATE *ppuState, u64 stringAddress, char *string,
                   u32 maxLenght);

// Security Engine Related
SECENG_ADDRESS_INFO mmuGetSecEngInfoFromAddress(u64 inputAddress);
u64 mmuContructEndAddressFromSecEngAddr(u64 inputAddress, bool *socAccess);

// Main R/W Routines.
u64 MMURead(XENON_CONTEXT *cpuContext, PPU_STATE *ppuState, u64 EA,
            s8 byteCount, bool speculativeLoad = false);
void MMUWrite(XENON_CONTEXT *cpuContext, PPU_STATE *ppuState, u64 data, u64 EA,
              s8 byteCount, bool cacheStore = false);

void MMUMemCpyFromHost(PPU_STATE* ppuState, u32 dest, const void* source, u64 size, bool cacheStore = false);

void MMUMemCpy(PPU_STATE* ppuState, u32 dest, u32 source, u64 size, bool cacheStore = false);

// Helper Read Routines.
u8 MMURead8(PPU_STATE *ppuState, u64 EA, bool speculativeLoad = false);
u16 MMURead16(PPU_STATE *ppuState, u64 EA, bool speculativeLoad = false);
u32 MMURead32(PPU_STATE *ppuState, u64 EA, bool speculativeLoad = false);
u64 MMURead64(PPU_STATE *ppuState, u64 EA, bool speculativeLoad = false);
// Helper Write Routines.
void MMUWrite8(PPU_STATE *ppuState, u64 EA, u8 data);
void MMUWrite16(PPU_STATE *ppuState, u64 EA, u16 data);
void MMUWrite32(PPU_STATE *ppuState, u64 EA, u32 data);
void MMUWrite64(PPU_STATE *ppuState, u64 EA, u64 data);
} // namespace PPCInterpreter
