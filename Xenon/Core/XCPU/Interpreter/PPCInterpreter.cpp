/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Base/Logging/Log.h"
#include "Base/Global.h"
#include "Core/XCPU/XenonCPU.h"
#include "Core/XCPU/PPU/PPU.h"

#include "PPCInterpreter.h"

using namespace PPCInterpreter;

// Forward Declaration
Xe::XCPU::XenonContext* PPCInterpreter::xenonContext = nullptr;
PPCInterpreter::PPCDecoder PPCInterpreter::ppcDecoder{};

// Interpreter Single Instruction Processing.
void PPCInterpreter::ppcExecuteSingleInstruction(sPPEState *ppeState) {
  sPPUThread &thread = curThread;

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
  if (static_cast<u32>(thread.CIA) == 0x80081764) {
    return;
  }

  // XamSetPowerMode call to KeSetPowerMode bypass.
  if (static_cast<u32>(thread.CIA) == 0x817AC968) {
    return;
  }

  // XDK 17.489.0 AudioChipCorder Device Detect bypass. This is not needed for
  // older console revisions.
  if (static_cast<u32>(thread.CIA) == 0x801AF580) {
    return;
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

  // Pretend ARGON hardware is present, to avoid the call
  if (static_cast<u32>(thread.CIA) == 0x800819E0) {
    thread.GPR[11] |= 0x08; // Set bit 3 (ARGON present)
    LOG_INFO(SMC, "Faked XboxHardwareInfo bit 3 to skip HalNoteArgonErrors");
  }

  // Pretend ARGON hardware is present, to avoid the call
  if (static_cast<u32>(thread.CIA) == 0x80081A60) {
    thread.GPR[11] |= 0x08; // Set bit 3 (ARGON present)
    LOG_INFO(SMC, "Faked XboxHardwareInfo bit 3 to skip HalRecordArgonErrors");
  }

  // Skip bootanim (for now).
  if (static_cast<u32>(thread.CIA) == 0x80081EA4) {
    LOG_INFO(Xenon, "Skipping bootanim load.");
    thread.GPR[3] = 0;
  }

  if (static_cast<u32>(thread.CIA) == 0x800FC288) {
    LOG_INFO(Xenon, "VdRetrainEDRAM returning 0.");
    thread.GPR[3] = 0;
  }

  if (static_cast<u32>(thread.CIA) == 0x800F9130) {
    LOG_INFO(Xenon, "VdIsHSIOTrainingSucceeded returning 1.");
    thread.GPR[3] = 1;
  }

  // Skip media detection in XAM for now.
  if (static_cast<u32>(thread.CIA) == 0x8175E61C) {
    thread.GPR[3] = 0;
  }

  // This is just to set a PC breakpoint in any PPU/Thread.
  if (static_cast<u32>(thread.CIA) == 0x8011bc94) {
    LOG_DEBUG(Xenon, "Breakpoint HIT.");
  }

  // This is to set a PPU0[Thread0] breakpoint.
  if (thread.SPR.PIR == 0) {
    thread.lastRegValue = GPR(11);
  }

  instructionHandler function =
    ppcDecoder.decode(thread.CI.opcode);

  function(ppeState);
}

void PPCInterpreter::ppcInterpreterTrap(sPPEState *ppeState, u32 trapNumber) {
  sPPUThread &thread = curThread;
  // Hacky optimization
  #undef curThread
  #define curThread thread

  // DbgPrint, r3 = PCSTR stringAddress, r4 = int String Size.
  switch (trapNumber) {
  case 0x14:
  case 0x1A: {
    u32 strAddr = GPR(3);
    u64 strSize = static_cast<u64>(GPR(4));
    std::unique_ptr<u8[]> buffer = std::make_unique<STRIP_UNIQUE_ARR(buffer)>(strSize+1);
    MMURead(xenonContext, ppeState, strAddr, strSize, buffer.get());
    char *dbgString = reinterpret_cast<char*>(buffer.get());
    dbgString[strSize] = '\0'; // nul-term
    Base::Log::NoFmtMessage(Base::Log::Class::DebugPrint, Base::Log::Level::Guest, dbgString);
    break;
  }
  case 0x16: {
    if (Config::debug.softHaltOnAssertions && XeMain::GetCPU()) {
      LOG_XBOX(Xenon, "FATAL ERROR! Halting CPU...");
      PPU *PPU = XeMain::GetCPU()->GetPPU(ppeState->ppuID);
      if (PPU)
        PPU->Halt(0, true, ppeState->ppuID, curThreadId);
    }
  } break;
  case 0x17:
    // DebugLoadImageSymbols, type signature:
    // PUBLIC VOID DebugLoadImageSymbols(IN PSTRING ModuleName == $r3,
    //                   IN PKD_SYMBOLS_INFO Info == $r4)
    ppcDebugLoadImageSymbols(ppeState, GPR(3), GPR(4));
    break;
  case 0x19: {
    if (Config::debug.softHaltOnAssertions) {
#ifndef NO_GFX
      if (XeMain::GetCPU()) {
        LOG_XBOX(Xenon, "Assertion! Halting CPU... (Continuing will cause execution to resume as normal)");
        PPU *PPU = XeMain::GetCPU()->GetPPU(ppeState->ppuID);
        if (PPU)
          PPU->Halt(0, true, ppeState->ppuID, curThreadId);
      }
#else
      LOG_XBOX(Xenon, "Assertion! Continuing...");
#endif
      thread.progExceptionType = PROGRAM_EXCEPTION_TYPE_TRAP;
      return;
    } else if (Config::debug.autoContinueOnGuestAssertion) {
      LOG_XBOX(Xenon, "Assertion! Automatically continuing execution...");
      thread.progExceptionType = PROGRAM_EXCEPTION_TYPE_TRAP;
      return;
    } else {
      LOG_XBOX(Xenon, "Assertion!");
    }
    break;
  }
  case 0x18:
    // DebugUnloadImageSymbols, type signature:
    // PUBLIC VOID DebugUnloadImageSymbols(IN PSTRING ModuleName == $r3,
    //                   IN PKD_SYMBOLS_INFO Info == $r4)
    ppcDebugUnloadImageSymbols(ppeState, GPR(3), GPR(4));
    break;
  default:
    LOG_WARNING(Xenon, "Unimplemented trap! trapNumber = '0x{:X}'", trapNumber);
    break;
  }

  _ex |= PPU_EX_PROG;
  thread.progExceptionType = PROGRAM_EXCEPTION_TYPE_TRAP;
  // Hacky optimization
  #undef curThread
  #define curThread ppeState->ppuThread[curThreadId]
}
