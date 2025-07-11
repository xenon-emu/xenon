// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include <bit>
#include <memory>
#include <unordered_map>

#include "Base/Bitfield.h"
#include "Base/LRUCache.h"
#include "Base/Vector128.h"
#include "Core/XCPU/XenonSOC.h"
#include "Core/XCPU/IIC/IIC.h"
#include "Core/XCPU/XenonReservations.h"
#include "Core/XCPU/eFuse.h"

// PowerPC Opcode definitions
/*
* All original authors of the rpcs3 PPU_Decoder and PPU_Opcodes maintain their original copyright.
* Modifed for usage in the Xenon Emulator
* All rights reserved
* License: GPL2
*/
template<typename T, u32 I, u32 N>
using PPCBitfield = bitfield<T, sizeof(T) * 8 - N - I, N>;
union PPCOpcode {
  u32 opcode;
  PPCBitfield<u32, 0, 6> main; // 0..5
  ControlField<PPCBitfield<u32, 30, 1>, PPCBitfield<u32, 16, 5>> sh64; // 30 + 16..20
  ControlField<PPCBitfield<u32, 26, 1>, PPCBitfield<u32, 21, 5>> mbe64; // 26 + 21..25
  PPCBitfield<u32, 11, 5> vuimm; // 11..15
  PPCBitfield<u32, 6, 5> vs; // 6..10
  PPCBitfield<u32, 22, 4> vsh; // 22..25
  PPCBitfield<u32, 21, 1> oe; // 21
  PPCBitfield<u32, 11, 10> spr; // 11..20
  PPCBitfield<u32, 21, 5> vc; // 21..25
  PPCBitfield<u32, 21, 11> xo; // 21..30
  PPCBitfield<u32, 16, 5> vb; // 16..20
  PPCBitfield<u32, 11, 5> va; // 11..15
  PPCBitfield<u32, 6, 5> vd; // 6..10
  PPCBitfield<u32, 31, 1> lk; // 31
  PPCBitfield<u32, 30, 1> aa; // 30
  PPCBitfield<u32, 16, 5> rb; // 16..20
  PPCBitfield<u32, 11, 5> ra; // 11..15
  PPCBitfield<u32, 6, 5> rd; // 6..10
  PPCBitfield<u32, 16, 16> uimm16; // 16..31
  PPCBitfield<u32, 11, 1> l11; // 11
  PPCBitfield<u32, 6, 5> rs; // 6..10
  PPCBitfield<s32, 16, 16> simm16; // 16..31, signed
  PPCBitfield<s32, 16, 14> ds; // 16..29, signed
  PPCBitfield<s32, 11, 5> vsimm; // 11..15, signed
  PPCBitfield<s32, 6, 26> ll; // 6..31, signed
  PPCBitfield<s32, 6, 24> li; // 6..29, signed
  PPCBitfield<u32, 20, 7> lev; // 20..26
  PPCBitfield<u32, 16, 4> i; // 16..19
  PPCBitfield<u32, 11, 3> crfs; // 11..13
  PPCBitfield<u32, 10, 1> l10; // 10
  PPCBitfield<u32, 6, 3> crfd; // 6..8
  PPCBitfield<u32, 16, 5> crbb; // 16..20
  PPCBitfield<u32, 11, 5> crba; // 11..15
  PPCBitfield<u32, 6, 5> crbd; // 6..10
  PPCBitfield<u32, 21, 1> vrc; // 31
  PPCBitfield<u32, 31, 1> rc; // 31
  PPCBitfield<u32, 26, 5> me32; // 26..30
  PPCBitfield<u32, 21, 5> mb32; // 21..25
  PPCBitfield<u32, 16, 5> sh32; // 16..20
  PPCBitfield<u32, 11, 5> bi; // 11..15
  PPCBitfield<u32, 6, 5> bo; // 6..10
  PPCBitfield<u32, 19, 2> bh; // 19..20
  PPCBitfield<u32, 21, 5> frc; // 21..25
  PPCBitfield<u32, 16, 5> frb; // 16..20
  PPCBitfield<u32, 11, 5> fra; // 11..15
  PPCBitfield<u32, 6, 5> frd; // 6..10
  PPCBitfield<u32, 12, 8> crm; // 12..19
  PPCBitfield<u32, 6, 5> frs; // 6..10
  PPCBitfield<u32, 7, 8> flm; // 7..14
  PPCBitfield<u32, 6, 1> l6; // 6
  PPCBitfield<u32, 15, 1> l15; // 15
  ControlField<PPCBitfield<s32, 16, 14>, FixedField<u32, 0, 2>> bt14;
  ControlField<PPCBitfield<s32, 6, 24>, FixedField<u32, 0, 2>> bt24;
  
