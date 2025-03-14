// Copyright 2025 Xenon Emulator Project

#include "PPCInterpreter.h"

#include "Base/Logging/Log.h"

using namespace PPCInterpreter;

// Forward Declaration
XENON_CONTEXT* PPCInterpreter::intXCPUContext = nullptr;
RootBus* PPCInterpreter::sysBus = nullptr;
PPCInterpreter::PPCDecoder PPCInterpreter::ppcDecoder{};

// Interpreter Single Instruction Processing.
void PPCInterpreter::ppcExecuteSingleInstruction(PPU_STATE *ppuState) {
  // RGH 2 for CB_A 9188 in a JRunner XDKBuild.
  if (curThread.CIA == 0x000000000200C870) {
    // GPR(5) = 0;
  }

  // RGH 2 for CB_A 9188 in a JRunner Normal Build.
  if (curThread.CIA == 0x000000000200C820) {
    GPR(3) = 0;
  }

  // RGH 2 17489 in a JRunner Corona XDKBuild.
  if (curThread.CIA == 0x000000000200C7F0) {
    GPR(3) = 0;
  }

  // 3BL Check Bypass Devkit 2.0.1838.1
  if (curThread.CIA == 0x0000000003004994) {
    // GPR(3) = 1;
  }

  // 4BL Check Bypass Devkit 2.0.1838.1
  if (curThread.CIA == 0x0000000003004BF0) {
    // GPR(3) = 1;
  }

  // 3BL Signature Check Bypass Devkit 2.0.2853.0
  if (curThread.CIA == 0x0000000003006488) {
    // GPR(3) = 0;
  }

  // XDK 17.489.0 AudioChipCorder Device Detect bypass. This is not needed for
  // older console revisions.
  if (static_cast<u32>(curThread.CIA) == 0x801AF580) {
    return;
  }

  // This is just to set a PC breakpoint in any PPU/Thread.
  if (static_cast<u32>(curThread.CIA) == 0x8009CE40) {
    u8 a = 0;
  }

  // This is to set a PPU0[Thread0] breakpoint.
  if (curThread.SPR.PIR == 0) {
    curThread.lastRegValue = GPR(11);
  }

  instructionHandler function =
    ppcDecoder.decode(curThread.CI.opcode);

  function(ppuState);
}

//
// Exception definitions.
//

/* Exception name(Reset Vector) */

