/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include <unordered_map>

#include "Base/Logging/Log.h"

#include "PPCInterpreter.h"

//#define SYSCALL_DEBUG

#ifdef SYSCALL_DEBUG
static const std::unordered_map<u8, const std::string> syscallIndexMap17489 = {
  { 0, "HvxGetVersion" },
  { 1, "HvxStartupProcessors" },
  { 2, "HvxQuiesceProcessor" },
  { 3, "HvxFlushEntireTb" },
  { 4, "HvxFlushSingleTb" },
  { 5, "HvxRelocateAndFlush" },
  { 6, "HvxGetSpecialPurposeRegister" },
  { 7, "HvxSetSpecialPurposeRegister" },
  { 8, "HvxGetSocRegister" },
  { 9, "HvxSetSocRegister" },
  { 10, "HvxSetTimebaseToZero" },
  { 11, "HvxZeroPage" },
  { 12, "HvxFlushDcacheRange" },
  { 13, "HvxPostOutput" },
  { 14, "HvxEnablePPUPerformanceMonitor" },
  { 15, "HvxGetImagePageTableEntry" },
  { 16, "HvxSetImagePageTableEntry" },
  { 17, "HvxCreateImageMapping" },
  { 18, "HvxMapImagePage" },
  { 19, "HvxCompleteImageMapping" },
  { 20, "HvxLoadImageData" },
  { 21, "HvxFinishImageDataLoad" },
  { 22, "HvxStartResolveImports" },
  { 23, "HvxResolveImports" },
  { 24, "HvxFinishImageLoad" },
  { 25, "HvxAbandonImageLoad" },
  { 26, "HvxUnmapImagePages" },
  { 27, "HvxUnmapImage" },
  { 28, "HvxUnmapImageRange" },
  { 29, "HvxCreateUserMode" },
  { 30, "HvxDeleteUserMode" },
  { 31, "HvxFlushUserModeTb" },
  { 32, "HvxSetPowerMode" },
  { 33, "HvxShadowBoot" },
  { 34, "HvxBlowFuses" },
  { 35, "HvxFsbInterrupt" },
  { 36, "HvxLockL2" },
  { 37, "HvxDvdAuthBuildNVPage" },
  { 38, "HvxDvdAuthVerifyNVPage" },
  { 39, "HvxDvdAuthRecordAuthenticationPage" },
  { 40, "HvxDvdAuthRecordXControl" },
  { 41, "HvxDvdAuthGetAuthPage" },
  { 42, "HvxDvdAuthVerifyAuthPage" },
  { 43, "HvxDvdAuthGetNextLBAIndex" },
  { 44, "HvxDvdAuthVerifyLBA" },
  { 45, "HvxDvdAuthClearDiscAuthInfo" },
  { 46, "HvxKeysInitialize" },
  { 47, "HvxKeysGetKeyProperties" },
  { 48, "HvxKeysGetStatus" },
  { 49, "HvxKeysGenerateRandomKey" },
  { 50, "HvxKeysGetFactoryChallenge" },
  { 51, "HvxKeysSetFactoryResponse" },
  { 52, "HvxKeysSaveBootLoader" },
  { 53, "HvxKeysSaveKeyVault" },
  { 54, "HvxKeysSetKey" },
  { 55, "HvxKeysGetKey" },
  { 56, "HvxKeysGetDigest" },
  { 57, "HvxKeysRsaPrvCrypt" },
  { 58, "HvxKeysHmacSha" },
  { 59, "HvxKeysAesCbc" },
  { 60, "HvxKeysDes2Cbc" },
  { 61, "HvxKeysDesCbc" },
  { 62, "HvxKeysObscureKey" },
  { 63, "HvxKeysSaveSystemUpdate" },
  { 64, "HvxKeysExecute" },
  { 65, "HvxDvdAuthTestMode" },
  { 66, "HvxEnableTimebase" },
  { 67, "HvxHdcpCalculateMi" },
  { 68, "HvxHdcpCalculateAKsvSignature" },
  { 69, "HvxHdcpCalculateBKsvSignature" },
  { 70, "HvxSetRevocationList" },
  { 71, "HvxEncryptedAllocationReserve" },
  { 72, "HvxEncryptedAllocationMap" },
  { 73, "HvxEncryptedAllocationUnmap" },
  { 74, "HvxEncryptedAllocationRelease" },
  { 75, "HvxEncryptedSweepAddressRange" },
  { 76, "HvxKeysExCreateKeyVault" },
  { 77, "HvxKeysExLoadKeyVault" },
  { 78, "HvxKeysExSaveKeyVault" },
  { 79, "HvxKeysExSetKey" },
  { 80, "HvxKeysExGetKey" },
  { 81, "HvxGetUpdateSequence" },
  { 82, "HvxSecurityInitialize" },
  { 83, "HvxSecurityLoadSettings" },
  { 84, "HvxSecuritySaveSettings" },
  { 85, "HvxSecuritySetDetected" },
  { 86, "HvxSecurityGetDetected" },
  { 87, "HvxSecuritySetActivated" },
  { 88, "HvxSecurityGetActivated" },
  { 89, "HvxSecuritySetStat" },
  { 90, "HvxGetProtectedFlags" },
  { 91, "HvxSetProtectedFlag" },
  { 92, "HvxDvdAuthGetAuthResults" },
  { 93, "HvxDvdAuthSetDriveAuthResult" },
  { 94, "HvxDvdAuthSetDiscAuthResult" },
  { 95, "HvxImageTransformImageKey" },
  { 96, "HvxImageXexHeader" },
  { 97, "HvxRevokeLoad" },
  { 98, "HvxRevokeSave" },
  { 99, "HvxRevokeUpdate" },
  { 100, "HvxDvdAuthGetMediaId" },
  { 101, "HvxXexActivationGetNonce" },
  { 102, "HvxXexActivationSetLicense" },
  { 103, "HvxXexActivationVerifyOwnership" },
  { 104, "HvxIptvSetBoundaryKey" },
  { 105, "HvxIptvSetSessionKey" },
  { 106, "HvxIptvVerifyOmac1Signature" },
  { 107, "HvxIptvGetAesCtrTransform" },
  { 108, "HvxIptvGetSessionKeyHash" },
  { 109, "HvxImageDvdEmulationMode" },
  { 110, "HvxImageUserMode" },
  { 111, "HvxImageShim" },
  { 112, "HvxExpansionInstall" },
  { 113, "HvxExpansionCall" },
  { 114, "HvxDvdAuthFwcr" },
  { 115, "HvxDvdAuthFcrt" },
  { 116, "HvxDvdAuthEx" },
  { 117, "HvxTest" },
};