  // VMX128 Bitfields.

  // VD128 = VD128l | (VD128h << 5)
  // VA128 = VA128l | (VA128h << 5) | (VA128H << 6)
  // VB128 = VB128l | (VB128h << 5)
  struct {
    u32 VB128h : 2;
    u32 VD128h : 2;
    u32 : 1;
    u32 VA128h : 1;
    u32 : 4;
    u32 VA128H : 1;
    u32 VB128l : 5;
    u32 VA128l : 5;
    u32 VD128l : 5;
    u32 : 6;
  } VMX128;
  // VD128 = VD128l | (VD128h << 5)
  struct {
    u32 : 2;
    u32 VD128h : 2;
    u32 : 7;
    u32 RB : 5;
    u32 RA : 5;
    u32 VD128l : 5;
    u32 : 6;
  } VMX128_1;
  // VD128 = VD128l | (VD128h << 5)
  // VA128 = VA128l | (VA128h << 5) | (VA128H << 6)
  // VB128 = VB128l | (VB128h << 5)
  struct {
    u32 VB128h : 2;
    u32 VD128h : 2;
    u32 : 1;
    u32 VA128h : 1;
    u32 VC : 3;
    u32 : 1;
    u32 VA128H : 1;
    u32 VB128l : 5;
    u32 VA128l : 5;
    u32 VD128l : 5;
    u32 : 6;
  } VMX128_2;
  // VD128 = VD128l | (VD128h << 5)
  // VB128 = VB128l | (VB128h << 5)
  struct {
    u32 VB128h : 2;
    u32 VD128h : 2;
    u32 : 7;
    u32 VB128l : 5;
    u32 IMM : 5;
    u32 VD128l : 5;
    u32 : 6;
  } VMX128_3;
  // VD128 = VD128l | (VD128h << 5)
  // VB128 = VB128l | (VB128h << 5)
  struct {
    u32 VB128h : 2;
    u32 VD128h : 2;
    u32 : 2;
    u32 z : 2;
    u32 : 3;
    u32 VB128l : 5;
    u32 IMM : 5;
    u32 VD128l : 5;
    u32 : 6;
  } VMX128_4;
  // VD128 = VD128l | (VD128h << 5)
  // VA128 = VA128l | (VA128h << 5) | (VA128H << 6)
  // VB128 = VB128l | (VB128h << 5)
  struct {
    u32 VB128h : 2;
    u32 VD128h : 2;
    u32 : 1;
    u32 VA128h : 1;
    u32 SH : 4;
    u32 VA128H : 1;
    u32 VB128l : 5;
    u32 VA128l : 5;
    u32 VD128l : 5;
    u32 : 6;
  } VMX128_5;
  // VD128 = VD128l | (VD128h << 5)
  // VB128 = VB128l | (VB128h << 5)
  // PERM = PERMl | (PERMh << 5)
  struct {
    u32 VB128h : 2;
    u32 VD128h : 2;
    u32 : 2;
    u32 PERMh : 3;
    u32 : 2;
    u32 VB128l : 5;
    u32 PERMl : 5;
    u32 VD128l : 5;
    u32 : 6;
  } VMX128_P;
  // VD128 = VD128l | (VD128h << 5)
  // VA128 = VA128l | (VA128h << 5) | (VA128H << 6)
  // VB128 = VB128l | (VB128h << 5)
  struct {
    u32 VB128h : 2;
    u32 VD128h : 2;
    u32 : 1;
    u32 VA128h : 1;
    u32 Rc : 1;
    u32 : 3;
    u32 VA128H : 1;
    u32 VB128l : 5;
    u32 VA128l : 5;
    u32 VD128l : 5;
    u32 : 6;
  } VMX128_R;
};

