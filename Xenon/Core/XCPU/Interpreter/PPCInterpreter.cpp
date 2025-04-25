// Copyright 2025 Xenon Emulator Project

#include "PPCInterpreter.h"

#include "Base/Logging/Log.h"

using namespace PPCInterpreter;

// Forward Declaration
XENON_CONTEXT* PPCInterpreter::CPUContext = nullptr;
RootBus* PPCInterpreter::sysBus = nullptr;
PPCInterpreter::PPCDecoder PPCInterpreter::ppcDecoder{};

// Interpreter Single Instruction Processing.
void PPCInterpreter::ppcExecuteSingleInstruction(PPU_STATE *ppuState) {
  PPU_THREAD_REGISTERS &thread = curThread;

  // RGH 2 for CB_A 9188 in a JRunner XDKBuild.
  if (thread.CIA == 0x0200C870) {
    GPR(5) = 0;
  }

  // RGH 2 for CB_A 9188 in a JRunner Normal Build.
  if (thread.CIA == 0x0200C820) {
    //GPR(3) = 0;
  }

  // RGH 2 17489 in a JRunner Corona XDKBuild.
  if (thread.CIA == 0x0200C7F0) {
    GPR(3) = 0;
  }

  // 3BL Check Bypass Devkit 2.0.1838.1
  if (thread.CIA == 0x03004994) {
    //GPR(3) = 1;
  }

  // 4BL Check Bypass Devkit 2.0.1838.1
  if (thread.CIA == 0x03004BF0) {
    //GPR(3) = 1;
  }

  // 3BL Signature Check Bypass Devkit 2.0.2853.0
  if (thread.CIA == 0x03006488) {
    //GPR(3) = 0;
  }

  // INIT_POWER_MODE bypass 2.0.17489.0.
  if ((u32)thread.CIA == 0x80081764) {
    return;
  }

  // XAudioRenderDriverInitialize bypass 2.0.17489.0.
  if ((u32)thread.CIA == 0x80081830) {
    return;
  }

  // XDK 17.489.0 AudioChipCorder Device Detect bypass. This is not needed for
  // older console revisions.
  if (static_cast<u32>(thread.CIA) == 0x801AF580) {
    return;
  }

  // EDRAM Chip ID Check. r10 = 0xD1.
  if (static_cast<u32>(thread.CIA) == 0x800FC140) {
    LOG_INFO(Xenon, "EDRAM Chip ID Check.");
    thread.GPR[10] = 0xD1;
  }
 
  // VdpHasWarmBooted. Set r11 to 0x10. Skips GPU Driver init.
  if (static_cast<u32>(thread.CIA) == 0x801b1c38) {
    LOG_INFO(Xenon, "VdpHasWarmBooted");
    thread.GPR[11] = 0x10;
  }

  // VdpWriteXDVOUllong. Set r10 to 1. Skips XDVO write loop.
  if (static_cast<u32>(thread.CIA) == 0x800ef7c0) {
    LOG_INFO(Xenon, "VdpWriteXDVOUllong");
    thread.GPR[10] = 1;
  }

  // VdpSetDisplayTimingParameter. Set r11 to 0x10. Skips ANA Check.
  if (static_cast<u32>(thread.CIA) == 0x800f6264) {
    LOG_INFO(Xenon, "VdpSetDisplayTimingParameter");
    thread.GPR[11] = 0x15E;
  }

  // VdSwap Call. 2.0.17489.0
  if (static_cast<u32>(thread.CIA) == 0x800F8E20) {
    LOG_INFO(Xenon, "*** VdSwap ***");
  }

  // This is just to set a PC breakpoint in any PPU/Thread.
  if (static_cast<u32>(thread.CIA) == 0x8009CE40) {
    u8 a = 0;
  }

  // This is to set a PPU0[Thread0] breakpoint.
  if (thread.SPR.PIR == 0) {
    thread.lastRegValue = GPR(11);
  }

  instructionHandler function =
    ppcDecoder.decode(thread.CI.opcode);

  function(ppuState);
}

//
// Exception definitions.
//

/* Exception name(Reset Vector) */

