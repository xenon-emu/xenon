// Copyright 2025 Xenon Emulator Project

#include "PPCInterpreter.h"
#include "Core/XCPU/PostBus/PostBus.h"

//
// Xbox 360 Memory map, info taken from various sources.
//

// Everything can fit on 32 bits on the 360, so MS uses upper bits of the 64 bit
// EA to manage L2 cache, further research required on this.

// 0x200 00000000 - 0x200 00008000                  32K SROM - 1BL Location.
// 0x200 00010000 - 0x200 00020000                  64K SRAM.
// 0x200 00050000 - 0x200 00056000                  Interrupt controller.
// 0x200 C8000000 - 0x200 C9000000                  NAND Flash 1:1
// 0x200 C9000000 - 0x200 CA000000                  Currently unknown, I
// suspect that maybe it is additional space for 512 MB NAND Flash images.
// 0x200 EA000000 - 0x200 EA010000                  PCI Bridge
// 0x200 EC800000 - 0x200 EC810000                  GPU

#define MMU_PAGE_SIZE_4KB 12
#define MMU_PAGE_SIZE_64KB 16
#define MMU_PAGE_SIZE_1MB 20
#define MMU_PAGE_SIZE_16MB 24

// The processor generated address (EA) is subdivided, upper 32 bits are used
// as flags for the 'Security Engine'
//
// 0x00000X**_00000000 X = region, ** = key select
// X = 0 should be Physical
// X = 1 should be Hashed
// X = 2 should be SoC
// X = 3 should be Encrypted

// 0x8000020000060000 Seems to be the random number generator. Implement this?

/*
 * Hash page table definitions
 */

#define PPC_SPR_SDR_64_HTABORG 0x0FFFFFFFFFFC0000ULL
#define PPC_SPR_SDR_64_HTABSIZE 0x000000000000001FULL

#define PPC_HPTES_PER_GROUP 8

//
// PTE 0.
//

// Page valid.
#define PPC_HPTE64_VALID 0x0000000000000001ULL
// Page Hash identifier.
#define PPC_HPTE64_HASH 0x0000000000000002ULL
// Page Large bit.
#define PPC_HPTE64_LARGE 0x0000000000000004ULL
// Page AVPN.
#define PPC_HPTE64_AVPN 0x0001FFFFFFFFFF80ULL
// Page AVPN [0:51]
#define PPC_HPTE64_AVPN_0_51 0x0001FFFFFFFFF000ULL

//
// PTE 1.
//

// RPN when L = 0.
#define PPC_HPTE64_RPN_NO_LP 0x000003FFFFFFF000UL
// RPN when L = 1.
#define PPC_HPTE64_RPN_LP 0x000003FFFFFFE000UL
// Large Page Selector bit.
#define PPC_HPTE64_LP 0x0000000000001000ULL
// Bolted PTE.
#define HPTE64_V_BOLTED 0x0000000000000010ULL
// Changed bit.
#define PPC_HPTE64_C 0x0000000000000080ULL
// Referenced bit.
#define PPC_HPTE64_R 0x0000000000000100ULL

// DSISR Flags.
#define DSISR_ISSTORE 0x02000000
#define DSISR_NOPTE 0x40000000

// Page table entry structure.
struct PPC_HPTE64 {
  u64 pte0;
  u64 pte1;
};

void PPCInterpreter::PPCInterpreter_slbia(PPU_STATE *ppuState) {
  for (auto &slbEntry : curThread.SLB) {
    slbEntry.V = 0;
  }
  // Invalidate both ERAT's
  curThread.iERAT.invalidateAll();
  curThread.dERAT.invalidateAll();
}

void PPCInterpreter::PPCInterpreter_tlbiel(PPU_STATE *ppuState) {
  X_FORM_L_rB

  // The PPU adds two new fields to this instruction, them being LP abd IS.

  bool LP = (curThread.GPR[rB] & 0x1000) >> 12;
  bool invalSelector =
      (curThread.GPR[rB] & 0x800) >> 11;
  u8 p = mmuGetPageSize(ppuState, L, LP);
  u64 VA, VPN = 0;

  if (invalSelector == 0) {
    // The TLB is as selective as possible when invalidating TLB entries.The
    // invalidation match criteria is VPN[38:79 - p], L, LP, and LPID.

    VA = curThread.GPR[rB];

    if (VA > 0x7FFFFFFF) {
      VPN = (VA >> 16) & ~0x7F;
    } else if (VA > 0x20000000) {
      if (p == 0x18)
        VPN = (VA >> 16) & ~0xFF;
      else
        VPN = (VA >> 16) & ~0x7F;
    } else {
      VPN = (VA >> 16) & ~0xF;
    }

    for (auto &tlbEntry : ppuState->TLB.tlbSet0) {
      if (tlbEntry.V && tlbEntry.VPN == VPN && tlbEntry.p == p) {
        tlbEntry.V = 0;
      }
    }
    for (auto &tlbEntry : ppuState->TLB.tlbSet1) {
      if (tlbEntry.V && tlbEntry.VPN == VPN && tlbEntry.p == p) {
        tlbEntry.V = 0;
      }
    }
    for (auto &tlbEntry : ppuState->TLB.tlbSet2) {
      if (tlbEntry.V && tlbEntry.VPN == VPN && tlbEntry.p == p) {
        tlbEntry.V = 0;
      }
    }
    for (auto &tlbEntry : ppuState->TLB.tlbSet3) {
      if (tlbEntry.V && tlbEntry.VPN == VPN && tlbEntry.p == p) {
        tlbEntry.V = 0;
      }
    }
  } else {
    // Index to one of the 256 rows of the tlb. Possible entire tlb
    // invalidation.
    u8 tlbCongruenceClass = 0;
    u64 rb_44_51 =
        (curThread.GPR[rB] & 0xFF000) >> 12;

    ppuState->TLB.tlbSet0[rb_44_51].V = false;
    ppuState->TLB.tlbSet0[rb_44_51].VPN = 0;
    ppuState->TLB.tlbSet0[rb_44_51].RPN = 0;
    ppuState->TLB.tlbSet0[rb_44_51].L = false;
    ppuState->TLB.tlbSet0[rb_44_51].LP = 0;
    ppuState->TLB.tlbSet0[rb_44_51].p = 0;

    ppuState->TLB.tlbSet1[rb_44_51].V = false;
    ppuState->TLB.tlbSet1[rb_44_51].VPN = 0;
    ppuState->TLB.tlbSet1[rb_44_51].RPN = 0;
    ppuState->TLB.tlbSet1[rb_44_51].L = false;
    ppuState->TLB.tlbSet1[rb_44_51].LP = 0;
    ppuState->TLB.tlbSet1[rb_44_51].p = 0;

    ppuState->TLB.tlbSet2[rb_44_51].V = false;
    ppuState->TLB.tlbSet2[rb_44_51].VPN = 0;
    ppuState->TLB.tlbSet2[rb_44_51].RPN = 0;
    ppuState->TLB.tlbSet2[rb_44_51].L = false;
    ppuState->TLB.tlbSet2[rb_44_51].LP = 0;
    ppuState->TLB.tlbSet2[rb_44_51].p = 0;

    ppuState->TLB.tlbSet3[rb_44_51].V = false;
    ppuState->TLB.tlbSet3[rb_44_51].VPN = 0;
    ppuState->TLB.tlbSet3[rb_44_51].RPN = 0;
    ppuState->TLB.tlbSet3[rb_44_51].L = false;
    ppuState->TLB.tlbSet3[rb_44_51].LP = 0;
    ppuState->TLB.tlbSet3[rb_44_51].p = 0;
  }

  // Should only invalidate entries for a specific set of addresses.
  // Invalidate both ERAT's *** BUG *** !!!
  curThread.iERAT.invalidateAll();
  curThread.dERAT.invalidateAll();
}