//
// PowerPC Register definitions
//

/*
Condition Register

The CR fields can be set in one of the following ways:
* Specified fields of the CR can be set from a GPR by using the mtcrf and mtocrf
instruction. * The contents of the XER[0-3] can be moved to another CR field by
using the mcrf instruction. * A specified field of the XER can be copied to a
specified field of the CR by using the mcrxr instruction. * A specified field of
the FPSCR can be copied to a specified field of the CR by using the mcrfs
instruction. * Logical instructions of the condition register can be used to
perform logical operations on specified bits in the condition register. * CR0
can be the implicit result of an integer instruction. * CR1 can be the implicit
result of a floating-point instruction. * A specified CR field can indicate the
result of either an integer or floating-point compare instruction
*/
union CRegister {
  u32 CR_Hex;
  // Individual Bit access.
  u8 bits[32];

  u8& operator [](size_t bitIdx) { return bits[bitIdx]; }

  struct {
    u32 CR7 : 4;
    u32 CR6 : 4;
    u32 CR5 : 4;
    u32 CR4 : 4;
    u32 CR3 : 4;
    u32 CR2 : 4;
    u32 CR1 : 4;
    u32 CR0 : 4;
  };
};

// Rounding modes useb by FPSCR's RN bits. They specify rounding mode regarding several operations
// inside the FPU.
// See PPC Assembly IBM Programming Environment Manual, 2.3, page 109, table 3-8.
enum eFPRoundMode : u32 {
  roundModeNearest = 0,
  roundModeTowardZero = 1,
  roundModePlusInfinity = 2,
  roundModeNegativeInfinity = 3
};

/*
Floating-Point Status and Control Register (FPSCR)
*/
union FPSCRegister {
  // Rounding mode (towards: nearest, zero, +inf, -inf)
  PPCBitfield<eFPRoundMode, 30, 2> RN;
  // Non-IEEE mode enable (aka flush-to-zero)
  PPCBitfield<u32, 29, 1> NI;
  // Inexact exception enable
  PPCBitfield<u32, 28, 1> XE;
  // IEEE division by zero exception enable
  PPCBitfield<u32, 27, 1> ZE;
  // IEEE underflow exception enable
  PPCBitfield<u32, 26, 1> UE;
  // IEEE overflow exception enable
  PPCBitfield<u32, 25, 1> OE;
  // Invalid operation exception enable
  PPCBitfield<u32, 24, 1> VE;
  // Invalid operation exception for integer conversion (sticky)
  PPCBitfield<u32, 23, 1> VXCVI;
  // Invalid operation exception for square root (sticky)
  PPCBitfield<u32, 22, 1> VXSQRT;
  // Invalid operation exception for software request (sticky)
  PPCBitfield<u32, 21, 1> VXSOFT;
  // reserved
  PPCBitfield<u32, 20, 1> R0;
  // Floating point result flags (includes FPCC) (not sticky)
  // from more to less significand: class, <, >, =, ?
  PPCBitfield<u32, 15, 5> FPRF;
  PPCBitfield<u32, 15, 1> C; // Floating-point result class descriptor (C)
  PPCBitfield<u32, 16, 1> FL; // Floating-point less than or negative
  PPCBitfield<u32, 17, 1> FG; // Floating-point greater than or positive
  PPCBitfield<u32, 18, 1> FE; // Floating-point equal or zero
  PPCBitfield<u32, 19, 1> FU; // Floating-point unordered or NaN 
  // Fraction inexact (not sticky)
  PPCBitfield<u32, 14, 1> FI;
  // Fraction rounded (not sticky)
  PPCBitfield<u32, 13, 1> FR;
  // Invalid operation exception for invalid comparison (sticky)
  PPCBitfield<u32, 12, 1> VXVC;
  // Invalid operation exception for inf * 0 (sticky)
  PPCBitfield<u32, 11, 1> VXIMZ;
  // Invalid operation exception for 0 / 0 (sticky)
  PPCBitfield<u32, 10, 1> VXZDZ;
  // Invalid operation exception for inf / inf (sticky)
  PPCBitfield<u32, 9, 1> VXIDI;
  // Invalid operation exception for inf - inf (sticky)
  PPCBitfield<u32, 8, 1> VXISI;
  // Invalid operation exception for SNaN (sticky)
  PPCBitfield<u32, 7, 1> VXSNAN;
  // Inexact exception (sticky)
  PPCBitfield<u32, 6, 1> XX;
  // Division by zero exception (sticky)
  PPCBitfield<u32, 5, 1> ZX;
  // Underflow exception (sticky)
  PPCBitfield<u32, 4, 1> UX;
  // Overflow exception (sticky)
  PPCBitfield<u32, 3, 1> OX;
  // Invalid operation exception summary (not sticky)
  PPCBitfield<u32, 2, 1> VX;
  // Enabled exception summary (not sticky)
  PPCBitfield<u32, 1, 1> FEX;
  // Exception summary (sticky)
  PPCBitfield<u32, 0, 1> FX;