// Returns the register name as an std::string.
static std::string getSyscallNameFromIndex17489(u32 syscallID) {
  auto it = syscallIndexMap17489.find(syscallID);
  if (it != syscallIndexMap17489.end()) {
    return it->second;
  } else { return "Unknown";}
}
#endif

// Instruction Synchronize
void PPCInterpreter::PPCInterpreter_isync(sPPEState *ppeState) {
  // Do nothing
}

// Enforce In-order Execution of I/O
void PPCInterpreter::PPCInterpreter_eieio(sPPEState *ppeState) {
  // Do nothing
}

// System Call
void PPCInterpreter::PPCInterpreter_sc(sPPEState *ppeState) {
  // Raise the exception.
  _ex |= ppuSystemCallEx;
  curThread.exHVSysCall = _instr.lev & 1;
#ifdef SYSCALL_DEBUG
  if (curThread.GPR[0] != 0) { // Don't want to ouput every time that HvxGetVersions is called.
    LOG_DEBUG(Xenon, "{}(Thread{:#d}): Issuing Syscall {}", ppeState->ppuName, (u8)ppeState->currentThread,
      getSyscallNameFromIndex17489(curThread.GPR[0]));
  }
#endif // SYSCALL_DEBUG

}

// SLB Move To Entry
void PPCInterpreter::PPCInterpreter_slbmte(sPPEState *ppeState) {
  u64 VSID = QGET(GPRi(rs), 0, 51);

  const u8 Ks = QGET(GPRi(rs), 52, 52);
  const u8 Kp = QGET(GPRi(rs), 53, 53);
  const u8 N = QGET(GPRi(rs), 54, 54);
  const u8 L = QGET(GPRi(rs), 55, 55);
  const u8 C = QGET(GPRi(rs), 56, 56);
  const u8 LP = QGET(GPRi(rs), 57, 59);

  const u64 ESID = QGET(GPRi(rb), 0, 35);
  bool V = QGET(GPRi(rb), 36, 36);
  const u16 Index = QGET(GPRi(rb), 52, 63);

  // VSID is VA 0-52 bit, the remaining 28 bits are adress data
  // so whe shift 28 bits left here so we only do it once per entry.
  // This speeds MMU translation since the shift is only done once.
  VSID = VSID << 28;

  curThread.SLB[Index].ESID = ESID;
  curThread.SLB[Index].VSID = VSID;
  curThread.SLB[Index].V = V;
  curThread.SLB[Index].Kp = Kp;
  curThread.SLB[Index].Ks = Ks;
  curThread.SLB[Index].N = N;
  curThread.SLB[Index].L = L;
  curThread.SLB[Index].C = C;
  curThread.SLB[Index].LP = LP;
  curThread.SLB[Index].vsidReg = GPRi(rs);
  curThread.SLB[Index].esidReg = GPRi(rb);
}