void PPCInterpreter::PPCInterpreter_tlbie(PPU_STATE* ppuState)
{
    // Do nothing.
    LOG_INFO(Xenon, "tlbie");

    // Should only invalidate entries for a specific set of addresses.
    // Invalidate both ERAT's *** BUG *** !!!
    curThread.iERAT.invalidateAll();
    curThread.dERAT.invalidateAll();
}

void PPCInterpreter::PPCInterpreter_tlbsync(PPU_STATE* ppuState)
{
    // Do nothing.
    LOG_INFO(Xenon, "tlbsync");
}

// Helper function for getting Page Size (p bit).
u8 PPCInterpreter::mmuGetPageSize(PPU_STATE *ppuState, bool L, u8 LP) {

  // Large page selection works the following way:
  // First check if pages are large (L)
  // if (L) the page size can be one of two defined pages. On the XBox 360,
  // MS decided to use two of the three page sizes, 64Kb and 16Mb.
  // Selection between them is made using bits 16 - 19 of HID6 SPR.

  // HID6 16-17 bits select Large Page size 1.
  // HID6 18-19 bits select Large Page size 2.
  u8 LB_16_17 = static_cast<u8>(QGET(ppuState->SPR.HID6, 16, 17));
  u8 LB_18_19 = static_cast<u8>(QGET(ppuState->SPR.HID6, 18, 19));

  // Final p size.
  u8 p = 0;
  // Page size in decimal.
  u32 pSize = 0;

  // Large page?
  if (L == 0) {
    // If L equals 0, the small page size is used, 4Kb in this case.
    pSize = 4096;
  } else {
    // Large Page Selector
    if (LP == 0) {
      switch (LB_16_17) {
      case 0b0000:
        pSize = 16777216; // 16 Mb page size
        break;
      case 0b0001:
        pSize = 1048576; // 1 Mb page size
        break;
      case 0b0010:
        pSize = 65536; // 64 Kb page size
        break;
      }
    } else if (LP == 1) {
      switch (LB_18_19) {
      case 0b0000:
        pSize = 16777216; // 16 Mb page size
        break;
      case 0b0001:
        pSize = 1048576; // 1 Mb page size
        break;
      case 0b0010:
        pSize = 65536; // 16 Mb page size
        break;
      }
    }
  }

  // p size is Log(2) of Page Size.
  p = static_cast<u8>(log2(pSize));
  return p;
}

// This is done when TLB Reload is in software-controlled mode.
void PPCInterpreter::mmuAddTlbEntry(PPU_STATE *ppuState) {
#define PPE_TLB_INDEX_LVPN_MASK 0xE00000000000
#define MMU_GET_TLB_INDEX_TI(x) (static_cast<u16>((x & 0xFF0) >> 4))
#define MMU_GET_TLB_INDEX_TS(x) (static_cast<u16>(x & 0xF))

#define PPE_TLB_VPN_V_MASK 0x1
#define PPE_TLB_VPN_L_MASK 0x4
#define PPE_TLB_VPN_AVPN_MASK 0x1FFFFFFFFFF80
#define PPE_TLB_VPN_AVPN_16MB_MASK 0x1FFFFFFFFFF00

#define PPE_TLB_RPN_ARPN_MASK 0x3FFFFFFE000
#define PPE_TLB_RPN_ARPN_AND_LP_MASK 0x3FFFFFFF000
#define PPE_TLB_RPN_LP_MASK 0x1000

  u64 tlbIndex = ppuState->SPR.PPE_TLB_Index;
  u64 tlbVpn = ppuState->SPR.PPE_TLB_VPN;
  u64 tlbRpn = ppuState->SPR.PPE_TLB_RPN;

  // Valid entry.
  bool vpnValid = tlbVpn & PPE_TLB_VPN_V_MASK;
  // Large pages.
  bool L = (tlbVpn & PPE_TLB_VPN_L_MASK) >> 2;
  // Large pages selector
  u32 LP = (tlbRpn & PPE_TLB_RPN_LP_MASK) >> 12;

  // Get Page Size
  u8 p = mmuGetPageSize(ppuState, L, LP);

  // AVPN
  u64 AVPN = 0;

  // VPN = AVPN concatenated with LVPN
  // The AVPN corresponds to VPN[15:56].
  // When reading a 16 MB TLB entry, bit [56] of the AVPN is undefined.
  if (p == MMU_PAGE_SIZE_16MB) // 16 Mb entry
  {
    AVPN = tlbVpn & PPE_TLB_VPN_AVPN_16MB_MASK;
  } else {
    AVPN = tlbVpn & PPE_TLB_VPN_AVPN_MASK;
  }

  u64 ARPN = 0;

  if (L) {
    ARPN = tlbRpn & PPE_TLB_RPN_ARPN_MASK;
  } else {
    ARPN = tlbRpn & PPE_TLB_RPN_ARPN_AND_LP_MASK;
  }

  // LVPN[0:2] corresponds to VPN[57:59].
  u64 LVPN = (tlbIndex & PPE_TLB_INDEX_LVPN_MASK) >> 41;

  // Virtual page number
  u64 VPN = AVPN | LVPN;

  // Real Page Number
  // The ARPN corresponds to RPN[22:50].
  // To obtain the full 30 - bit RPN, the ARPN is combined with the LP field.
  // We already did that.
  u64 RPN = ARPN;

  // Tlb Index (0 - 255) of current tlb set.
  u16 TI = MMU_GET_TLB_INDEX_TI(tlbIndex);
  u16 TS = MMU_GET_TLB_INDEX_TS(tlbIndex);

  // std::cout << "XCPU[" << ppuState->ppuName << "(Thrd"<< ppuState->currentThread
  //     << ")](MMU) : Adding tlb entry : (" << TI * TS << ")" << std::endl;
  // std::cout << " *** RPN:         0x" << RPN << std::endl;
  // std::cout << " *** VPN:         0x" << VPN << std::endl;
  // std::cout << "  *  TLB Set:     " << std::dec << TS << std::endl;
  // std::cout << "  *  TLB Index:   " << std::hex << TI << std::endl;
  // std::cout << "  *  p Size:      " << (static_cast<u16>(p) << std::endl;
  // std::cout << std::hex << std::endl;

  // TLB set to choose from
  // There are 4 sets of 256 entries each:
  switch (TS) {
  case 0b1000:
    ppuState->TLB.tlbSet0[TI].V = true;
    ppuState->TLB.tlbSet0[TI].RPN = RPN;
    ppuState->TLB.tlbSet0[TI].VPN = VPN;
    ppuState->TLB.tlbSet0[TI].p = p;
    ppuState->TLB.tlbSet0[TI].L = L;
    ppuState->TLB.tlbSet0[TI].LP = LP;
    break;
  case 0b0100:
    ppuState->TLB.tlbSet1[TI].V = true;
    ppuState->TLB.tlbSet1[TI].RPN = RPN;
    ppuState->TLB.tlbSet1[TI].VPN = VPN;
    ppuState->TLB.tlbSet1[TI].p = p;
    ppuState->TLB.tlbSet1[TI].L = L;
    ppuState->TLB.tlbSet1[TI].LP = LP;
    break;
  case 0b0010:
    ppuState->TLB.tlbSet2[TI].V = true;
    ppuState->TLB.tlbSet2[TI].RPN = RPN;
    ppuState->TLB.tlbSet2[TI].VPN = VPN;
    ppuState->TLB.tlbSet2[TI].p = p;
    ppuState->TLB.tlbSet2[TI].L = L;
    ppuState->TLB.tlbSet2[TI].LP = LP;
    break;
  case 0b0001:
    ppuState->TLB.tlbSet3[TI].V = true;
    ppuState->TLB.tlbSet3[TI].RPN = RPN;
    ppuState->TLB.tlbSet3[TI].VPN = VPN;
    ppuState->TLB.tlbSet3[TI].p = p;
    ppuState->TLB.tlbSet3[TI].L = L;
    ppuState->TLB.tlbSet3[TI].LP = LP;
    break;
  }
}