  u8 bits[32];

  u32 FPSCR_Hex = 0;

  // FPSCR's bit 20 is defined as reserved and set to zero. Attempts to modify it
  // are ignored by hardware, we set a mask in order to mimic this.
  static constexpr u32 fpscrMask = 0xFFFFF7FF;

  FPSCRegister() = default;
  explicit FPSCRegister(u32 hexValue) : FPSCR_Hex{ hexValue & fpscrMask } {}

  u8& operator[](u8 bitIdx) { return bits[bitIdx]; }

  FPSCRegister& operator=(u32 inValue) { FPSCR_Hex = inValue & fpscrMask; return *this; }

  FPSCRegister& operator|=(u32 inValue) { FPSCR_Hex |= inValue & fpscrMask; return *this; }

  FPSCRegister& operator&=(u32 inValue) { FPSCR_Hex &= inValue; return *this; }

  FPSCRegister& operator^=(u32 inValue) { FPSCR_Hex ^= inValue & fpscrMask; return *this; }

  // Clears both Fraction Inexact and Fraction Rounded bits.
  void clearFIFR() { FI = 0; FR = 0; }
};

/*
 XER Register (XER)
*/
union XERegister {
  u32 XER_Hex;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 ByteCount : 7;
    u32 R0 : 22;
    u32 CA : 1;
    u32 OV : 1;
    u32 SO : 1;
  };
#else
  struct {
    u32 SO : 1;
    u32 OV : 1;
    u32 CA : 1;
    u32 R0 : 22;
    u32 ByteCount : 7;
  };
#endif
};

/*
Time Base (TB)
*/
union TB {
  u64 TB_Hex;
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 TBL : 32;
    u64 TBU : 32;
  };
#else
  struct {
    u64 TBU : 32;
    u64 TBL : 32;
  };
#endif
};

/*
Machine State Register (MSR)
*/
union MSRegister {
  u64 MSR_Hex;
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 LE : 1;
    u64 RI : 1;
    u64 PMM : 1;
    u64 : 1;
    u64 DR : 1;
    u64 IR : 1;
    u64 : 2;
    u64 FE1 : 1;
    u64 BE : 1;
    u64 SE : 1;
    u64 FE0 : 1;
    u64 ME : 1;
    u64 FP : 1;
    u64 PR : 1;
    u64 EE : 1;
    u64 ILE : 1;
    u64 : 8;
    u64 VXU : 1;
    u64 : 34;
    u64 HV : 1;
    u64 : 1;
    u64 TA : 1;
    u64 SF : 1;
  };
#else
  struct {
    u64 SF : 1;
    u64 TA : 1;
    u64 : 1;
    u64 HV : 1;
    u64 : 34;
    u64 VXU : 1;
    u64 : 8;
    u64 ILE : 1;
    u64 EE : 1;
    u64 PR : 1;
    u64 FP : 1;
    u64 ME : 1;
    u64 FE0 : 1;
    u64 SE : 1;
    u64 BE : 1;
    u64 FE1 : 1;
    u64 : 2;
    u64 IR : 1;
    u64 DR : 1;
    u64 : 1;
    u64 PMM : 1;
    u64 RI : 1;
    u64 LE : 1;
  };
