// Copyright 2025 Xenon Emulator Project

#include "Base/Logging/Log.h"

#include "PPCInterpreter.h"

u32 PPCInterpreter::CRCompU(PPU_STATE *ppuState, u64 num1, u64 num2) {
  u32 CR = 0;

  if (num1 < num2)
    BSET(CR, 4, CR_BIT_LT);
  else if (num1 > num2)
    BSET(CR, 4, CR_BIT_GT);
  else
    BSET(CR, 4, CR_BIT_EQ);

  if (curThread.SPR.XER.SO)
    BSET(CR, 4, CR_BIT_SO);

  return CR;
}

u32 PPCInterpreter::CRCompS32(PPU_STATE *ppuState, u32 num1, u32 num2) {
  u32 CR = 0;

  if ((long)num1 < (long)num2)
    BSET(CR, 4, CR_BIT_LT);
  else if ((long)num1 > (long)num2)
    BSET(CR, 4, CR_BIT_GT);
  else
    BSET(CR, 4, CR_BIT_EQ);

  if (curThread.SPR.XER.SO)
    BSET(CR, 4, CR_BIT_SO);

  return CR;
}

u32 PPCInterpreter::CRCompS64(PPU_STATE *ppuState, u64 num1, u64 num2) {
  u32 CR = 0;

  if (static_cast<s64>(num1) < static_cast<s64>(num2))
    BSET(CR, 4, CR_BIT_LT);
  else if (static_cast<s64>(num1) > static_cast<s64>(num2))
    BSET(CR, 4, CR_BIT_GT);
  else
    BSET(CR, 4, CR_BIT_EQ);

  if (curThread.SPR.XER.SO)
    BSET(CR, 4, CR_BIT_SO);

  return CR;
}

u32 PPCInterpreter::CRCompS(PPU_STATE *ppuState, u64 num1, u64 num2) {
  if (curThread.SPR.MSR.SF)
    return (CRCompS64(ppuState, num1, num2));
  else
    return (CRCompS32(ppuState, static_cast<u32>(num1), static_cast<u32>(num2)));
}

void PPCInterpreter::ppcDebugLoadImageSymbols(PPU_STATE *ppuState,
                                              u64 moduleNameAddress,
                                              u64 moduleInfoAddress) {
  // Loaded module name.
  char moduleName[128];
  // Loaded module info.
  KD_SYMBOLS_INFO Kdinfo;

  mmuReadString(ppuState, moduleNameAddress, moduleName, 128);
  u8 a = 0;
  Kdinfo.BaseOfDll = MMURead32(ppuState, moduleInfoAddress);
  Kdinfo.ProcessId = MMURead32(ppuState, moduleInfoAddress + 4);
  Kdinfo.CheckSum = MMURead32(ppuState, moduleInfoAddress + 8);
  Kdinfo.SizeOfImage = MMURead32(ppuState, moduleInfoAddress + 12);

  LOG_XBOX(Xenon, "[{}]: *** DebugLoadImageSymbols ***", ppuState->ppuName);
  LOG_XBOX(Xenon, "Loaded: {} at address {:#x} - {:#x}", moduleName, Kdinfo.BaseOfDll, (Kdinfo.BaseOfDll + Kdinfo.SizeOfImage));
}

void PPCInterpreter::ppcDebugUnloadImageSymbols(PPU_STATE *ppuState,
                                                u64 moduleNameAddress,
                                                u64 moduleInfoAddress)
{}