// Translation Lookaside Buffer Search
bool PPCInterpreter::mmuSearchTlbEntry(PPU_STATE *ppuState, u64 *RPN, u64 VA,
                                       u64 VPN, u8 p, bool LP) {
  // Index to choose from the 256 ways of the TLB
  u16 tlbIndex = 0;
  // Tlb Set that was least Recently used for replacement.
  u8 tlbSet = 0;

  // 4 Kb - (VA[52:55] xor VA[60:63]) || VA[64:67]
  // 64 Kb - (VA[52:55] xor VA[56:59]) || VA[60:63]
  // 16MB - VA[48:55]

  // 52-55 bits of 80 VA
  u16 bits36_39 = static_cast<u16>(QGET(VA, 36, 39));
  // 56-59 bits of 80 VA
  u16 bits40_43 = static_cast<u16>(QGET(VA, 40, 43));
  // 60-63 bits of 80 VA
  u16 bits44_47 = static_cast<u16>(QGET(VA, 44, 47));
  // 64-67 bits of 80 VA
  u16 bits48_51 = static_cast<u16>(QGET(VA, 48, 51));
  // 48-55 bits of 80 VA
  u16 bits32_39 = static_cast<u16>(QGET(VA, 32, 39));

  switch (p) {
  case MMU_PAGE_SIZE_64KB:
    tlbIndex = ((bits36_39 ^ bits40_43) << 4 | bits44_47);
    break;
  case MMU_PAGE_SIZE_16MB:
    if (bits32_39 < 0x80 && bits32_39 > 0x20)
      tlbIndex = bits32_39 & 0xF;
    else if (bits32_39 < 0x20)
      tlbIndex = bits32_39;
    else {
      tlbIndex = (bits40_43 << 4) ^ bits44_47;
    }
    break;
  default:
    // p = 12 bits, 4KB
    tlbIndex = ((bits36_39 ^ bits44_47) << 4 | bits48_51);
    break;
  }

  if (ppuState->TLB.tlbSet0[tlbIndex].V) {
    if (static_cast<u16>(ppuState->TLB.tlbSet0[tlbIndex].VPN) == static_cast<u16>(VPN)) {
      // Check to see if its this entry!
      // Need to have a tag
      if (ppuState->TLB.tlbSet0[tlbIndex].p == p) {
        if (ppuState->TLB.tlbSet0[tlbIndex].L) {
          if (ppuState->TLB.tlbSet0[tlbIndex].LP == LP) {
            // Probably It!
            *RPN = ppuState->TLB.tlbSet0[tlbIndex].RPN;
            return true;
          }
        } else {
          // Probably It!
          *RPN = ppuState->TLB.tlbSet0[tlbIndex].RPN;
          return true;
        }
      }
    }
  } else {
    // Entry was invalid, make this set the a candidate for refill.
    tlbSet = 0b1000;
  }
  if (ppuState->TLB.tlbSet1[tlbIndex].V) {
    if (static_cast<u16>(ppuState->TLB.tlbSet1[tlbIndex].VPN) == static_cast<u16>(VPN)) {
      // Check to see if its this entry!
      // Need to have a tag
      if (ppuState->TLB.tlbSet1[tlbIndex].p == p) {
        if (ppuState->TLB.tlbSet1[tlbIndex].L) {
          if (ppuState->TLB.tlbSet1[tlbIndex].LP == LP) {
            // Probably It!
            *RPN = ppuState->TLB.tlbSet1[tlbIndex].RPN;
            return true;
          }
        } else {
          // Probably It!
          *RPN = ppuState->TLB.tlbSet1[tlbIndex].RPN;
          return true;
        }
      }
    }
  } else {
    // Entry was invalid, make this set the a candidate for refill.
    tlbSet = 0b0100;
  }
  if (ppuState->TLB.tlbSet2[tlbIndex].V) {
    if (static_cast<u16>(ppuState->TLB.tlbSet2[tlbIndex].VPN) == static_cast<u16>(VPN)) {
      // Check to see if its this entry!
      // Need to have a tag
      if (ppuState->TLB.tlbSet2[tlbIndex].p == p) {
        if (ppuState->TLB.tlbSet2[tlbIndex].L) {
          if (ppuState->TLB.tlbSet2[tlbIndex].LP == LP) {
            // Probably It!
            *RPN = ppuState->TLB.tlbSet2[tlbIndex].RPN;
            return true;
          }
        } else {
          // Probably It!
          *RPN = ppuState->TLB.tlbSet2[tlbIndex].RPN;
          return true;
        }
      }
    }
  } else {
    // Entry was invalid, make this set the a candidate for refill.
    tlbSet = 0b0010;
  }
  if (ppuState->TLB.tlbSet3[tlbIndex].V) {
    if (static_cast<u16>(ppuState->TLB.tlbSet3[tlbIndex].VPN) == static_cast<u16>(VPN)) {
      // Check to see if its this entry!
      // Need to have a tag
      if (ppuState->TLB.tlbSet3[tlbIndex].p == p) {
        if (ppuState->TLB.tlbSet3[tlbIndex].L) {
          if (ppuState->TLB.tlbSet3[tlbIndex].LP == LP) {
            // Probably It!
            *RPN = ppuState->TLB.tlbSet3[tlbIndex].RPN;
            return true;
          }
        } else {
          // Probably It!
          *RPN = ppuState->TLB.tlbSet3[tlbIndex].RPN;
          return true;
        }
      }
    }
  } else {
    // Entry was invalid, make this set the a candidate for refill.
    tlbSet = 0b0001;
  }

  // If the PPE is running on TLB Software managed mode, then this SPR
  // is updated every time a Data or Instr Storage Exception occurs. This
  // ensures that the next time that the tlb software updates via an
  // interrupt, the index for replacement is not conflictive.
  // On normal conditions this is done for the LRU index of the TLB.

  // Software management of the TLB. 0 = Hardware, 1 = Software.
  bool tlbSoftwareManaged = ((ppuState->SPR.LPCR & 0x400) >> 10);

  if (tlbSoftwareManaged) {
    u64 tlbIndexHint =
        curThread.SPR.PPE_TLB_Index_Hint;
    u8 currentTlbSet = tlbIndexHint & 0xF;
    u8 currentTlbIndex = static_cast<u8>(tlbIndexHint & 0xFF0) >> 4;
    currentTlbSet = tlbSet;
    if (currentTlbIndex == 0xFF) {
      if (currentTlbSet == 8) {
        currentTlbSet = 1;
      } else {
        currentTlbSet = currentTlbSet << 1;
      }
    }

    if (currentTlbSet == 0)
      currentTlbSet = 1;

    tlbIndex = tlbIndex << 4;
    tlbIndexHint = tlbIndex |= currentTlbSet;
    curThread.SPR.PPE_TLB_Index_Hint =
        tlbIndexHint;
  }
  return false;
}