// System reset Exception (0x100)
void PPCInterpreter::ppcResetException(PPU_STATE *ppuState) {
  PPU_THREAD_REGISTERS &thread = curThread;

  LOG_INFO(Xenon, "[{}](Thrd{:#d}): Reset exception.", ppuState->ppuName, static_cast<s8>(curThreadId));
  thread.SPR.SRR0 = thread.NIA;
  thread.SPR.SRR1 = thread.SPR.MSR.MSR_Hex & (QMASK(0, 32) | QMASK(37, 41) | QMASK(48, 63));
  thread.SPR.MSR.MSR_Hex = thread.SPR.MSR.MSR_Hex & ~(QMASK(48, 50) | QMASK(52, 55) | QMASK(58, 59) | QMASK(61, 63));
  thread.SPR.MSR.MSR_Hex = thread.SPR.MSR.MSR_Hex | (QMASK(0, 0) | QMASK(3, 3));
  thread.NIA = 0x100;
  thread.SPR.MSR.DR = 0;
  thread.SPR.MSR.IR = 0;
}
// Data Storage Exception (0x300)
void PPCInterpreter::ppcDataStorageException(PPU_STATE *ppuState) {
  PPU_THREAD_REGISTERS &thread = curThread;

  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): Data Storage exception. EA: {:#x}.", ppuState->ppuName, static_cast<s8>(curThreadId), thread.SPR.DAR);
  thread.SPR.SRR0 = thread.CIA;
  thread.SPR.SRR1 = thread.SPR.MSR.MSR_Hex & (QMASK(0, 32) | QMASK(37, 41) | QMASK(48, 63));
  thread.SPR.MSR.MSR_Hex = thread.SPR.MSR.MSR_Hex & ~(QMASK(48, 50) | QMASK(52, 55) | QMASK(58, 59) | QMASK(61, 63));
  thread.SPR.MSR.MSR_Hex = thread.SPR.MSR.MSR_Hex | (QMASK(0, 0) | QMASK(3, 3));
  thread.NIA = 0x300;
  thread.SPR.MSR.DR = 0;
  thread.SPR.MSR.IR = 0;
}
// Data Segment Exception (0x380)
void PPCInterpreter::ppcDataSegmentException(PPU_STATE *ppuState) {
  PPU_THREAD_REGISTERS &thread = curThread;

  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): Data Segment exception.", ppuState->ppuName, static_cast<s8>(curThreadId));
  thread.SPR.SRR0 = thread.CIA;
  thread.SPR.SRR1 = thread.SPR.MSR.MSR_Hex & (QMASK(0, 32) | QMASK(37, 41) | QMASK(48, 63));
  thread.SPR.MSR.MSR_Hex = thread.SPR.MSR.MSR_Hex & ~(QMASK(48, 50) | QMASK(52, 55) | QMASK(58, 59) | QMASK(61, 63));
  thread.SPR.MSR.MSR_Hex = thread.SPR.MSR.MSR_Hex | (QMASK(0, 0) | QMASK(3, 3));
  thread.NIA = 0x380;
  thread.SPR.MSR.DR = 0;
  thread.SPR.MSR.IR = 0;
}
// Instruction Storage Exception (0x400)
void PPCInterpreter::ppcInstStorageException(PPU_STATE *ppuState) {
  PPU_THREAD_REGISTERS &thread = curThread;

  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): Instruction Storage exception. EA = {:#x}", ppuState->ppuName, static_cast<s8>(curThreadId), thread.CIA);
  thread.SPR.SRR0 = thread.CIA;
  thread.SPR.SRR1 = thread.SPR.MSR.MSR_Hex & (QMASK(0, 32) | QMASK(37, 41) | QMASK(48, 63));
  thread.SPR.SRR1 |= QMASK(33, 33);
  thread.SPR.MSR.MSR_Hex = thread.SPR.MSR.MSR_Hex & ~(QMASK(48, 50) | QMASK(52, 55) | QMASK(58, 59) | QMASK(61, 63));
  thread.SPR.MSR.MSR_Hex = thread.SPR.MSR.MSR_Hex | (QMASK(0, 0) | QMASK(3, 3));
  thread.NIA = 0x400;
  thread.SPR.MSR.DR = 0;
  thread.SPR.MSR.IR = 0;
}
// Instruction Segment Exception (0x480)
void PPCInterpreter::ppcInstSegmentException(PPU_STATE *ppuState) {
  PPU_THREAD_REGISTERS &thread = curThread;

  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): Instruction Segment exception.", ppuState->ppuName, static_cast<s8>(curThreadId));
  thread.SPR.SRR0 = thread.CIA;
  thread.SPR.SRR1 = thread.SPR.MSR.MSR_Hex & (QMASK(0, 32) | QMASK(37, 41) | QMASK(48, 63));
  thread.SPR.MSR.MSR_Hex = thread.SPR.MSR.MSR_Hex & ~(QMASK(48, 50) | QMASK(52, 55) | QMASK(58, 59) | QMASK(61, 63));
  thread.SPR.MSR.MSR_Hex = thread.SPR.MSR.MSR_Hex | (QMASK(0, 0) | QMASK(3, 3));
  thread.NIA = 0x480;
  thread.SPR.MSR.DR = 0;
  thread.SPR.MSR.IR = 0;
}
// External Exception (0x500)
void PPCInterpreter::ppcExternalException(PPU_STATE *ppuState) {
  PPU_THREAD_REGISTERS &thread = curThread;

  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): External exception.", ppuState->ppuName, static_cast<s8>(curThreadId));
  thread.SPR.SRR0 = thread.NIA;
  thread.SPR.SRR1 = thread.SPR.MSR.MSR_Hex & (QMASK(0, 32) | QMASK(37, 41) | QMASK(48, 63));
  thread.SPR.MSR.MSR_Hex = thread.SPR.MSR.MSR_Hex & ~(QMASK(48, 50) | QMASK(52, 55) | QMASK(58, 59) | QMASK(61, 63));
  thread.SPR.MSR.MSR_Hex = thread.SPR.MSR.MSR_Hex | (QMASK(0, 0) | QMASK(3, 3));
  thread.NIA = 0x500;
  thread.SPR.MSR.DR = 0;
  thread.SPR.MSR.IR = 0;
}
// Program Exception (0x700)
void PPCInterpreter::ppcProgramException(PPU_STATE *ppuState) {
  PPU_THREAD_REGISTERS &thread = curThread;

  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): Program exception.", ppuState->ppuName, static_cast<s8>(curThreadId));
  thread.SPR.SRR0 = thread.CIA;
  thread.SPR.SRR1 = thread.SPR.MSR.MSR_Hex & (QMASK(0, 32) | QMASK(37, 41) | QMASK(48, 63));
  BSET(thread.SPR.SRR1, 64, thread.exceptTrapType);
  thread.SPR.MSR.MSR_Hex = thread.SPR.MSR.MSR_Hex & ~(QMASK(48, 50) | QMASK(52, 55) | QMASK(58, 59) | QMASK(61, 63));
  thread.SPR.MSR.MSR_Hex = thread.SPR.MSR.MSR_Hex | (QMASK(0, 0) | QMASK(3, 3));
  thread.NIA = 0x700;
  thread.SPR.MSR.DR = 0;
  thread.SPR.MSR.IR = 0;
}