// SLB Invalidate Entry
void PPCInterpreter::PPCInterpreter_slbie(sPPEState *ppeState) {
  // ESID
  const u64 ESID = QGET(GPRi(rb), 0, 35);
  // Class
  const u8 C = QGET(GPRi(rb), 36, 36);

  for (auto &slbEntry : curThread.SLB) {
    if (slbEntry.V && slbEntry.C == C && slbEntry.ESID == ESID) {
      slbEntry.V = false;
    }
  }

  // Should only invalidate entries for a specific set of addresses.
  // Invalidate both ERAT's *** BUG *** !!!
  curThread.iERAT.invalidateAll();
  curThread.dERAT.invalidateAll();
}

// Return From Interrupt Doubleword
void PPCInterpreter::PPCInterpreter_rfid(sPPEState *ppeState) {

  // Compose new MSR as per specs
  const u64 srr1 = curThread.SPR.SRR1;
  u64 new_msr = 0;

  const u32 usr = BGET(srr1, 64, 49);
  if (usr) {
    //(("WARNING!! Cannot really do Problem mode"));
    BSET(new_msr, 64, 49);
  }

  // MSR.0 = SRR1.0 | SRR1.1
  const u32 b = BGET(srr1, 64, 0) || BGET(srr1, 64, 1);
  if (b) {
    BSET(new_msr, 64, 0);
  }

  const u32 b3 = BGET(curThread.SPR.MSR.hexValue, 64, 3);

  // MSR.51 = (MSR.3 & SRR1.51) | ((~MSR.3) & MSR.51)
  if ((b3 && BGET(srr1, 64, 51)) || (!b3 && BGET(curThread.SPR.MSR.hexValue, 64, 51))) {
    BSET(new_msr, 64, 51);
  }

  // MSR.3 = MSR.3 & SRR1.3
  if (b3 && BGET(srr1, 64, 3)) {
    BSET(new_msr, 64, 3);
  }

  // MSR.48 = SRR1.48
  if (BGET(srr1, 64, 48)) {
    BSET(new_msr, 64, 48);
  }

  // MSR.58 = SRR1.58 | SRR1.49
  if (usr || BGET(srr1, 64, 58)) {
    BSET(new_msr, 64, 58);
  }

  // MSR.59 = SRR1.59 | SRR1.49
  if (usr || BGET(srr1, 64, 59)) {
    BSET(new_msr, 64, 59);
  }

  // MSR.1:2,4:32,37:41,49:50,52:57,60:63 = SRR1.<same>
  new_msr = new_msr | (srr1 & (QMASK(1, 2) | QMASK(4, 32) | QMASK(37, 41) |
                               QMASK(49, 50) | QMASK(52, 57) | QMASK(60, 63)));

  // See what changed and take actions
  // NB: we ignore a bunch of bits..
  const u64 diff_msr = curThread.SPR.MSR.hexValue ^ new_msr;

  // NB: we dont do half-modes
  if (diff_msr & QMASK(58, 59)) {
    curThread.SPR.MSR.IR = usr != 0 || (new_msr & QMASK(58, 59)) != 0;
    curThread.SPR.MSR.DR = usr != 0 || (new_msr & QMASK(58, 59)) != 0;
  }

  curThread.SPR.MSR.hexValue = new_msr;
  curThread.NIA = curThread.SPR.SRR0 & ~3;

  // Check for 32-bit mode of operation.
  if (!curThread.SPR.MSR.SF)
    curThread.NIA = static_cast<u32>(curThread.NIA);
}