#endif
};

/*
Processor Version Register (PVR)
*/
union PVRegister {
  u32 PVR_Hex;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 Revision : 16;
    u32 Version : 16;
  };
#else
  struct {
    u32 Version : 16;
    u32 Revision : 16;
  };
#endif
};

// Vector Status and Control Register
union VSCRegister {
  u32 hexValue;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 SAT : 1;
    u32 res0 : 15;
    u32 NJ : 1;
    u32 res1 : 15;
  };
#else
  struct {
    u32 res1 : 15;
    u32 NJ : 1;
    u32 res0 : 15;
    u32 SAT : 1;
  };
#endif// __LITTLE_ENDIAN__
};

// Segment Lookaside Buffer Entry
struct SLBEntry {
  u8 V;
  u8 LP; // Large Page selector
  u8 C;
  u8 L;
  u8 N;
  u8 Kp;
  u8 Ks;
  u64 VSID;
  u64 ESID;
  u64 vsidReg;
  u64 esidReg;
};

// Traslation lookaside buffer.
// Holds a cache of the recently used PTE's.
struct TLBEntry {
  bool V;   // Entry valid.
  u64 VPN;  // Pre calculated VPN. We do this ahead of time for performance reasons
            // and because of the possible use of the LVPN.
  u64 pte0; // Holds the valid bit, as well as the AVPN
  u64 pte1; // Contains the RPN.
};
struct TLB_Reg {
  TLBEntry tlbSet0[256];
  TLBEntry tlbSet1[256];
  TLBEntry tlbSet2[256];
  TLBEntry tlbSet3[256];
};

// This SPR's are duplicated for every thread.
struct PPU_THREAD_SPRS {
  // Fixed Point Exception Register (XER)
  XERegister XER;
  // Link Register
  u64 LR;
  // Count Register
  u64 CTR;
  // CFAR: I dont know the definition.
  u64 CFAR;
  // VXU Register Save.
  u32 VRSAVE;
  // Data Storage Interrupt Status Register
  u64 DSISR;
  // Data Address Register
  u64 DAR;
  // Decrementer Register
  // The contents of the Decrementer are treated as a signed integer.
  s32 DEC;
  // Machine Status Save/Restore Register 0
  u64 SRR0;
  // Machine Status Save/Restore Register 1
  u64 SRR1;
  // Address Compare Control Register
  u64 ACCR;
  // Software Use Special Purpose Register 0 - 3
  u64 SPRG0;
  u64 SPRG1;
  u64 SPRG2;
  u64 SPRG3;
  // Hypervisor Software Use Special Purpose Register 0
  u64 HSPRG0;
  // Hypervisor Software Use Special Purpose Register 1
  u64 HSPRG1;
  // Hypervisor Machine Status Save/Restore Register 0
  u64 HSRR0;
  // Hypervisor Machine Status Save/Restore Register 1
  u64 HSRR1;
  // Thread Status Register Local (TSRL)
  u64 TSRL;
  // Thread Status Register Remote (TSRR)
  u64 TSSR;
  // PPE Translation Lookaside Buffer Index Hint Register
  u64 PPE_TLB_Index_Hint;
  // Data Address Breakpoint
  u64 DABR;
  // Data Address Breakpoint Extension
  u64 DABRX;
  // Machine-State Register
  MSRegister MSR;
  // Processor Identification Register
  u32 PIR;
};
// This contains SPR's that are shared by both threads.
struct PPU_STATE_SPRS {
  // Storage Description Register 1
  u64 SDR1;
  // Control Register
  u32 CTRL;
  // Time Base
  u64 TB;
  // Processor Version Register
  PVRegister PVR;
  // Hypervisor Decrementer
  u32 HDEC;
  // Real Mode Offset Register
  u64 RMOR;
  // Hypervisor Real Mode Offset Register
  u64 HRMOR;
  // Logical Partition Control Register (This register has partially shared fields)
  // FIXME!
  u64 LPCR;
  // Logical Partition Identity Register
  u32 LPIDR;
  // Thread Switch Control Register
  u32 TSCR;
  // Thread Switch Timeout Register
  u64 TTR;
  // Translation Lookaside Buffer Index Register
  u64 PPE_TLB_Index;
  // Translation Lookaside Buffer Virtual-Page Number Register
  u64 PPE_TLB_VPN;
  // Translation Lookaside Buffer Real-Page Number Register
  u64 PPE_TLB_RPN;
  // Translation Lookaside Buffer RMT Register
  u64 PPE_TLB_RMT;
  // Hardware Implementation Register 0
  u64 HID0;
  // Hardware Implementation Register 1
  u64 HID1;
  // Hardware Implementation Register 4
  u64 HID4;
  // Hardware Implementation Register 6
  u64 HID6;
};