void PPCInterpreter::ppcDecrementerException(PPU_STATE *ppuState) {
  PPU_THREAD_REGISTERS &thread = curThread;

  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): Decrementer exception.", ppuState->ppuName, static_cast<s8>(curThreadId));
  thread.SPR.SRR0 = thread.NIA;
  thread.SPR.SRR1 = thread.SPR.MSR.MSR_Hex & (QMASK(0, 32) | QMASK(37, 41) | QMASK(48, 63));
  thread.SPR.MSR.MSR_Hex =  thread.SPR.MSR.MSR_Hex & ~(QMASK(48, 50) | QMASK(52, 55) | QMASK(58, 59) | QMASK(61, 63));
  thread.SPR.MSR.MSR_Hex = thread.SPR.MSR.MSR_Hex | QMASK(0, 0) | (QMASK(3, 3));
  thread.NIA = 0x900;
  thread.SPR.MSR.DR = 0;
  thread.SPR.MSR.IR = 0;
}

// System Call Exception (0xC00)
void PPCInterpreter::ppcSystemCallException(PPU_STATE *ppuState) {
  PPU_THREAD_REGISTERS &thread = curThread;

  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): System Call exception. Syscall ID: {:#x}", ppuState->ppuName, static_cast<s8>(curThreadId), GPR(0));
  thread.SPR.SRR0 = thread.NIA;
  thread.SPR.SRR1 = thread.SPR.MSR.MSR_Hex & (QMASK(0, 32) | QMASK(37, 41) | QMASK(48, 63));
  thread.SPR.MSR.MSR_Hex = thread.SPR.MSR.MSR_Hex & ~(QMASK(48, 50) | QMASK(52, 55) | QMASK(58, 59) | QMASK(61, 63));
  thread.SPR.MSR.MSR_Hex = thread.SPR.MSR.MSR_Hex | QMASK(0, 0) | (thread.exceptHVSysCall ? 0 : QMASK(3, 3));
  thread.NIA = 0xC00;
  thread.SPR.MSR.DR = 0;
  thread.SPR.MSR.IR = 0;
}