// Trap Word
void PPCInterpreter::PPCInterpreter_tw(sPPEState *ppeState) {
  const sl32 a = static_cast<sl32>(GPRi(ra));
  const sl32 b = static_cast<sl32>(GPRi(rb));

  if ((a < b && (_instr.bo & 0x10)) ||
      (a > b && (_instr.bo & 0x8)) ||
      (a == b && (_instr.bo & 0x4)) ||
      (static_cast<u32>(a) < static_cast<u32>(b) && (_instr.bo & 0x2)) ||
      (static_cast<u32>(a) > static_cast<u32>(b) && (_instr.bo & 0x1))) {
    ppcInterpreterTrap(ppeState, b);
  }
}

// Trap Word Immediate
void PPCInterpreter::PPCInterpreter_twi(sPPEState *ppeState) {
  const s64 sA = GPRi(ra), sB = _instr.simm16;
  const u64 a = sA, b = sB;

  if (((_instr.bo & 0x10) && sA < sB) ||
      ((_instr.bo & 0x8) && sA > sB) ||
      ((_instr.bo & 0x4) && sA == sB) ||
      ((_instr.bo & 0x2) && a < b) ||
      ((_instr.bo & 0x1) && a > b)) {
    ppcInterpreterTrap(ppeState, static_cast<u32>(sB));
  }
}

// Trap Doubleword
void PPCInterpreter::PPCInterpreter_td(sPPEState *ppeState) {
  const s64 sA = GPRi(ra), sB = GPRi(rb);
  const u64 a = sA, b = sB;

  if (((_instr.bo & 0x10) && sA < sB) ||
      ((_instr.bo & 0x8) && sA > sB) ||
      ((_instr.bo & 0x4) && sA == sB) ||
      ((_instr.bo & 0x2) && a < b) ||
      ((_instr.bo & 0x1) && a > b)) {
    ppcInterpreterTrap(ppeState, static_cast<u32>(sB));
  }
}

// Trap Doubleword Immediate
void PPCInterpreter::PPCInterpreter_tdi(sPPEState *ppeState) {
  const s64 sA = GPRi(ra), sB = _instr.simm16;
  const u64 a = sA, b = sB;

  if (((_instr.bo & 0x10) && sA < sB) ||
      ((_instr.bo & 0x8) && sA > sB) ||
      ((_instr.bo & 0x4) && sA == sB) ||
      ((_instr.bo & 0x2) && a < b) ||
      ((_instr.bo & 0x1) && a > b)) {
    ppcInterpreterTrap(ppeState, static_cast<u32>(sB));
  }
}