// System reset Exception (0x100)
void PPCInterpreter::ppcResetException(PPU_STATE *ppuState) {
  LOG_INFO(Xenon, "[{}](Thrd{:#d}): Reset exception.", ppuState->ppuName, static_cast<s8>(curThreadId));
  curThread.SPR.SRR0 = curThread.NIA;
  curThread.SPR.SRR1 = curThread.SPR.MSR.MSR_Hex & (QMASK(0, 32) | QMASK(37, 41) | QMASK(48, 63));
  curThread.SPR.MSR.MSR_Hex = curThread.SPR.MSR.MSR_Hex & ~(QMASK(48, 50) | QMASK(52, 55) | QMASK(58, 59) | QMASK(61, 63));
  curThread.SPR.MSR.MSR_Hex = curThread.SPR.MSR.MSR_Hex | (QMASK(0, 0) | QMASK(3, 3));
  curThread.NIA = ppuState->SPR.HRMOR + 0x100;
  curThread.SPR.MSR.DR = 0;
  curThread.SPR.MSR.IR = 0;
}
// Data Storage Exception (0x300)
void PPCInterpreter::ppcDataStorageException(PPU_STATE *ppuState) {
  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): Data Storage exception.", ppuState->ppuName, static_cast<s8>(curThreadId));
  curThread.SPR.SRR0 = curThread.CIA;
  curThread.SPR.SRR1 = curThread.SPR.MSR.MSR_Hex & (QMASK(0, 32) | QMASK(37, 41) | QMASK(48, 63));
  curThread.SPR.MSR.MSR_Hex = curThread.SPR.MSR.MSR_Hex & ~(QMASK(48, 50) | QMASK(52, 55) | QMASK(58, 59) | QMASK(61, 63));
  curThread.SPR.MSR.MSR_Hex = curThread.SPR.MSR.MSR_Hex | (QMASK(0, 0) | QMASK(3, 3));
  curThread.NIA = ppuState->SPR.HRMOR + 0x300;
  curThread.SPR.MSR.DR = 0;
  curThread.SPR.MSR.IR = 0;
}
// Data Segment Exception (0x380)
void PPCInterpreter::ppcDataSegmentException(PPU_STATE *ppuState) {
  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): Data Segment exception.", ppuState->ppuName, static_cast<s8>(curThreadId));
  curThread.SPR.SRR0 = curThread.CIA;
  curThread.SPR.SRR1 = curThread.SPR.MSR.MSR_Hex & (QMASK(0, 32) | QMASK(37, 41) | QMASK(48, 63));
  curThread.SPR.MSR.MSR_Hex = curThread.SPR.MSR.MSR_Hex & ~(QMASK(48, 50) | QMASK(52, 55) | QMASK(58, 59) | QMASK(61, 63));
  curThread.SPR.MSR.MSR_Hex = curThread.SPR.MSR.MSR_Hex | (QMASK(0, 0) | QMASK(3, 3));
  curThread.NIA = ppuState->SPR.HRMOR + 0x380;
  curThread.SPR.MSR.DR = 0;
  curThread.SPR.MSR.IR = 0;
}
// Instruction Storage Exception (0x400)
void PPCInterpreter::ppcInstStorageException(PPU_STATE *ppuState) {
  PPU_THREAD_REGISTERS& thread = curThread;

  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): Instruction Storage exception.", ppuState->ppuName, static_cast<s8>(curThreadId));
  curThread.SPR.SRR0 = curThread.CIA;
  curThread.SPR.SRR1 = curThread.SPR.MSR.MSR_Hex & (QMASK(0, 32) | QMASK(37, 41) | QMASK(48, 63));
  curThread.SPR.SRR1 |= QMASK(33, 33);
  curThread.SPR.MSR.MSR_Hex = curThread.SPR.MSR.MSR_Hex & ~(QMASK(48, 50) | QMASK(52, 55) | QMASK(58, 59) | QMASK(61, 63));
  curThread.SPR.MSR.MSR_Hex = curThread.SPR.MSR.MSR_Hex | (QMASK(0, 0) | QMASK(3, 3));
  curThread.NIA = ppuState->SPR.HRMOR + 0x400;
  curThread.SPR.MSR.DR = 0;
  curThread.SPR.MSR.IR = 0;
}
// Instruction Segment Exception (0x480)
void PPCInterpreter::ppcInstSegmentException(PPU_STATE *ppuState) {
  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): Instruction Segment exception.", ppuState->ppuName, static_cast<s8>(curThreadId));
  curThread.SPR.SRR0 = curThread.CIA;
  curThread.SPR.SRR1 = curThread.SPR.MSR.MSR_Hex & (QMASK(0, 32) | QMASK(37, 41) | QMASK(48, 63));
  curThread.SPR.MSR.MSR_Hex = curThread.SPR.MSR.MSR_Hex & ~(QMASK(48, 50) | QMASK(52, 55) | QMASK(58, 59) | QMASK(61, 63));
  curThread.SPR.MSR.MSR_Hex = curThread.SPR.MSR.MSR_Hex | (QMASK(0, 0) | QMASK(3, 3));
  curThread.NIA = ppuState->SPR.HRMOR + 0x480;
  curThread.SPR.MSR.DR = 0;
  curThread.SPR.MSR.IR = 0;
}
// External Exception (0x500)
void PPCInterpreter::ppcExternalException(PPU_STATE *ppuState) {
  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): External exception.", ppuState->ppuName, static_cast<s8>(curThreadId));
  curThread.SPR.SRR0 = curThread.NIA;
  curThread.SPR.SRR1 = curThread.SPR.MSR.MSR_Hex & (QMASK(0, 32) | QMASK(37, 41) | QMASK(48, 63));
  curThread.SPR.MSR.MSR_Hex = curThread.SPR.MSR.MSR_Hex & ~(QMASK(48, 50) | QMASK(52, 55) | QMASK(58, 59) | QMASK(61, 63));
  curThread.SPR.MSR.MSR_Hex = curThread.SPR.MSR.MSR_Hex | (QMASK(0, 0) | QMASK(3, 3));
  curThread.NIA = ppuState->SPR.HRMOR + 0x500;
  curThread.SPR.MSR.DR = 0;
  curThread.SPR.MSR.IR = 0;
}
// Program Exception (0x700)
void PPCInterpreter::ppcProgramException(PPU_STATE *ppuState) {
  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): Program exception.", ppuState->ppuName, static_cast<s8>(curThreadId));
  curThread.SPR.SRR0 = curThread.CIA;
  curThread.SPR.SRR1 = curThread.SPR.MSR.MSR_Hex & (QMASK(0, 32) | QMASK(37, 41) | QMASK(48, 63));
  BSET(curThread.SPR.SRR1, 64, curThread.exceptTrapType);
  curThread.SPR.MSR.MSR_Hex = curThread.SPR.MSR.MSR_Hex & ~(QMASK(48, 50) | QMASK(52, 55) | QMASK(58, 59) | QMASK(61, 63));
  curThread.SPR.MSR.MSR_Hex = curThread.SPR.MSR.MSR_Hex | (QMASK(0, 0) | QMASK(3, 3));
  curThread.NIA = ppuState->SPR.HRMOR + 0x700;
  curThread.SPR.MSR.DR = 0;
  curThread.SPR.MSR.IR = 0;
}