void PPCInterpreter::ppcInterpreterTrap(PPU_STATE *ppuState, u32 trapNumber) {
  PPU_THREAD_REGISTERS &thread = curThread;
  // Hacky optimization
  #undef curThread
  #define curThread thread

  // DbgPrint, r3 = PCSTR stringAddress, r4 = int String Size.
  switch (trapNumber) {
  case 0x14: {
    u32 strAddr = GPR(3);
    u64 strSize = (u64)GPR(4);
    std::unique_ptr<u8[]> buffer = std::make_unique<STRIP_UNIQUE_ARR(buffer)>(strSize+1);
    MMURead(CPUContext, ppuState, strAddr, strSize, buffer.get());
    char *dbgString = reinterpret_cast<char*>(buffer.get());
    dbgString[strSize] = '\0'; // nul-term
    Base::Log::NoFmtMessage(Base::Log::Class::DebugPrint, Base::Log::Level::Guest, dbgString);
    break;
  }
  case 0x17:
    // DebugLoadImageSymbols, type signature:
    // PUBLIC VOID DebugLoadImageSymbols(IN PSTRING ModuleName == $r3,
    //                   IN PKD_SYMBOLS_INFO Info == $r4)
    ppcDebugLoadImageSymbols(ppuState, GPR(3), GPR(4));
    break;
  case 0x18:
    // DebugUnloadImageSymbols, type signature:
    // PUBLIC VOID DebugUnloadImageSymbols(IN PSTRING ModuleName == $r3,
    //                   IN PKD_SYMBOLS_INFO Info == $r4)
    ppcDebugUnloadImageSymbols(ppuState, GPR(3), GPR(4));
    break;
  default:
    LOG_WARNING(Xenon, "Unimplemented trap! trapNumber = '0x{:X}'", trapNumber);
    break;
  }

  _ex |= PPU_EX_PROG;
  thread.exceptTrapType = TRAP_TYPE_SRR1_TRAP_TRAP;
  // Hacky optimization
  #undef curThread
  #define curThread ppuState->ppuThread[curThreadId]
}

// FP Unavailable (0x800)
void PPCInterpreter::ppcFPUnavailableException(PPU_STATE* ppuState) {
  PPU_THREAD_REGISTERS& thread = curThread;
  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): FPU exception.", ppuState->ppuName, static_cast<s8>(curThreadId));
  thread.SPR.SRR0 = thread.CIA;
  thread.SPR.SRR1 = thread.SPR.MSR.MSR_Hex & (QMASK(0, 32) | QMASK(37, 41) | QMASK(48, 63));
  thread.SPR.MSR.MSR_Hex = thread.SPR.MSR.MSR_Hex & ~(QMASK(48, 50) | QMASK(52, 55) | QMASK(58, 59) | QMASK(61, 63));
  thread.SPR.MSR.MSR_Hex = thread.SPR.MSR.MSR_Hex | QMASK(0, 0) | (QMASK(3, 3));
  thread.NIA = 0x800;
  thread.SPR.MSR.DR = 0;
  thread.SPR.MSR.IR = 0;
}

// VX Unavailable (0xF20)
void PPCInterpreter::ppcVXUnavailableException(PPU_STATE* ppuState) {
  PPU_THREAD_REGISTERS& thread = curThread;
  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): VXU exception.", ppuState->ppuName, static_cast<s8>(curThreadId));
  thread.SPR.SRR0 = thread.NIA;
  thread.SPR.SRR1 = thread.SPR.MSR.MSR_Hex & (QMASK(0, 32) | QMASK(37, 41) | QMASK(48, 63));
  thread.SPR.MSR.MSR_Hex = thread.SPR.MSR.MSR_Hex & ~(QMASK(48, 50) | QMASK(52, 55) | QMASK(58, 59) | QMASK(61, 63));
  thread.SPR.MSR.MSR_Hex = thread.SPR.MSR.MSR_Hex | QMASK(0, 0) | (QMASK(3, 3));
  thread.NIA = 0xF20;
  thread.SPR.MSR.DR = 0;
  thread.SPR.MSR.IR = 0;
}