// Routine to read a string from memory, using a PSTRNG given by the kernel.
void PPCInterpreter::mmuReadString(PPU_STATE *ppuState, u64 stringAddress,
                                   char *string, u32 maxLenght) {
  u32 strIndex;
  u32 stringBufferAddress = 0;
  u16 Length = 0;

  Length = MMURead16(ppuState, stringAddress);
  if (Length < maxLenght)
    maxLenght = Length + 1;

  // Search Buffer
  stringBufferAddress = MMURead32(ppuState, stringAddress + 4);

  for (strIndex = 0; strIndex < maxLenght; strIndex++, stringBufferAddress++) {
    string[strIndex] = MMURead8(ppuState, stringBufferAddress);
    if (string[strIndex] == 0)
      break;
  }

  string[maxLenght - 1] = 0;
}

SECENG_ADDRESS_INFO
PPCInterpreter::mmuGetSecEngInfoFromAddress(u64 inputAddress) {
  // 0x00000X**_00000000 X = region, ** = key select
  // X = 0 should be Physical
  // X = 1 should be Hashed
  // X = 2 should be SoC
  // X = 3 should be Encrypted

  SECENG_ADDRESS_INFO addressInfo;

  u64 regionMask = 0xF0000000000;
  u64 keyMask = 0xFF00000000;
  u32 region = (inputAddress & regionMask) >> 32;

  addressInfo.keySelected = static_cast<u8>((inputAddress & keyMask) >> 32);
  addressInfo.accesedAddr = static_cast<u32>(inputAddress);

  switch (region) {
  case 0x0:
    addressInfo.regionType = SECENG_REGION_PHYS;
    break;
  case 0x100:
    addressInfo.regionType = SECENG_REGION_HASHED;
    break;
  case 0x200:
    addressInfo.regionType = SECENG_REGION_SOC;
    break;
  case 0x300:
    addressInfo.regionType = SECENG_REGION_ENCRYPTED;
    break;
  default:
    break;
  }
  return addressInfo;
}

u64 PPCInterpreter::mmuContructEndAddressFromSecEngAddr(u64 inputAddress,
                                                        bool *socAccess) {
  SECENG_ADDRESS_INFO inputAddressInfo =
      mmuGetSecEngInfoFromAddress(inputAddress);

  u64 outputAddress = 0;

  switch (inputAddressInfo.regionType) {
  case SECENG_REGION_PHYS:
    // Going to RAM directly.
    outputAddress = inputAddressInfo.accesedAddr;
    break;
  case SECENG_REGION_HASHED:
    // Going to RAM directly without hashing!
    outputAddress = inputAddressInfo.accesedAddr;
    break;
  case SECENG_REGION_SOC:
    *socAccess = true;
    outputAddress = inputAddressInfo.accesedAddr;
    break;
  case SECENG_REGION_ENCRYPTED:
    // Going to RAM directly without encryption!
    outputAddress = inputAddressInfo.accesedAddr;
    break;
  default:
    break;
  }
  return outputAddress;
}

