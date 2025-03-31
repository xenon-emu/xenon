// Copyright 2025 Xenon Emulator Project

#include "Base/Logging/Log.h"

#include "PPCInterpreter.h"

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