void PPCInterpreter::ppcDecrementerException(PPU_STATE *ppuState) {
  PPU_THREAD_REGISTERS& thread = curThread;

  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): Decrementer exception.", ppuState->ppuName, static_cast<s8>(curThreadId));
  curThread.SPR.SRR0 = curThread.NIA;
  curThread.SPR.SRR1 = curThread.SPR.MSR.MSR_Hex & (QMASK(0, 32) | QMASK(37, 41) | QMASK(48, 63));
  curThread.SPR.MSR.MSR_Hex =  curThread.SPR.MSR.MSR_Hex & ~(QMASK(48, 50) | QMASK(52, 55) | QMASK(58, 59) | QMASK(61, 63));
  curThread.SPR.MSR.MSR_Hex = curThread.SPR.MSR.MSR_Hex | QMASK(0, 0) | (QMASK(3, 3));
  curThread.NIA = ppuState->SPR.HRMOR + 0x900;
  curThread.SPR.MSR.DR = 0;
  curThread.SPR.MSR.IR = 0;
}

// System Call Exception (0xC00)
void PPCInterpreter::ppcSystemCallException(PPU_STATE *ppuState) {
  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): System Call exception.", ppuState->ppuName, static_cast<s8>(curThreadId));
  curThread.SPR.SRR0 = curThread.NIA;
  curThread.SPR.SRR1 = curThread.SPR.MSR.MSR_Hex & (QMASK(0, 32) | QMASK(37, 41) | QMASK(48, 63));
  curThread.SPR.MSR.MSR_Hex = curThread.SPR.MSR.MSR_Hex & ~(QMASK(48, 50) | QMASK(52, 55) | QMASK(58, 59) | QMASK(61, 63));
  curThread.SPR.MSR.MSR_Hex = curThread.SPR.MSR.MSR_Hex | QMASK(0, 0) | (curThread.exceptHVSysCall ? 0 : QMASK(3, 3));
  curThread.NIA = ppuState->SPR.HRMOR + 0xC00;
  curThread.SPR.MSR.DR = 0;
  curThread.SPR.MSR.IR = 0;
}

void PPCInterpreter::ppcInterpreterTrap(PPU_STATE *ppuState, u32 trapNumber) {
  // DbgPrint, r3 = PCSTR stringAddress, r4 = int String Size.
  if (trapNumber == 0x14) {
    std::string dbgString;
    dbgString.resize(curThread.GPR[0x4]);
    size_t strSize = (size_t)curThread.GPR[0x4];
    for (int idx = 0; idx < strSize; idx++) {
      dbgString[idx] = MMURead8(ppuState, GPR(3) + idx);
    }
    LOG_XBOX(DebugPrint, "> {}", dbgString);
  }

  if (trapNumber == 0x17) {
    // DebugLoadImageSymbols, type signature:
    // PUBLIC VOID DebugLoadImageSymbols(IN PSTRING ModuleName == $r3,
    //                   IN PKD_SYMBOLS_INFO Info == $r4)

    ppcDebugLoadImageSymbols(ppuState, GPR(3), GPR(4));
  }
  if (trapNumber == 24) {
    // DebugUnloadImageSymbols, type signature:
    // PUBLIC VOID DebugUnloadImageSymbols(IN PSTRING ModuleName == $r3,
    //                   IN PKD_SYMBOLS_INFO Info == $r4)

    ppcDebugUnloadImageSymbols(ppuState, GPR(3), GPR(4));
  }

  _ex |= PPU_EX_PROG;
  curThread.exceptTrapType = TRAP_TYPE_SRR1_TRAP_TRAP;
}