// Main address translation mechanism used on the XCPU.
bool PPCInterpreter::MMUTranslateAddress(u64 *EA, PPU_STATE *ppuState,
                                         bool memWrite) {
  // Every time the CPU does a load or store, it goes trough the MMU.
  // The MMU decides based on MSR, and some other regs if address translation
  // for Instr/Data is in Real Mode (EA = RA) or in Virtual Mode (Page
  // Address Translation).

  // Xbox 360 MMU contains a very similar to the CELL-BE MMU.
  // Has two ERAT's (64 entry, 2 way), one for Instructions (I-ERAT) and Data
  // (D-ERAT), this caches Effective to Physical adress translations done
  // recently.
  // It also contains a 1024 entry 4 * 256 columns TLB array, wich caches
  // recent Page tables. TLB on the Xbox 360 can be Software/Hardware managed.
  // This is controlled via TL bit of the LPCR SPR.

  /* TODO */
  // Implement L1 per-core data/inst cache and cache handling code.

  //
  // Current thread SPR's used in MMU..
  //

  // Machine State Register.
  MSRegister _msr = curThread.SPR.MSR;
  // Logical Partition Control Register.
  u64 LPCR = ppuState->SPR.LPCR;
  // Hypervisor Real Mode Offset Register.
  u64 HRMOR = ppuState->SPR.HRMOR;
  // Real Mode Offset Register.
  u64 RMOR = ppuState->SPR.RMOR;
  // Upper 32 bits of EA, used when getting the VPN.
  u64 upperEA = (*EA & 0xFFFFFFFF00000000);

  // On 32-Bit mode of opertaion MSR[SF] = 0, high order 32 bits of the EA
  // are truncated, effectively clearing them.
  if (!_msr.SF)
    *EA = static_cast<u32>(*EA);

  // Real Adress, this is what we want.
  u64 RA = 0;

  //
  // ERAT's
  //

  // Each ERAT entry holds the EA-to-RA translation for an aligned 4 KB area of memory. When 
  // using a 4 KB page size, each ERAT entry holds the information for exactly one page.When
  // using large pages, each ERAT entry contains a 4 KB section of the page, meaning that large
  // pages can occupy several ERAT entries.All EA - to - RA mappings are kept in the ERAT including
  // both real - mode and virtual - mode addresses(that is, addresses accessed with MSR[IR] equal to
  // 0 or 1).
  // TODO:
  // The ERATs identify each translation entry with some combination of the MSR[SF, IR, DR, 
  // PR, and HV] bits, depending on whether the entry is in the I - ERAT or D - ERAT.This allows the
  // ERATs to distinguish between translations that are valid for the various modes of operation.
  // See IBM_CBE_Handbook_v1.1 Page 82.
  
  // Search ERAT's
  if (curThread.iFetch) {
    // iERAT
    RA = curThread.iERAT.getElement((*EA & ~0xFFF));
    if (RA != -1) {
      RA |= (*EA & 0xFFF);
      *EA = RA;
      return true;
    }
  }
  else {
    // dERAT
    RA = curThread.dERAT.getElement((*EA & ~0xFFF));
    if (RA != -1) {
      RA |= (*EA & 0xFFF);
      *EA = RA;
      return true;
    }
  }

  // Holds whether the cpu thread issuing the fetch is running in Real or
  // Virtual mode. It defaults to Real Mode, as this is how the XCPU starts
  // all threads.
  bool realMode = true;
  // If this EA bit is set, then address generated in Real Mode isn't OR'ed
  // with the contents of HRMOR register.
  bool eaZeroBit = ((*EA & 0x8000000000000000) >> 63);
  // LPCR(LPES) bit 1.
  bool lpcrLPESBit1 = ((LPCR & 0x8) >> 3);
  // Software management of the TLB. 0 = Hardware, 1 = Software.
  bool tlbSoftwareManaged = ((LPCR & 0x400) >> 10);

  // Instruction relocate and instruction fetch
  if (_msr.IR && curThread.iFetch)
    realMode = false;
  // Data fetch
  else if (_msr.DR)
    realMode = false;

  // Real Addressing Mode
  if (realMode) {
    // If running in Hypervisor Offset mode.
    if (_msr.HV) {
      if (eaZeroBit) {
        // Real address is bits 22-63 of Effective Address.
        // RA = EA[22:63]
        RA = (*EA & 0x3FFFFFFFFFF);
      } else {
        // RA = (EA[22:43] | HRMOR[22:43]) || EA[44:63]
        RA = (((*EA & 0x3FFFFF00000) | (HRMOR & 0x3FFFFF00000)) |
              (*EA & 0xFFFFF));
      }
    }
    // Real Offset Mode
    else {
      // RA = (EA[22:43] | RMOR[22:43]) || EA[44:63]
      if (lpcrLPESBit1) {
        RA = (((*EA & 0x3FFFFF00000) | (RMOR & 0x3FFFFF00000)) |
              (*EA & 0xFFFFF));
      } else {
        // Mode Fault. LPAR Interrupt.
        LOG_CRITICAL(Xenon_MMU, "LPAR Interrupt unimplemented.");
        SYSTEM_PAUSE();
      }
    }
  }
  // Virtual Mode
  else {
    // Page size bits
    u8 p = 0;
    // Large pages
    bool L = 0;
    // Large Page Selector (LP)
    u8 LP = 0;
    // Efective Segment ID
    u64 ESID = QGET(*EA, 0, 35);
    // ESID = ESID << 28;
    //  Virtual Segment ID
    u64 VSID = 0;

    /*** Segmentation ***/
    // 64 bit EA -> 65 bit VA
    // ESID -> VSID

    SLBEntry currslbEntry;

    bool slbHit = false;
    // Search the SLB to get the VSID
    for (auto &slbEntry :
         curThread.SLB) {
      if (slbEntry.V && (slbEntry.ESID == ESID)) {
        // Entry valid & SLB->ESID = EA->VSID
        currslbEntry = slbEntry;
        VSID = slbEntry.VSID;
        L = slbEntry.L;
        LP = slbEntry.LP;
        slbHit = true;
        break;
      }
    }

    // Virtual Page Number
    u64 VPN = 0;
    // Real Page Number
    u64 RPN = 0;
    // Page
    u32 Page = 0;
    // Byte offset
    u32 Byte = 0;

    // We hit the SLB, get the VA
    if (slbHit) {
      //
      // Virtual Addresss Generation
      //

      // 1. Get the p Size
      p = mmuGetPageSize(ppuState, L, LP);

      // Get our Virtual Address - 65 bit
      // VSID + 28 bit adress data.
      u64 VA = VSID | (*EA & 0xFFFFFFF);
      // Page Offset.
      u64 PAGE = (QGET(*EA, 36, 63 - p) << p);

      if (VA > 0x7FFFFFFF) {
        VPN = (VA >> 16) & ~0x7F;
      } else if (VA > 0x20000000) {
        if (p == 0x18)
          VPN = (VA >> 16) & ~0xFF;
        else
          VPN = (VA >> 16) & ~0x7F;
      } else {
        VPN = (VA >> 16) & ~0xF;
      }

      if (mmuSearchTlbEntry(ppuState, &RPN, VA, VPN, p, LP)) {
        // TLB Hit, proceed.
        goto end;
      } else {
        // TLB miss, if we are in software managed mode, generate an
        // interrupt, else do page table search.
        if (tlbSoftwareManaged) {
          bool hv = curThread.SPR.MSR.HV;
          bool sfMode =
              curThread.SPR.MSR.SF;
          u64 CIA = curThread.CIA;

          if (curThread.iFetch) {
            curThread.exceptReg |=
                PPU_EX_INSSTOR;
          } else {

            curThread.exceptReg |=
                PPU_EX_DATASTOR;
            curThread.SPR.DAR = *EA;
            curThread.SPR.DSISR =
                DSISR_NOPTE;
          }
          return false;
        } else {
          // Page Table Lookup:
          // Walk the Page table to find a Page that translates our current VA.

          // Save MSR DR & IR Bits. When an exception occurs they must be reset
          // to whatever they where.
          bool msrDR =
              curThread.SPR.MSR.DR;
          bool msrIR =
              curThread.SPR.MSR.IR;

          // Disable relocation.
          curThread.SPR.MSR.DR = 0;
          curThread.SPR.MSR.IR = 0;

          // Get the primary and secondary hashes.
          u64 hash0 = (VSID >> 28) ^ (PAGE >> p);
          u64 hash1 = ~hash0;

          // Get hash table origin and hash table mask.
          u64 htabOrg = ppuState->SPR.SDR1 & PPC_SPR_SDR_64_HTABORG;
          u64 htabSize = ppuState->SPR.SDR1 & PPC_SPR_SDR_64_HTABSIZE;

          // Create the mask.
          u64 htabMask = QMASK(64 - (11 + htabSize), 63);

          // And both hashes with the created mask.
          hash0 = hash0 & htabMask;
          hash1 = hash1 & htabMask;

          // Get both PTEG's addresses.
          u64 pteg0Addr = htabOrg | (hash0 << 7);
          u64 pteg1Addr = htabOrg | (hash1 << 7);

          /*
          The 16-byte PTEs are organized in memory as groups of eight entries,
          called PTE groups (PTEGs), each one a full 128-byte cache line. A
          hardware table lookup consists of searching a primary PTEG and then,
          if necessary, searching a secondary PTEG to find the correct PTE to be
          reloaded into the TLB.
          */

          // Hardware searches PTEGs in the following order:
          // 1. Request the even primary PTEG entries.
          // 2. Search PTE[0], PTE[2], PTE[4], and PTE[6].
          // 3. Request the odd primary PTEG entries.
          // 4. Search PTE[1], PTE[3], PTE[5], and PTE[7].
          // 5. Repeat steps 1 through 4 with the secondary PTE.
          // 6. If no match occurs, raise a data storage exception.

          // First PTEG.
          PPC_HPTE64 pteg0[PPC_HPTES_PER_GROUP];

          // Get the pteg data from memory while relocation is off.
          for (size_t i = 0; i < PPC_HPTES_PER_GROUP; i++) {
            pteg0[i].pte0 =
                PPCInterpreter::MMURead64(ppuState, pteg0Addr + i * 16);
            pteg0[i].pte1 =
                PPCInterpreter::MMURead64(ppuState, pteg0Addr + i * 16 + 8);
          }

          // We compare all pte's in order for simplicity.
          for (size_t i = 0; i < PPC_HPTES_PER_GROUP; i++) {
            /*
                Conditions for a match to occur:

                * PTE: H = 0 for the primary PTEG, 1 for the secondary PTEG
                * PTE: V = 1
                * PTE: AVPN[0:51] = VA0:51
                * if p < 28, PTE: AVPN[52:51+q] = VA[52:51+q]
            */

            // H = 0?
            if (((pteg0[i].pte0 & PPC_HPTE64_HASH) >> 1) != 0) {
              continue;
            }

            // V?
            if ((pteg0[i].pte0 & PPC_HPTE64_VALID) != true) {
              continue;
            }

            u64 pteAVPN = pteg0[i].pte0 & PPC_HPTE64_AVPN;

            // PTE:AVPN[0:51] = VA[0:51]?
            if (((pteAVPN & PPC_HPTE64_AVPN_0_51) >> 12) != (VA >> 28)) {
              continue;
            }

            /*
                The PPE also requires that PTE[LP] = SLBE[LP] whenever PTE[L] =
               '1'. In other words, the PTE page size must match the SLBE page
               size exactly for a page-table match to occur.
            */

            // L?
            if (static_cast<u8>((pteg0[i].pte0 & PPC_HPTE64_LARGE) >> 2) != static_cast<u8>(L)) {
              continue;
            }

            // L & LP?
            if (L && (((pteg0[i].pte1 & PPC_HPTE64_LP) >> 12) != LP)) {
              continue;
            }

            // Match found. Extract the RPN from the page table.

            // Set relocation back to whatever it was.
            curThread.SPR.MSR.DR = msrDR;
            curThread.SPR.MSR.IR = msrIR;

            // Update Referenced and Change Bits if necessary.
            if (!((pteg0[i].pte1 & PPC_HPTE64_R) >> 8)) {
              // Referenced
              MMUWrite64(ppuState, pteg0Addr + i * 16 + 8,
                         (pteg0[i].pte1 | 0x100));
            }
            if (!((pteg0[i].pte1 & PPC_HPTE64_C) >> 7)) {
              // Access is a data write?
              if (memWrite) {
                // Change
                MMUWrite64(ppuState, pteg0Addr + i * 16 + 8,
                           (pteg0[i].pte1 | 0x80));
              }
            }

            if (L) {
              // RPN is PTE[86:114].
              RPN = pteg0[i].pte1 & PPC_HPTE64_RPN_LP;
            } else {
              // RPN is PTE[86:115].
              RPN = pteg0[i].pte1 & PPC_HPTE64_RPN_NO_LP;
            }

            goto end;
          }

          // Second PTEG.
          PPC_HPTE64 pteg1[PPC_HPTES_PER_GROUP];

          for (size_t i = 0; i < PPC_HPTES_PER_GROUP; i++) {
            pteg1[i].pte0 =
                PPCInterpreter::MMURead64(ppuState, pteg1Addr + i * 16);
            pteg1[i].pte1 =
                PPCInterpreter::MMURead64(ppuState, pteg1Addr + i * 16 + 8);
          }

          // We compare all pte's in order for simplicity.
          for (size_t i = 0; i < PPC_HPTES_PER_GROUP; i++) {
            /*
                Conditions for a match to occur:

                * PTE: H = 0 for the primary PTEG, 1 for the secondary PTEG
                * PTE: V = 1
                * PTE: AVPN[0:51] = VA0:51
                * if p < 28, PTE: AVPN[52:51+q] = VA[52:51+q]
            */

            // H = 0?
            if (((pteg1[i].pte0 & PPC_HPTE64_HASH) >> 1) != 0) {
              continue;
            }

            // V?
            if ((pteg1[i].pte0 & PPC_HPTE64_VALID) != true) {
              continue;
            }

            u64 pteAVPN = pteg1[i].pte0 & PPC_HPTE64_AVPN;

            // PTE:AVPN[0:51] = VA[0:51]?
            if (((pteAVPN & PPC_HPTE64_AVPN_0_51) >> 12) != (VA >> 28)) {
              continue;
            }

            /*
                The PPE also requires that PTE[LP] = SLBE[LP] whenever PTE[L] =
               '1'. In other words, the PTE page size must match the SLBE page
               size exactly for a page-table match to occur.
            */

            // L?
            if (static_cast<u8>((pteg1[i].pte0 & PPC_HPTE64_LARGE) >> 2) != static_cast<u8>(L)) {
              continue;
            }

            // L & LP?
            if (L && static_cast<u8>((pteg1[i].pte1 & PPC_HPTE64_LP) >> 12) != LP) {
              continue;
            }

            // Match found. Extract the RPN from the page table.

            // Set relocation back to whatever it was.
            curThread.SPR.MSR.DR = msrDR;
            curThread.SPR.MSR.IR = msrIR;

            // Update Referenced and Change Bits if necessary.
            if (!((pteg1[i].pte1 & PPC_HPTE64_R) >> 8)) {
              // Referenced
              MMUWrite64(ppuState, pteg1Addr + i * 16 + 8,
                         (pteg1[i].pte1 | 0x100));
            }
            if (!((pteg1[i].pte1 & PPC_HPTE64_C) >> 7)) {
              // Access is a data write?
              if (memWrite) {
                // Change
                MMUWrite64(ppuState, pteg1Addr + i * 16 + 8,
                           (pteg1[i].pte1 | 0x80));
              }
            }

            if (L) {
              // RPN is PTE[86:114].
              RPN = pteg1[i].pte1 & PPC_HPTE64_RPN_LP;
            } else {
              // RPN is PTE[86:115].
              RPN = pteg1[i].pte1 & PPC_HPTE64_RPN_NO_LP;
            }

            goto end;
          }

          // Set MSR to IR/DR mode before raising the interrupt to whatever they
          // were.
          curThread.SPR.MSR.DR = msrDR;
          curThread.SPR.MSR.IR = msrIR;

          // Page Table Lookup Fault.
          // Issue Data/Instr Storage interrupt.

          // Instruction read.
          if (curThread.iFetch) {
            curThread.exceptReg |= PPU_EX_INSSTOR;

          } else if (memWrite) {
            // Data write.
            curThread.exceptReg |= PPU_EX_DATASTOR;
            curThread.SPR.DAR = *EA;
            curThread.SPR.DSISR = DSISR_NOPTE | DSISR_ISSTORE;
          } else {
            // Data read.
            curThread.exceptReg |= PPU_EX_DATASTOR;
            curThread.SPR.DAR = *EA;
            curThread.SPR.DSISR = DSISR_NOPTE;
          }
          return false;
        }
      }
    } else {
      // SLB Miss
      // Data or Inst Segment Exception
      if (curThread.iFetch) {
        curThread.exceptReg |= PPU_EX_INSTSEGM;
      } else {
        curThread.exceptReg |= PPU_EX_DATASEGM;
        curThread.SPR.DAR = *EA;
      }
      return false;
    }

  end:
    RA = (RPN | QGET(*EA, 64 - p, 63));
    // Real Address 0 - 21 bits are not implemented;
    QSET(RA, 0, 21, 0)
  }

  // Save in ERAT's
  if (curThread.iFetch) {
    // iERAT
    curThread.iERAT.putElement((*EA & ~0xFFF), (RA & ~0xFFF));
  }
  else {
    // dERAT
    curThread.dERAT.putElement((*EA & ~0xFFF), (RA & ~0xFFF));
  }

  *EA = RA;
  return true;
}

