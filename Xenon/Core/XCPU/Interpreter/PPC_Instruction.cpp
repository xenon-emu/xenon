// Copyright 2025 Xenon Emulator Project. All rights reserved.

/*
* All original authors of the rpcs3 PPU_Decoder and PPU_Opcodes maintain their original copyright.
* Modifed for usage in the Xenon Emulator
* All rights reserved
* License: GPL2
*/

#include "Base/Global.h"
#include "Core/XCPU/Xenon.h"

#include "PPC_Instruction.h"
#include "PPCInterpreter.h"

namespace PPCInterpreter {
  void PPCInterpreter_nop(PPU_STATE *ppuState) {
    // Do nothing
  }
  void PPCInterpreter_invalid(PPU_STATE *ppuState) {
    if (Config::debug.haltOnInvalidInstructions) {
      XeMain::GetCPU()->Halt();
      Config::imgui.debugWindow = true; // Open debugger on bad fault
    }

    LOG_CRITICAL(Xenon, "PPC Interpreter: Invalid instruction found! Data: 0x{:X} (opcode, value[s]), address: 0x{:X}",
      _instr.opcode,
      curThread.CIA);
  }

  void PPCInterpreter_known_unimplemented(const char *name, PPU_STATE *ppuState) {
    LOG_CRITICAL(Xenon, "PPC Interpreter: {} is not implemented! Data: 0x{:X}, address: 0x{:X}",
      name,
      _instr.opcode,
      curThread.CIA);
  }

  PPCDecoder::PPCDecoder() {
    fillTables();
    fillNameTables();
  }

  constexpr std::pair<const char*, char> getBCInfo(u32 bo, u32 bi) {
    std::pair<const char*, char> info{};

    // handle bd(n)z(f|t) and bd(n)z
    switch (bo) {
      case 0b00000:
      case 0b00001: return { "bdnzf", 'f' };
      case 0b00010:
      case 0b00011: return { "bdzf", 'f' };
      case 0b01000:
      case 0b01001: return { "bdnzt", 't' };
      case 0b01010:
      case 0b01011: return { "bdzt", 't' };
      case 0b10010: return { "bdz", '\0' };
      case 0b11010: return { "bdz", '-' };
      case 0b11011: return { "bdz", '+' };
      case 0b10000: return { "bdnz", '\0' };
      case 0b11000: return { "bdnz", '-' };
      case 0b11001: return { "bdnz", '+' };
    }

    const bool isMinus = (bo & 0b11) == 0b10;
    const bool isPlus  = (bo & 0b11) == 0b11;

    if ((bo & 0b11100) == 0b00100) {
      // bge/ble/bne/bns
      if (isMinus) info.second = '-';
      else if (isPlus) info.second = '+';

      switch (bi % 4) {
        case 0x0: info.first = "bge"; break;
        case 0x1: info.first = "ble"; break;
        case 0x2: info.first = "bne"; break;
        case 0x3: info.first = "bns"; break;
      }
      return info;
    }

    if ((bo & 0b11100) == 0b01100) {
      // blt/bgt/beq/bso
      if (isMinus) info.second = '-';
      else if (isPlus) info.second = '+';

      switch (bi % 4) {
        case 0x0: info.first = "blt"; break;
        case 0x1: info.first = "bgt"; break;
        case 0x2: info.first = "beq"; break;
        case 0x3: info.first = "bso"; break;
      }
      return info;
    }

    return info; // default: empty pair
  }

  const std::string PPCInterpreter_getFullName(u32 instr) {
    if (instr == 0x60000000) {
      return "nop";
    }

    PPCOpcode op;
    op.opcode = instr;
    u32 decodedInstr = PPCDecode(instr);
    std::string baseName = ppcDecoder.getNameTable()[decodedInstr];

    switch (Base::JoaatStringHash(baseName)) {
    case "cmpi"_j: {
      return op.l10 ? "cmpdi" : "cmpwi";
    } break;
    case "addic"_j: {
      return op.main & 1 ? "addic." : "addic";
    } break;
    case "addi"_j: {
      return op.ra == 0 ? "li" : "addi";
    } break;
    case "addis"_j: {
      return op.ra == 0 ? "lis" : "addis";
    } break;
    case "bc"_j: {
      const u32 bo = op.bo;
      const u32 bi = op.bi;
      const s32 bd = op.ds * 4;
      const u32 aa = op.aa;
      const u32 lk = op.lk;

      const auto [inst, sign] = getBCInfo(bo, bi);
      if (!inst) {
        return "bc";
      }

      std::string finalInstr = inst;
      if (lk)
        finalInstr += 'l';
      if (aa)
        finalInstr += 'a';
      if (sign)
        finalInstr += sign;
      return finalInstr;
    } break;
    case "b"_j: {
      const u32 li = op.bt24;
      const u32 aa = op.aa;
      const u32 lk = op.lk;

      switch (lk) {
      case 0: {
        switch (aa) {
          case 0: return "b";
          case 1: return "ba";
        }
      } break;
      case 1: {
        switch (aa) {
          case 0: return "bl";
          case 1: return "bla";
        }
      } break;
      }
    } break;
    case "bclr"_j: {
      const u32 bo = op.bo;
      const u32 bi = op.bi;
      const u32 bh = op.bh;
      const u32 lk = op.lk;

      if (bo == 0b10100) {
        return lk ? "blrl" : "blr";
      }

      const auto [inst, sign] = getBCInfo(bo, bi);
      if (!inst) {
        return "bclr";
      }

      std::string finalInstr = std::string(inst) + (lk ? "lrl" : "lr");
      if (sign)
        finalInstr += sign;
      return finalInstr;
    } break;
    case "bcctr"_j: {
      const u32 bo = op.bo;
      const u32 bi = op.bi;
      const u32 bh = op.bh;
      const u32 lk = op.lk;

      if (bo == 0b10100) {
        return lk ? "bctrl" : "bctr";
      }

      const auto [inst, sign] = getBCInfo(bo, bi);
      if (!inst || inst[1] == 'd') {
        return "bcctr";
      }

      std::string finalInstr = std::string(inst) + (lk ? "lrl" : "lr");
      finalInstr += lk ? "ctrl" : "ctr";
      if (sign)
        finalInstr += sign;
      return finalInstr;
    } break;
    }

    return baseName;
  }
}
