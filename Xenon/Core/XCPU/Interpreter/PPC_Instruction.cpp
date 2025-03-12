/*
* Copyright 2025 Xenon Emulator Project

* All original authors of the rpcs3 PPU_Decoder and PPU_Opcodes maintain their original copyright.
* Modifed for usage in the Xenon Emulator
* All rights reserved
* License: GPL2
*/

#include "PPC_Instruction.h"

#include "PPCInterpreter.h"

namespace PPCInterpreter {
  void PPCInterpreter_nop(PPU_STATE *ppuState) {
    // Do nothing
  }
  void PPCInterpreter_invalid(PPU_STATE *ppuState) {

    LOG_CRITICAL(Xenon, "PPC Interpreter: Invalid instruction found! Data: {:#x} (opcode, value[s]), address: {:#x}",
      _instr.opcode,
      curThread.CIA);
  }

  void PPCInterpreter_known_unimplemented(const char *name, PPU_STATE *ppuState) {
    LOG_CRITICAL(Xenon, "PPC Interpreter: {} is not implemented! Data: {:#x}, address: {:#x}",
      name,
      _instr.opcode,
      curThread.CIA);
  }

  PPCDecoder::PPCDecoder() {
    fillTables();
    fillNameTables();
  }
}