// Move From Special Purpose Register
void PPCInterpreter::PPCInterpreter_mfspr(sPPEState *ppeState) {
  u32 spr = _instr.spr;
  spr = ((spr & 0x1F) << 5) | ((spr >> 5) & 0x1F);

  switch (static_cast<eXenonSPR>(spr)) {
  case eXenonSPR::XER:
    GPRi(rs) = curThread.SPR.XER.hexValue;
    break;
  case eXenonSPR::LR:
    GPRi(rs) = curThread.SPR.LR;
    break;
  case eXenonSPR::CTR:
    GPRi(rs) = curThread.SPR.CTR;
    break;
  case eXenonSPR::DSISR:
    GPRi(rs) = curThread.SPR.DSISR;
    break;
  case eXenonSPR::DAR:
    GPRi(rs) = curThread.SPR.DAR;
    break;
  case eXenonSPR::DEC:
    GPRi(rs) = curThread.SPR.DEC;
    break;
  case eXenonSPR::HDEC:
    GPRi(rs) = ppeState->SPR.HDEC;
    break;
  case eXenonSPR::SDR1:
    GPRi(rs) = ppeState->SPR.SDR1.hexValue;
    break;
  case eXenonSPR::SRR0:
    GPRi(rs) = curThread.SPR.SRR0;
    break;
  case eXenonSPR::SRR1:
    GPRi(rs) = curThread.SPR.SRR1;
    break;
  case eXenonSPR::CFAR:
    GPRi(rs) = curThread.SPR.CFAR;
    break;
  case eXenonSPR::CTRLRD:
    GPRi(rs) = ppeState->SPR.CTRL.hexValue;
    break;
  case eXenonSPR::VRSAVE:
    GPRi(rs) = curThread.SPR.VRSAVE;
    break;
  case eXenonSPR::TBLRO:
    GPRi(rs) = ppeState->SPR.TB.TBL;
    break;
  case eXenonSPR::TBURO:
    GPRi(rs) = ppeState->SPR.TB.TBU;
    break;
  case eXenonSPR::SPRG0:
    GPRi(rs) = curThread.SPR.SPRG0;
    break;
  case eXenonSPR::SPRG1:
    GPRi(rs) = curThread.SPR.SPRG1;
    break;
  case eXenonSPR::SPRG2:
    GPRi(rs) = curThread.SPR.SPRG2;
    break;
  case eXenonSPR::SPRG3RD:
  case eXenonSPR::SPRG3:
    GPRi(rs) = curThread.SPR.SPRG3;
    break;
  case eXenonSPR::PVR:
    GPRi(rs) = ppeState->SPR.PVR.hexValue;
    break;
  case eXenonSPR::HSPRG0:
    GPRi(rs) = curThread.SPR.HSPRG0;
    break;
  case eXenonSPR::HSPRG1:
    GPRi(rs) = curThread.SPR.HSPRG1;
    break;
  case eXenonSPR::RMOR:
    GPRi(rs) = ppeState->SPR.RMOR.hexValue;
    break;
  case eXenonSPR::HRMOR:
    GPRi(rs) = ppeState->SPR.HRMOR.hexValue;
    break;
  case eXenonSPR::LPCR:
    GPRi(rs) = ppeState->SPR.LPCR.hexValue;
    break;
  case eXenonSPR::TSCR:
    GPRi(rs) = ppeState->SPR.TSCR.hexValue;
    break;
  case eXenonSPR::TTR:
    GPRi(rs) = ppeState->SPR.TTR.hexValue;
    break;
  case eXenonSPR::PPE_TLB_Index_Hint:
    GPRi(rs) = curThread.SPR.PPE_TLB_Index_Hint.hexValue;
    break;
  case eXenonSPR::HID0:
    GPRi(rs) = ppeState->SPR.HID0.hexValue;
    break;
  case eXenonSPR::HID1:
    GPRi(rs) = ppeState->SPR.HID1.hexValue;
    break;
  case eXenonSPR::HID4:
    GPRi(rs) = ppeState->SPR.HID4.hexValue;
    break;
  case eXenonSPR::HID6:
    GPRi(rs) = ppeState->SPR.HID6.hexValue;
    break;
  case eXenonSPR::DABR:
    GPRi(rs) = curThread.SPR.DABR.hexValue;
    break;
  case eXenonSPR::PIR:
    GPRi(rs) = curThread.SPR.PIR;
    break;
  default:
    LOG_ERROR(Xenon, "{}(Thrd{:#d}) mfspr: Unknown SPR: 0x{:X}", ppeState->ppuName, static_cast<u8>(curThreadId), spr);
    break;
  }
}

