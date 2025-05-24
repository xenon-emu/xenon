// Copyright 2025 Xenon Emulator Project

#include "JITEmitter_Helpers.h"

void PPCInterpreter::PPCInterpreterJIT_mfspr(PPU_STATE *ppuState, JITBlockBuilder *b, PPCOpcode instr) {
  u64 rS = 0, crm = 0;
  PPC_OPC_TEMPL_XFX(instr.opcode, rS, crm);
  u32 sprNum = instr.spr;
  sprNum = ((sprNum & 0x1F) << 5) | ((sprNum >> 5) & 0x1F);
  u64 value = 0;

  x86::Gp rSValue = newGP64();

  switch (sprNum) {
  case SPR_XER:
    COMP->mov(rSValue, SPRPtr(XER).Base());
    break;
  case SPR_LR:
    COMP->mov(rSValue, SPRPtr(LR));
    break;
  case SPR_CTR:
    COMP->mov(rSValue, SPRPtr(CTR));
    break;
  case SPR_DSISR:
    COMP->mov(rSValue, SPRPtr(DSISR));
    break;
  case SPR_DAR:
    COMP->mov(rSValue, SPRPtr(DAR));
    break;
  case SPR_DEC:
    COMP->mov(rSValue, SPRPtr(DEC));
    break;
  case SPR_SDR1:
    COMP->mov(rSValue, SharedSPRPtr(SDR1));
    break;
  case SPR_SRR0:
    COMP->mov(rSValue, SPRPtr(SRR0));
    break;
  case SPR_SRR1:
    COMP->mov(rSValue, SPRPtr(SRR1));
    break;
  case SPR_CFAR:
    COMP->mov(rSValue, SPRPtr(CFAR));
    break;
  case SPR_CTRLRD:
    COMP->mov(rSValue, SharedSPRPtr(CTRL));
    break;
  case SPR_VRSAVE:
    COMP->mov(rSValue, SPRPtr(VRSAVE));
    break;
  case SPR_TBL_RO:
    COMP->mov(rSValue, SharedSPRPtr(TB));
    break;
  case SPR_TBU_RO:
    COMP->mov(rSValue, SharedSPRPtr(TB));
    COMP->and_(rSValue, 0xFFFFFFFF00000000);
    break;
  case SPR_SPRG0:
    COMP->mov(rSValue, SPRPtr(SPRG0));
    break;
  case SPR_SPRG1:
    COMP->mov(rSValue, SPRPtr(SPRG1));
    break;
  case SPR_SPRG2:
    COMP->mov(rSValue, SPRPtr(SPRG2));
    break;
  case SPR_SPRG3:
    COMP->mov(rSValue, SPRPtr(SPRG3));
    break;
  case SPR_TB:
    COMP->mov(rSValue, SharedSPRPtr(TB));
    break;
  case SPR_PVR:
    COMP->mov(rSValue, SharedSPRPtr(PVR.PVR_Hex));
    break;
  case SPR_HSPRG0:
    COMP->mov(rSValue, SPRPtr(HSPRG0));
    break;
  case SPR_HSPRG1:
    COMP->mov(rSValue, SPRPtr(HSPRG1));
    break;
  case SPR_RMOR:
    COMP->mov(rSValue, SharedSPRPtr(RMOR));
    break;
  case SPR_HRMOR:
    COMP->mov(rSValue, SharedSPRPtr(HRMOR));
    break;
  case SPR_LPCR:
    COMP->mov(rSValue, SharedSPRPtr(LPCR));
    break;
  case SPR_TSCR:
    COMP->mov(rSValue, SharedSPRPtr(TSCR));
    break;
  case SPR_TTR:
    COMP->mov(rSValue, SharedSPRPtr(TTR));
    break;
  case SPR_PpeTlbIndexHint:
    COMP->mov(rSValue, SPRPtr(PPE_TLB_Index_Hint));
    break;
  case SPR_HID0:
    COMP->mov(rSValue, SharedSPRPtr(HID0));
    break;
  case SPR_HID1:
    COMP->mov(rSValue, SharedSPRPtr(HID1));
    break;
  case SPR_HID4:
    COMP->mov(rSValue, SharedSPRPtr(HID4));
    break;
  case SPR_DABR:
    COMP->mov(rSValue, SPRPtr(DABR));
    break;
  case SPR_HID6:
    COMP->mov(rSValue, SharedSPRPtr(HID6));
    break;
  case SPR_PIR:
    COMP->mov(rSValue, SPRPtr(PIR));
    break;
  default:
    LOG_ERROR(Xenon, "{}(Thrd{:#d}) mfspr: Unknown SPR: {:#x}", ppuState->ppuName, static_cast<u8>(curThreadId), sprNum);
    break;
  }

  COMP->mov(GPRPtr(rS), rSValue);
}