// MMU Read Routine, used by the CPU
u64 PPCInterpreter::MMURead(XENON_CONTEXT *cpuContext, PPU_STATE *ppuState,
                            u64 EA, s8 byteCount) {
  u64 data = 0;
  u64 oldEA = EA;

  if (EA == 0xc000000000022794) {
    u8 a = 0;
  }

  // Exception ocurred?
  if (MMUTranslateAddress(&EA, ppuState, false) == false)
    return 0;

  bool socRead = false;

  EA = mmuContructEndAddressFromSecEngAddr(EA, &socRead);

  // When the xboxkrnl writes to address 0x7FFFxxxx is writing to the IIC
  // so we use that address here to validate its an soc write.
  if (((oldEA & 0x000000007fff0000) >> 16) == 0x7FFF) {
    socRead = true;
  }

  // TODO: Investigate this values FSB_CONFIG_RX_STATE - Needed to Work!
  if (curThread.CIA == 0x1003598) {
    GPR(11) = 0x0E;
  }

  if (curThread.CIA == 0x1003644) {
    GPR(11) = 0x02;
  }

  // Hack Needed for CB to work, seems like it reads from some SoC this value
  // and checks againts the fuses.
  if (socRead && EA == 0x00061000) {
    data = 0x0000000000000020;
    return data;
  }

  // Hack Needed for CB to work, reading ram size to this address, further
  // research required. Free60.org shows this belongs to BIU address range.
  if (socRead && EA == 0xE1040000) {
    data = 0x0000000020000000;
    return data;
  }

  // Hardware register address
  if (socRead && EA == 0x00061008) {
    data = 0;
    return data;
  }

  // SECENG address, CB compares this to 0.
  // Further research required!
  if ((socRead && EA == 0x26000) || (socRead && EA == 0x26008)) {
    data = 0;
    return data;
  }

  // Check if reading from Security Engine config block
  if (socRead && EA >= XE_SECENG_ADDR && EA < XE_SECENG_ADDR + XE_SECENG_SIZE) {
    u32 secAddr = static_cast<u32>(EA - XE_SECENG_ADDR);
    memcpy(&data, &intXCPUContext->secEngData[secAddr], byteCount);
    return data;
  }

  // Random Number Generator
  if (socRead && EA == 0x00060000) {
    u64 generatedRandomNumber = rand();
    memcpy(&data, &generatedRandomNumber, byteCount);
    return data;
  }

  // Read is from SROM area:
  if (socRead && EA >= XE_SROM_ADDR && EA < XE_SROM_ADDR + XE_SROM_SIZE) {
    u32 sromAddr = static_cast<u32>(EA) - static_cast<u32>(XE_SROM_ADDR);
    memcpy(&data, &cpuContext->SROM[sromAddr], byteCount);
    return data;
  }

  // Read is from SRAM area
  if (socRead && EA >= XE_SRAM_ADDR && EA < XE_SRAM_ADDR + XE_SRAM_SIZE) {
    u32 sramAddr = static_cast<u32>(EA) - static_cast<u32>(XE_SRAM_ADDR);
    memcpy(&data, &cpuContext->SRAM[sramAddr], byteCount);
    return data;
  }

  // Check if reading from eFuses section
  if (socRead && EA >= XE_FUSESET_LOC &&
      EA <= (XE_FUSESET_LOC + XE_FUSESET_SIZE)) {
    switch (static_cast<u32>(EA)) {
    case 0x20000:
      data = cpuContext->fuseSet.fuseLine00;
      break;
    case 0x20200:
      data = cpuContext->fuseSet.fuseLine01;
      break;
    case 0x20400:
      data = cpuContext->fuseSet.fuseLine02;
      break;
    case 0x20600:
      data = cpuContext->fuseSet.fuseLine03;
      break;
    case 0x20800:
      data = cpuContext->fuseSet.fuseLine04;
      break;
    case 0x20a00:
      data = cpuContext->fuseSet.fuseLine05;
      break;
    case 0x20c00:
      data = cpuContext->fuseSet.fuseLine06;
      break;
    case 0x20e00:
      data = cpuContext->fuseSet.fuseLine07;
      break;
    case 0x21000:
      data = cpuContext->fuseSet.fuseLine08;
      break;
    case 0x21200:
      data = cpuContext->fuseSet.fuseLine09;
      break;
    case 0x21400:
      data = cpuContext->fuseSet.fuseLine10;
      break;
    case 0x21600:
      data = cpuContext->fuseSet.fuseLine11;
      break;
    default:
      LOG_ERROR(Xenon_MMU, "Reading to unknown fuse at address {:#x}", EA);
      break;
    }

    return byteswap<u64>(data);
  }

  // Integrated Interrupt Controller in real mode, used when the HV wants to
  // start a CPUs IC.
  if (socRead && (EA & ~0xF000) >= XE_IIC_BASE &&
      (EA & ~0xF000) < XE_IIC_BASE + XE_IIC_SIZE && (EA & 0xFFFFF) < 0x56000) {
    intXCPUContext->xenonIIC.readInterrupt(EA, &data);
    return data;
  }

  // Time Base register. Writing here starts or stops the RTC apparently.
  if (socRead && EA == 0x611A0) {
    if (!intXCPUContext->timeBaseActive) {
      data = 0;
      return data;
    } else {
      data = 0x0001000000000000;
      return data;
    }
  }

  // CPU VID Register
  if (socRead && EA == 0x61188) {
    data = 0x382C00000000b001;
    return byteswap<u64>(data);
  }

  bool nand = false;
  bool pciConfigSpace = false;
  bool pciBridge = false;
  bool xGPU = false;

  if (EA >= 0xC8000000 && EA <= 0xCC000000) {
    nand = true;
  }

  if (EA >= 0xD0000000 && EA <= 0xD1000000) {
    pciConfigSpace = true;
  }

  if (EA >= 0xEA000000 && EA <= 0xEA010000) {
    pciBridge = true;
  }

  if (EA >= 0xEC800000 && EA <= 0xEC810000) {
      xGPU = true;
  }

  if (socRead && nand != true && pciBridge != true && pciConfigSpace != true && xGPU != true) {
      LOG_WARNING(Xenon_MMU, "SoC Read from {:#x}, returning 0.", EA);
    data = 0;
    return data;
  }

  if (EA == 0x22794) {
    u8 a = 0;
  }

  // External Read
  sysBus->Read(EA, &data, byteCount);
  return data;
}

