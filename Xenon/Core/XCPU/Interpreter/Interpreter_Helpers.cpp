// Copyright 2025 Xenon Emulator Project

#include "Base/Logging/Log.h"

#include "PPCInterpreter.h"

void PPCInterpreter::ppcUpdateCR(PPU_STATE &ppuState, s8 crNum, u32 crValue) {
  switch (crNum) {
  case 0:
    ppuState.ppuThread[ppuState.currentThread].CR.CR0 = crValue;
    break;
  case 1:
    ppuState.ppuThread[ppuState.currentThread].CR.CR1 = crValue;
    break;
  case 2:
    ppuState.ppuThread[ppuState.currentThread].CR.CR2 = crValue;
    break;
  case 3:
    ppuState.ppuThread[ppuState.currentThread].CR.CR3 = crValue;
    break;
  case 4:
    ppuState.ppuThread[ppuState.currentThread].CR.CR4 = crValue;
    break;
  case 5:
    ppuState.ppuThread[ppuState.currentThread].CR.CR5 = crValue;
    break;
  case 6:
    ppuState.ppuThread[ppuState.currentThread].CR.CR6 = crValue;
    break;
  case 7:
    ppuState.ppuThread[ppuState.currentThread].CR.CR7 = crValue;
    break;
  }
}

u32 PPCInterpreter::CRCompU(PPU_STATE &ppuState, u64 num1, u64 num2) {
  u32 CR = 0;

  if (num1 < num2)
    BSET(CR, 4, CR_BIT_LT);
  else if (num1 > num2)
    BSET(CR, 4, CR_BIT_GT);
  else
    BSET(CR, 4, CR_BIT_EQ);

  if (ppuState.ppuThread[ppuState.currentThread].SPR.XER.SO)
    BSET(CR, 4, CR_BIT_SO);

  return CR;
}

u32 PPCInterpreter::CRCompS32(PPU_STATE &ppuState, u32 num1, u32 num2) {
  u32 CR = 0;

  if ((long)num1 < (long)num2)
    BSET(CR, 4, CR_BIT_LT);
  else if ((long)num1 > (long)num2)
    BSET(CR, 4, CR_BIT_GT);
  else
    BSET(CR, 4, CR_BIT_EQ);

  if (ppuState.ppuThread[ppuState.currentThread].SPR.XER.SO)
    BSET(CR, 4, CR_BIT_SO);

  return CR;
}

u32 PPCInterpreter::CRCompS64(PPU_STATE &ppuState, u64 num1, u64 num2) {
  u32 CR = 0;

  if ((s64)num1 < (s64)num2)
    BSET(CR, 4, CR_BIT_LT);
  else if ((s64)num1 > (s64)num2)
    BSET(CR, 4, CR_BIT_GT);
  else
    BSET(CR, 4, CR_BIT_EQ);

  if (ppuState.ppuThread[ppuState.currentThread].SPR.XER.SO)
    BSET(CR, 4, CR_BIT_SO);

  return CR;
}

u32 PPCInterpreter::CRCompS(PPU_STATE &ppuState, u64 num1, u64 num2) {
  if (ppuState.ppuThread[ppuState.currentThread].SPR.MSR.SF)
    return (CRCompS64(ppuState, num1, num2));
  else
    return (CRCompS32(ppuState, (u32)num1, (u32)num2));
}

void PPCInterpreter::ppcDebugLoadImageSymbols(PPU_STATE &ppuState,
                                              u64 moduleNameAddress,
                                              u64 moduleInfoAddress) {
  // Loaded module name.
  char moduleName[128]{};
  // Loaded module info.
  KD_SYMBOLS_INFO Kdinfo{};

  mmuReadString(ppuState, moduleNameAddress, moduleName, 128);
  u8 a = 0;
  Kdinfo.BaseOfDll = MMURead32(ppuState, moduleInfoAddress);
  Kdinfo.ProcessId = MMURead32(ppuState, moduleInfoAddress + 4);
  Kdinfo.CheckSum = MMURead32(ppuState, moduleInfoAddress + 8);
  Kdinfo.SizeOfImage = MMURead32(ppuState, moduleInfoAddress + 12);

  LOG_XBOX(Xenon, "[{}]: *** DebugLoadImageSymbols ***", ppuState.ppuName);
  LOG_XBOX(Xenon, "Loaded: {} at address {:#x} - {:#x}", moduleName, Kdinfo.BaseOfDll, (Kdinfo.BaseOfDll + Kdinfo.SizeOfImage));
}

void PPCInterpreter::ppcDebugUnloadImageSymbols(PPU_STATE &ppuState,
                                                u64 moduleNameAddress,
                                                u64 moduleInfoAddress) {}