// Move To Special Purpose Register
void PPCInterpreter::PPCInterpreter_mtspr(sPPEState *ppeState) {
  u32 spr = _instr.spr;
  spr = ((spr & 0x1F) << 5) | ((spr >> 5) & 0x1F);

  switch (static_cast<eXenonSPR>(spr)) {
  case eXenonSPR::XER:
    curThread.SPR.XER.hexValue = static_cast<u32>(GPRi(rd));
    // Clear the unused bits in XER (35:56)
    curThread.SPR.XER.hexValue &= 0xE000007F;
    break;
  case eXenonSPR::LR:
    curThread.SPR.LR = GPRi(rd);
    break;
  case eXenonSPR::CTR:
    curThread.SPR.CTR = GPRi(rd);
    break;
  case eXenonSPR::DSISR:
    curThread.SPR.DSISR = GPRi(rd);
    break;
  case eXenonSPR::DAR:
    curThread.SPR.DAR = GPRi(rd);
    break;
  case eXenonSPR::DEC:
    curThread.SPR.DEC = static_cast<u32>(GPRi(rd));
    break;
  case eXenonSPR::SDR1:
    ppeState->SPR.SDR1.hexValue = GPRi(rd);
    break;
  case eXenonSPR::SRR0:
    curThread.SPR.SRR0 = GPRi(rd);
    break;
  case eXenonSPR::SRR1:
    curThread.SPR.SRR1 = GPRi(rd);
    break;
  case eXenonSPR::CFAR:
    curThread.SPR.CFAR = GPRi(rd);
    break;
  case eXenonSPR::CTRLWR: {
    uCTRL newCTRL;
    newCTRL.hexValue = static_cast<u32>(GPRi(rd));
    if (ppeState->currentThread == ePPUThread_Zero) {
      // Thread Zero
      if (ppeState->SPR.CTRL.TE1) {
        // TE1 is set, do not modify it.
        newCTRL.TE1 = 1;
      }
    } else {
      // Thread One
      if (ppeState->SPR.CTRL.TE0) {
        // TE0 is set, do not modify it.
        newCTRL.TE0 = 1;
      }
    }

    // TODO: Check this, reversing and docs suggests this is the correct behavior.
    // If a thread is being enabled, we must generate a reset interrupt on said thread.
    if (ppeState->SPR.CTRL.TE0 == 0 && newCTRL.TE0) { ppeState->ppuThread[0].exceptReg |= ppuSystemResetEx; }
    if (ppeState->SPR.CTRL.TE1 == 0 && newCTRL.TE1) { ppeState->ppuThread[1].exceptReg |= ppuSystemResetEx; }

    LOG_TRACE(Xenon, "{} (Thread{:#d}): Setting ctrl to {:#x}", ppeState->ppuName, (u8)ppeState->currentThread, newCTRL.hexValue);

    ppeState->SPR.CTRL = newCTRL;
    break;
  }
  case eXenonSPR::VRSAVE:
    curThread.SPR.VRSAVE = static_cast<u32>(GPRi(rd));
    break;
  case eXenonSPR::SPRG0:
    curThread.SPR.SPRG0 = GPRi(rd);
    break;
  case eXenonSPR::SPRG1:
    curThread.SPR.SPRG1 = GPRi(rd);
    break;
  case eXenonSPR::SPRG2:
    curThread.SPR.SPRG2 = GPRi(rd);
    break;
  case eXenonSPR::SPRG3:
    curThread.SPR.SPRG3 = GPRi(rd);
    break;
  case eXenonSPR::TBLWO:
    ppeState->SPR.TB.TBL = (GPRi(rd) & 0xFFFFFFFF);
    break;
  case eXenonSPR::TBUWO:
    ppeState->SPR.TB.TBU = (GPRi(rd) & 0xFFFFFFFF);
    break;
  case eXenonSPR::HSPRG0:
    curThread.SPR.HSPRG0 = GPRi(rd);
    break;
  case eXenonSPR::HSPRG1:
    curThread.SPR.HSPRG1 = GPRi(rd);
    break;
  case eXenonSPR::HDEC:
    ppeState->SPR.HDEC = static_cast<u32>(GPRi(rd));
    break;
  case eXenonSPR::RMOR:
    ppeState->SPR.RMOR.hexValue = GPRi(rd);
    break;
  case eXenonSPR::HRMOR:
    ppeState->SPR.HRMOR.hexValue = GPRi(rd);
    break;
  case eXenonSPR::LPCR:
    ppeState->SPR.LPCR.hexValue = GPRi(rd);
    break;
  case eXenonSPR::LPIDR:
    ppeState->SPR.LPIDR.hexValue = static_cast<u32>(GPRi(rd));
    break;
  case eXenonSPR::TSCR:
    ppeState->SPR.TSCR.hexValue = static_cast<u32>(GPRi(rd));
    break;
  case eXenonSPR::TTR:
    ppeState->SPR.TTR.hexValue = GPRi(rd);
    break;
  case eXenonSPR::PPE_TLB_Index:
    ppeState->SPR.PPE_TLB_Index.hexValue = GPRi(rd);
    break;
  case eXenonSPR::PPE_TLB_Index_Hint:
    curThread.SPR.PPE_TLB_Index_Hint.hexValue = GPRi(rd);
    break;
  case eXenonSPR::PPE_TLB_VPN:
    ppeState->SPR.PPE_TLB_VPN.hexValue = GPRi(rd);
    mmuAddTlbEntry(ppeState);
    break;
  case eXenonSPR::PPE_TLB_RPN:
    ppeState->SPR.PPE_TLB_RPN.hexValue = GPRi(rd);
    break;
  case eXenonSPR::HID0:
    ppeState->SPR.HID0.hexValue = GPRi(rd);
    break;
  case eXenonSPR::HID1:
    ppeState->SPR.HID1.hexValue = GPRi(rd);
    break;
  case eXenonSPR::HID4:
    ppeState->SPR.HID4.hexValue = GPRi(rd);
    break;
  case eXenonSPR::HID6:
    ppeState->SPR.HID6.hexValue = GPRi(rd);
    break;
  case eXenonSPR::DABR:
    curThread.SPR.DABR.hexValue = GPRi(rd);
    break;
  case eXenonSPR::DABRX:
    curThread.SPR.DABRX.hexValue = GPRi(rd);
    break;
  default:
    LOG_ERROR(Xenon, "{}(Thrd{:#d}) SPR 0x{:X} =0x{:X}", ppeState->ppuName, static_cast<u8>(curThreadId), spr, GPRi(rd));
    break;
  }
}