// MMU Write Routine, used by the CPU
void PPCInterpreter::MMUWrite(XENON_CONTEXT *cpuContext, PPU_STATE *ppuState,
                              u64 data, u64 EA, s8 byteCount, bool cacheStore) {
  u64 oldEA = EA;
  if (MMUTranslateAddress(&EA, ppuState, true) == false)
    return;

  if (oldEA >= 0x9E000000 && oldEA <= 0x9EFFFFFF) {
    u8 a = 0;
  }

  bool socWrite = false;

  EA = mmuContructEndAddressFromSecEngAddr(EA, &socWrite);

  // When the xboxkrnl writes to address 0x7FFFxxxx is writing to the IIC
  // so we use that address here to validate its an soc write.
  if (((oldEA & 0x000000007FFFF0000) >> 16) == 0x7FFF) {
    socWrite = true;
  }

  // CPU POST Bus
  if (socWrite && EA == POST_BUS_ADDR) {
    Xe::XCPU::POSTBUS::POST(data);
    return;
  }

  // Time Base register. Writing here starts or stops the RTC apparently.
  if (socWrite && EA == 0x611A0) {
    if (data == 0) {
      intXCPUContext->timeBaseActive = false;
      return;
    } else if (data == 0xFF01000000000000 ||
               data == 0x0001000000000000) // 0x1FF byte reversed!
    {
      intXCPUContext->timeBaseActive = true;
      return;
    }
  }

  // CPU VID Register
  if (socWrite && EA == 0x61188) {
    if (data != 0) {
      LOG_WARNING(Xenon, "(SOC): New VID value being set: {:#x}", data);
    }
    return;
  }

  // Check if writing to bootloader section
  if (socWrite && EA >= XE_SROM_ADDR && EA < XE_SROM_ADDR + XE_SROM_SIZE) {
    LOG_ERROR(Xenon_MMU, "Tried to write to XCPU SROM!");
    return;
  }

  // Check if writing to internal SRAM
  if (socWrite && EA >= XE_SRAM_ADDR && EA < XE_SRAM_ADDR + XE_SRAM_SIZE) {
    u32 sramAddr = static_cast<u32>(EA - XE_SRAM_ADDR);
    memcpy(&cpuContext->SRAM[sramAddr], &data, byteCount);
    return;
  }

  // Check if writing to Security Engine Config Block
  if (socWrite && EA >= XE_SECENG_ADDR &&
      EA < XE_SECENG_ADDR + XE_SECENG_SIZE) {
    u32 secAddr = static_cast<u32>(EA - XE_SECENG_ADDR);
    memcpy(&intXCPUContext->secEngData[secAddr], &data, byteCount);
    return;
  }

  // Integrated Interrupt Controller in real mode, used when the HV wants to
  // start a CPUs IC.
  if (socWrite && (EA & ~0xF000) >= XE_IIC_BASE &&
      (EA & ~0xF000) < XE_IIC_BASE + XE_IIC_SIZE && (EA & 0xFFFFF) < 0x560FF) {
    intXCPUContext->xenonIIC.writeInterrupt(EA, data);
    return;
  }

  bool nand = false;
  bool pciConfigSpace = false;
  bool pciBridge = false;
  bool xGPU = false;

  if (EA >= 0xC8000000 && EA <= 0xCC000000) {
    nand = true;
  }

  if (EA >= 0xD0000000 && EA <= 0xD1000000) {
    pciConfigSpace = true;
  }

  if (EA >= 0xEA000000 && EA <= 0xEA010000) {
    pciBridge = true;
  }

  if (EA >= 0xEC800000 && EA <= 0xEC810000) {
      xGPU = true;
  }

  if (socWrite && nand != true && pciBridge != true && pciConfigSpace != true && xGPU != true) {
    LOG_WARNING(Xenon_MMU, "SoC Write to {:#x}, data = {:#x}, invalidating.", EA, data);
    return;
  }

  // External Write
  sysBus->Write(EA, data, byteCount);

  intXCPUContext->xenonRes.Check(EA);
}