// Thread IDs for ease of handling
enum ePPUThread : u8 {
  ePPUThread_Zero = 0,
  ePPUThread_One,
  ePPUThread_None
};
enum ePPUThreadBit : u8 {
  ePPUThreadBit_None = 0,
  ePPUThreadBit_Zero,
  ePPUThreadBit_One
};

//
// Security Engine Related Structures
//

enum SECENG_REGION_TYPE {
  SECENG_REGION_PHYS = 0,
  SECENG_REGION_HASHED = 1,
  SECENG_REGION_SOC = 2,
  SECENG_REGION_ENCRYPTED = 3
};

struct SECENG_ADDRESS_INFO {
  // Real address we're accessing on the bus.
  u32 accessedAddr;
  // Region This address belongs to.
  SECENG_REGION_TYPE regionType;
  // Key used to hash/encrypt this address.
  u8 keySelected;
};

#define XE_RESET_VECTOR 0x100
#define XE_SROM_ADDR 0x0
#define XE_SROM_SIZE 0x8000
#define XE_SRAM_ADDR 0x10000
#define XE_SRAM_SIZE 0x10000
#define XE_FUSESET_LOC 0x20000
#define XE_FUSESET_SIZE 0x17FF
#define XE_L2_CACHE_SIZE 0x100000
// Corona: 0x00710800
// Jasper: 0x00710500
#define XE_PVR 0x00710500

// Exception Bitmasks for Exception Register

#define PPU_EX_NONE 0x0
#define PPU_EX_RESET 0x1
#define PPU_EX_MC 0x2
#define PPU_EX_DATASTOR 0x4
#define PPU_EX_DATASEGM 0x8
#define PPU_EX_INSSTOR 0x10
#define PPU_EX_INSTSEGM 0x20
#define PPU_EX_EXT 0x40
#define PPU_EX_ALIGNM 0x80
#define PPU_EX_PROG 0x100
#define PPU_EX_FPU 0x200
#define PPU_EX_DEC 0x400
#define PPU_EX_HDEC 0x800
#define PPU_EX_VXU 0x1000
#define PPU_EX_SC 0x2000
#define PPU_EX_TRACE 0x4000
#define PPU_EX_PERFMON 0x8000

// Program exception types.
#define PROGRAM_EXCEPTION_TYPE_FPU 43 // Floating Point Exception.
#define PROGRAM_EXCEPTION_TYPE_ILL 44 // Illegal instruction Exception.
#define PROGRAM_EXCEPTION_TYPE_PRIV 45 // Priviliged instruction Exception.
#define PROGRAM_EXCEPTION_TYPE_TRAP 46 // TRAP instruction Exception.

// Floating Point Register
struct FPRegister {
private:
  u64 hexValue = 0;
public:
  u32 asU32() { return static_cast<u32>(hexValue); }
  u64 asU64() { return hexValue; }
  double asDouble() { return std::bit_cast<double>(hexValue); }

  void setValue(u64 inValue) { hexValue = inValue; }
  void setValue(double inValue) { hexValue = std::bit_cast<u64>(inValue); }
};

//
// PowerPC State definition
//

// This contains all registers that are duplicated per thread.
struct PPU_THREAD_REGISTERS {
  // Special purpose registers
  PPU_THREAD_SPRS SPR;
  // Current Instruction Address
  u64 CIA;
  // Next Instruction Address
  u64 NIA;
  // Current instruction data
  PPCOpcode CI;
  // Instruction fetch flag
  bool instrFetch = false;
  // General-Purpose Registers (32)
  u64 GPR[32]{};
  // Floating-Point Registers (32)
  FPRegister FPR[32]{};
  // Vector Registers (128)
  Base::Vector128 VR[128]{};
  // Condition Register
  CRegister CR;
  // Floating-Point Status Control Register
  FPSCRegister FPSCR;
  // Segment Lookaside Buffer
  SLBEntry SLB[64]{};
  // Vector Status and Control Register
  VSCRegister VSCR;