// Move From Machine State Register
void PPCInterpreter::PPCInterpreter_mfmsr(sPPEState *ppeState) {
  GPRi(rd) = curThread.SPR.MSR.hexValue;
}

// Move To Machine State Register
void PPCInterpreter::PPCInterpreter_mtmsr(sPPEState *ppeState) {
  curThread.SPR.MSR.hexValue = GPRi(rs);
}

// Move To Machine State Register Doubleword
void PPCInterpreter::PPCInterpreter_mtmsrd(sPPEState *ppeState) {
  if (_instr.l15) {
    /*
       Bits 48 and 62 of register RS are placed into the corresponding bits
       of the MSR. The remaining bits of the MSR are unchanged
    */

    // Bit 48 = MSR[EE]
    curThread.SPR.MSR.EE = (GPRi(rs) & 0x8000) == 0x8000 ? 1 : 0;
    // Bit 62 = MSR[RI]
    curThread.SPR.MSR.RI = (GPRi(rs) & 0x2) == 0x2 ? 1 : 0;
  } else { // L = 0
    /*
       The result of ORing bits 00 and 01 of register RS is placed into MSR0
       The result of ORing bits 48 and 49 of register RS is placed into MSR48
       The result of ORing bits 58 and 49 of register RS is placed into MSR58
       The result of ORing bits 59 and 49 of register RS is placed into MSR59
       Bits 1:2, 4:47, 49:50, 52:57, and 60:63 of register RS are placed into
       the corresponding bits of the MSR
    */
    const u64 regRS = GPRi(rs);
    curThread.SPR.MSR.hexValue = regRS;

    // MSR0 = (RS)0 | (RS)1
    curThread.SPR.MSR.SF = (regRS & 0x8000000000000000) || (regRS & 0x4000000000000000) ? 1 : 0;

    // MSR48 = (RS)48 | (RS)49
    curThread.SPR.MSR.EE = (regRS & 0x8000) || (regRS & 0x4000) ? 1 : 0;

    // MSR58 = (RS)58 | (RS)49
    curThread.SPR.MSR.IR = (regRS & 0x20) || (regRS & 0x4000) ? 1 : 0;

    // MSR59 = (RS)59 | (RS)49
    curThread.SPR.MSR.DR = (regRS & 0x10) || (regRS & 0x4000) ? 1 : 0;
  }
}

// Synchronize
void PPCInterpreter::PPCInterpreter_sync(sPPEState *ppeState) {
  // Do nothing
}

// Data Cache Block Flush
void PPCInterpreter::PPCInterpreter_dcbf(sPPEState *ppeState) {
  // Do nothing
}

// Data Cache Block Invalidate
void PPCInterpreter::PPCInterpreter_dcbi(sPPEState *ppeState) {
  // Do nothing
}

// Data Cache Block Touch
void PPCInterpreter::PPCInterpreter_dcbt(sPPEState *ppeState) {
  // Do nothing
}

// Data Cache Block Touch for Store
void PPCInterpreter::PPCInterpreter_dcbtst(sPPEState *ppeState) {
  // Do nothing
}