void PPCInterpreter::MMUMemCpyFromHost(PPU_STATE *ppuState,
                                       u32 dest, const void* source, u64 size, bool cacheStore) {
  for (u64 i = 0; i != size; ++i) {
    MMUWrite(intXCPUContext, ppuState, reinterpret_cast<const u8*>(source)[i], dest + i, 1, cacheStore);
  }
}

void PPCInterpreter::MMUMemCpy(PPU_STATE *ppuState,
                               u32 dest, u32 source, u64 size, bool cacheStore) {
  for (u64 i = 0; i != size; ++i) {
    MMUWrite(intXCPUContext, ppuState, MMURead(intXCPUContext, ppuState, source + i, 1), dest + i, 1, cacheStore);
  }
}

// Reads 1 Byte of memory.
u8 PPCInterpreter::MMURead8(PPU_STATE *ppuState, u64 EA) {
  u8 data = 0;
  data = static_cast<u8>(MMURead(intXCPUContext, ppuState, EA, 1));
  return data;
}
// Reads 2 Bytes of memory.
u16 PPCInterpreter::MMURead16(PPU_STATE *ppuState, u64 EA) {
  u16 data = 0;
  data = static_cast<u16>(MMURead(intXCPUContext, ppuState, EA, 2));
  return byteswap<u16>(data);
}
// Reads 4 Bytes of memory.
u32 PPCInterpreter::MMURead32(PPU_STATE *ppuState, u64 EA) {
  u32 data = 0;
  data = static_cast<u32>(MMURead(intXCPUContext, ppuState, EA, 4));
  return byteswap<u32>(data);
}
// Reads 8 Bytes of memory.
u64 PPCInterpreter::MMURead64(PPU_STATE *ppuState, u64 EA) {
  u64 data = 0;
  data = MMURead(intXCPUContext, ppuState, EA, 8);
  return byteswap<u64>(data);
}
// Writes 1 Byte to memory.
void PPCInterpreter::MMUWrite8(PPU_STATE *ppuState, u64 EA, u8 data) {
  MMUWrite(intXCPUContext, ppuState, data, EA, 1);
}
// Writes 2 Bytes to memory.
void PPCInterpreter::MMUWrite16(PPU_STATE *ppuState, u64 EA, u16 data) {
  u16 dataBS = byteswap<u16>(data);
  MMUWrite(intXCPUContext, ppuState, dataBS, EA, 2);
}
// Writes 4 Bytes to memory.
void PPCInterpreter::MMUWrite32(PPU_STATE *ppuState, u64 EA, u32 data) {
  u32 dataBS = byteswap<u32>(data);
  MMUWrite(intXCPUContext, ppuState, dataBS, EA, 4);
}
// Writes 8 Bytes to memory.
void PPCInterpreter::MMUWrite64(PPU_STATE *ppuState, u64 EA, u64 data) {
  u64 dataBS = byteswap<u64>(data);
  MMUWrite(intXCPUContext, ppuState, dataBS, EA, 8);
}