  // ERAT's

  LRUCache iERAT{}; // Instruction effective to real address cache.
  LRUCache dERAT{}; // Data effective to real address cache.

  // Exception Register
  u16 exceptReg = 0;
  // Program Exception Type
  u16 progExceptionType = 0;
  // Tells wheter we're currently processing an exception.
  bool exceptionTaken = false;
  // For use with Data/Instruction Storage/Segment exceptions.
  u64 exceptEA = 0;
  // SystemCall Type
  bool exceptHVSysCall = false;

  // Interrupt EA for managing Interrupts.
  u64 intEA = 0;

  // Helper Debug Variables
  u64 lastWriteAddress = 0;
  u64 lastRegValue = 0;

  std::unique_ptr<PPU_RES> ppuRes{};
};

struct PPU_STATE {
  ~PPU_STATE() {
    for (u8 i = 0; i < 2; ++i) {
      ppuThread[i].ppuRes.reset();
    }
  }
  // Thread Specific State.
  PPU_THREAD_REGISTERS ppuThread[2] = {};
  // Current executing thread.
  ePPUThread currentThread = ePPUThread_Zero;
  // Shared Special Purpose Registers.
  PPU_STATE_SPRS SPR{};
  // Translation Lookaside Buffer
  TLB_Reg TLB{};
  // Address Translation Flag
  bool translationInProgress = false;
  // Current PPU Name, for ease of debugging.
  std::string ppuName{};
  // PPU ID
  u8 ppuID = 0;
};

struct XENON_CONTEXT {
  // 32Kb SROM
  u8 *SROM = new u8[XE_SROM_SIZE];
  // 64 Kb SRAM
  u8 *SRAM = new u8[XE_SRAM_SIZE];
  // 768 bits eFuse
  eFuses fuseSet = {};

  // Xenon IIC.
  Xe::XCPU::IIC::XenonIIC xenonIIC = {};

  XenonReservations xenonRes = {};

  // Time Base switch, possibly RTC register, the TB counter only runs if this
  // value is set.
  bool timeBaseActive = false;

  // Xenon SOC Blocks R/W methods.
  bool HandleSOCRead(u64 readAddr, u8* data, size_t byteCount);
  bool HandleSOCWrite(u64 writeAddr, const u8* data, size_t byteCount);

  //
  // SOC Blocks.
  // 

  // Secure OTP Block.
  std::unique_ptr<Xe::XCPU::SOC::SOCSECOTP_ARRAY> socSecOTPBlock = std::make_unique<Xe::XCPU::SOC::SOCSECOTP_ARRAY>();

  // Security Engine Block.
  std::unique_ptr<Xe::XCPU::SOC::SOCSECENG_BLOCK> socSecEngBlock = std::make_unique<Xe::XCPU::SOC::SOCSECENG_BLOCK>();

  // Secure RNG Block.
  std::unique_ptr<Xe::XCPU::SOC::SOCSECRNG_BLOCK> socSecRNGBlock = std::make_unique<Xe::XCPU::SOC::SOCSECRNG_BLOCK>();

  // CBI Block.
  std::unique_ptr<Xe::XCPU::SOC::SOCCBI_BLOCK> socCBIBlock = std::make_unique<Xe::XCPU::SOC::SOCCBI_BLOCK>();

  // PMW Block.
  std::unique_ptr<Xe::XCPU::SOC::SOCPMW_BLOCK> socPMWBlock = std::make_unique<Xe::XCPU::SOC::SOCPMW_BLOCK>();

  // Pervasive Block.
  std::unique_ptr<Xe::XCPU::SOC::SOCPRV_BLOCK> socPRVBlock = std::make_unique<Xe::XCPU::SOC::SOCPRV_BLOCK>();

private:

  // Mutex for thread safety.
  std::recursive_mutex mutex;

  // SOC Blocks R/W.

  // Security Engine Block.
  bool HandleSecEngRead(u64 readAddr, u8* data, size_t byteCount);
  bool HandleSecEngWrite(u64 writeAddr, const u8* data, size_t byteCount);

  // Secure OTP Block.
  bool HandleSecOTPRead(u64 readAddr, u8* data, size_t byteCount);
  bool HandleSecOTPWrite(u64 writeAddr, const u8* data, size_t byteCount);

  // Secure RNG Block.
  bool HandleSecRNGRead(u64 readAddr, u8* data, size_t byteCount);
  bool HandleSecRNGWrite(u64 writeAddr, const u8* data, size_t byteCount);

  // CBI Block.
  bool HandleCBIRead(u64 readAddr, u8* data, size_t byteCount);
  bool HandleCBIWrite(u64 writeAddr, const u8* data, size_t byteCount);

  // PMW Block.
  bool HandlePMWRead(u64 readAddr, u8* data, size_t byteCount);
  bool HandlePMWWrite(u64 writeAddr, const u8* data, size_t byteCount);

  // Pervasive logic Block.
  bool HandlePRVRead(u64 readAddr, u8* data, size_t byteCount);
  bool HandlePRVWrite(u64 writeAddr, const u8* data, size_t byteCount);
};

//
// Xenon Special Purpose Registers
//

#define SPR_XER 1
#define SPR_LR 8
#define SPR_CTR 9
#define SPR_DSISR 18
#define SPR_DAR 19
#define SPR_DEC 22
#define SPR_SDR1 25
#define SPR_SRR0 26
#define SPR_SRR1 27
#define SPR_CFAR 28
#define SPR_PID 48
#define SPR_ESR 62
#define SPR_IVPR 63
#define SPR_CTRLRD 136
#define SPR_CTRLWR 152
#define SPR_VRSAVE 256
#define SPR_TBL_RO 268
#define SPR_TBU_RO 269
#define SPR_SPRG0 272
#define SPR_SPRG1 273
#define SPR_SPRG2 274
#define SPR_SPRG3 275
#define SPR_TBL_WO 284
#define SPR_TBU_WO 285
#define SPR_TB 286
#define SPR_PVR 287
#define SPR_HSPRG0 304
#define SPR_HSPRG1 305
#define SPR_HDSISR 306
#define SPR_HDAR 307
#define SPR_DBCR0 308
#define SPR_DBCR1 309
#define SPR_HDEC 310
#define SPR_HIOR 311
#define SPR_RMOR 312
#define SPR_HRMOR 313
#define SPR_HSRR0 314
#define SPR_HSRR1 315
#define SPR_DAC1 316
#define SPR_DAC2 317
#define SPR_LPCR 318
#define SPR_LPIDR 319
#define SPR_TSR 336
#define SPR_TCR 340
#define SPR_SIAR 780
#define SPR_SDAR 781
#define SPR_TSRL 896
#define SPR_TSRR 897
#define SPR_TSCR 921
#define SPR_TTR 922
#define SPR_PpeTlbIndexHint 946
#define SPR_PpeTlbIndex 947
#define SPR_PpeTlbVpn 948
#define SPR_PpeTlbRpn 949
#define SPR_PpeTlbRmt 951
#define SPR_DSR0 952
#define SPR_DRMR0 953
#define SPR_DCIDR0 954
#define SPR_DRSR1 955
#define SPR_DRMR1 956
#define SPR_DCIDR1 957
#define SPR_ISSR0 976
#define SPR_IRMR0 977
#define SPR_ICIDR0 978
#define SPR_IRSR1 979
#define SPR_IRMR1 980
#define SPR_ICIDR1 981
#define SPR_HID0 1008
#define SPR_HID1 1009
#define SPR_IABR 1010
#define SPR_HID4 1012
#define SPR_DABR 1013
#define SPR_HID5 1014
#define SPR_DABRX 1015
#define SPR_BUSCSR 1016
#define SPR_HID6 1017
#define SPR_L2SR 1018
#define SPR_BPVR 1022
#define SPR_PIR 1023
