/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <bit>
#include <memory>
#include <unordered_map>

#include "Base/Bitfield.h"
#include "Base/LRUCache.h"
#include "Base/Vector128.h"
#include "Core/XCPU/Context/Reservations/XenonReservations.h"


// PowerPC Opcode definitions
/*
* All original authors of the rpcs3 PPU_Decoder and PPU_Opcodes maintain their original copyright.
* Modifed for usage in the Xenon Emulator
* All rights reserved
* License: GPL2
*/
template<typename T, u32 I, u32 N>
using PPCBitfield = bitfield<T, sizeof(T) * 8 - N - I, N>;
// PowerPC Instruction bitfields. Includes VMX128 bitfields.
union uPPCInstr {
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
  PPCBitfield<u32, 25, 1> v128rc; // 25
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
// PPU Registers Definitions, for both LE and BE byte orderings.
//

// General Purpose Register
typedef u64 GPR_t;

// Floating Point Register
struct sFPR {
private:
  u64 hexValue = 0;
public:
  u32 asU32() { return static_cast<u32>(hexValue); }
  u64 asU64() { return hexValue; }
  double asDouble() { return std::bit_cast<double>(hexValue); }

  void setValue(u64 inValue) { hexValue = inValue; }
  void setValue(double inValue) { hexValue = std::bit_cast<u64>(inValue); }
};

// Link Register
typedef u64 LR_t;

// Count Register
typedef u64 CTR_t;

// Data Storage Interrupt Status Register
typedef u32 DSISR_t;

// Data Address Register
typedef u64 DAR_t;

// Machine Status Save/Restore Register
typedef u64 SRR_t;

// Address Compare Control Register
union uACCR {
  u64 hexValue;
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 DR : 1;   // Data Read
    u64 DW : 1;   // Data Write
    u64 : 62;
  };
#else
  struct {
    u64 : 62;
    u64 DW : 1;   // Data Write
    u64 DR : 1;   // Data Read
  };
#endif
};

// VXU Register Save
typedef u32 VRSAVE_t;

// Software Use Special Purpose Register 
typedef u64 SPRG_t;

// Condition Register
union uCR {
  u32 CR_Hex;
  // Individual Bit access.
  u8 bits[32];

  u8 &operator [](size_t bitIdx) { return bits[bitIdx]; }

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

// Floating-Point Status and Control Register
union uFPSCR {
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

  uFPSCR() = default;
  explicit uFPSCR(u32 hexValue) : FPSCR_Hex{ hexValue & fpscrMask } {}

  u8& operator[](u8 bitIdx) { return bits[bitIdx]; }

  uFPSCR& operator=(u32 inValue) { FPSCR_Hex = inValue & fpscrMask; return *this; }

  uFPSCR& operator|=(u32 inValue) { FPSCR_Hex |= inValue & fpscrMask; return *this; }

  uFPSCR& operator&=(u32 inValue) { FPSCR_Hex &= inValue; return *this; }

  uFPSCR& operator^=(u32 inValue) { FPSCR_Hex ^= inValue & fpscrMask; return *this; }

  // Clears both Fraction Inexact and Fraction Rounded bits.
  void clearFIFR() { FI = 0; FR = 0; }
};

// Storage Description Register 1
union uSDR1 {
  u64 hexValue;
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 HTABSIZE : 5;   // Encoded size of Page Table
    u64 : 13;
    u64 HTABORG : 43;   // Real address of Page Table
    u64 : 3;
  };
#else
  struct {
    u64 : 3;
    u64 HTABORG : 43;   // Real address of Page Table
    u64 : 13;
    u64 HTABSIZE : 5;   // Encoded size of Page Table
  };
#endif
};

// Fixed-Point Exception Register
union uXER {
  u32 hexValue;
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

// Control Register
union uCTRL {
  u32 hexValue;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 RUN : 1;  //  Run state bit
    u32 : 13;
    u32 TH : 2;   // Thread history
    u32 : 6;
    u32 TE1 : 1;
    u32 TE0 : 1;  // Thread enable bits (Read/Write)
    u32 : 6;
    u32 CT : 2;   // Current thread active (Read Only)
  };
#else
  struct {
    u32 CT : 2;   // Current thread active (Read Only)
    u32 : 6;
    u32 TE0 : 1;  // Thread enable bits (Read/Write)
    u32 TE1 : 1;
    u32 : 6;
    u32 TH : 2;   // Thread history
    u32 : 13;
    u32 RUN : 1;  //  Run state bit
  };
#endif
};

// Time Base Register
union uTB {
  u64 hexValue;
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 TBL : 32;   // Lower 32 bits of Time Base
    u64 TBU : 32;   // Upper 32 bits of Time Base
  };
#else
  struct {
    u64 TBU : 32;   // Lower 32 bits of Time Base
    u64 TBL : 32;   // Upper 32 bits of Time Base
  };
#endif
};

// Processor Version Register
union uPVR {
  u32 hexValue;
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

// Decrementer Register
typedef s32 DEC_t;

// Real Mode Offset Register
union uRMOR {
  u64 hexValue;
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 : 20;
    u64 RMO : 22; // Real-mode offset
    u64 : 22;
  };
#else
  struct {
    u64 : 22;
    u64 RMO : 22; // Real-mode offset
    u64 : 20;
  };
#endif
};

// Hypervisor Real Mode Offset Register
union uHRMOR {
  u64 hexValue;
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 : 20;
    u64 HRMO : 22; // Real-mode offset
    u64 : 22;
  };
#else
  struct {
    u64 : 22;
    u64 HRMO : 22; // Real-mode offset
    u64 : 20;
  };
#endif
};

// Logical Partition Control Register
union uLPCR {
  u64 hexValue;
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 HDICE : 1;  // Hypervisor decrementer interrupt control enable
    u64 RMI : 1;    // Real-mode caching (caching inhibited)
    u64 LPES : 2;   // Logical partitioning (environment selector)
    u64 : 6;
    u64 TL : 1;     // Translation lookaside buffer (TLB) load
    u64 MER : 1;    // Mediate external exception request (interrupt enable)
    u64 : 14;
    u64 RMLS : 4;   // Real-mode limit selector
    u64 : 34;
  };
#else
  struct {
    u64 : 34;
    u64 RMLS : 4;   // Real-mode limit selector
    u64 : 14;
    u64 MER : 1;    // Mediate external exception request (interrupt enable)
    u64 TL : 1;     // Translation lookaside buffer (TLB) load
    u64 : 6;
    u64 LPES : 2;   // Logical partitioning (environment selector)
    u64 RMI : 1;    // Real-mode caching (caching inhibited)
    u64 HDICE : 1;  // Hypervisor decrementer interrupt control enable
  };
#endif
};

// Logical Partition Identity Register
union uLPIDR {
  u32 hexValue;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 LPID : 5; // Logical partition ID
    u32 : 27;
  };
#else
  struct {
    u32 : 27;
    u32 LPID : 5; // Logical partition ID
  };
#endif
};

// Thread Status Register (Local - Remote)
union uTSR {
  u64 hexValue;
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 FWDP : 20;  // Forward progress timer (Read Only)
    u64 : 31;
    u64 TP : 2;     // Thread priority (read/write)
    u64 : 11;
  };
#else
  struct {
    u64 : 11;
    u64 TP : 2;     // Thread priority (read/write)
    u64 : 31;
    u64 FWDP : 20;  // Forward progress timer (Read Only)
  };
#endif
};

// Thread Switch Control Register
union uTSCR {
  u32 hexValue;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 : 12;
    u32 Rsvd_l0 : 3;
    u32 UCP : 1;        // Problem state change thread priority enable
    u32 PSCTP : 1;      // Privileged but not hypervisor state change thread priority enable
    u32 Rsvd_l1 : 1;
    u32 FPCF : 1;       // Forward progress count flush enable
    u32 PBUMP : 1;      // Thread priority boost enable
    u32 WEXT : 1;       // External interrupt wakeup enable
    u32 WDEC1 : 1;      // Decrementer wakeup enable for thread 1
    u32 WDEC0 : 1;      // Decrementer wakeup enable for thread 0
    u32 Rsvd_l2 : 4;
    u32 DISP_CNT : 5;   // Thread dispatch count
  };
#else
  struct {
    u32 DISP_CNT : 5;   // Thread dispatch count
    u32 Rsvd_l2 : 4;
    u32 WDEC0 : 1;      // Decrementer wakeup enable for thread 0
    u32 WDEC1 : 1;      // Decrementer wakeup enable for thread 1
    u32 WEXT : 1;       // External interrupt wakeup enable
    u32 PBUMP : 1;      // Thread priority boost enable
    u32 FPCF : 1;       // Forward progress count flush enable
    u32 Rsvd_l1 : 1;
    u32 PSCTP : 1;      // Privileged but not hypervisor state change thread priority enable
    u32 UCP : 1;        // Problem state change thread priority enable
    u32 Rsvd_l0 : 3;
    u32 : 12;
  };
#endif
};

// Thread Switch Timeout Register (TTR)
union uTTR {
  u64 hexValue;
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 TTIM : 20;  //  Thread timeout flush value
    u64 : 44;
  };
#else
  struct {
    u64 : 44;
    u64 TTIM : 20;  //  Thread timeout flush value
  };
#endif
};

// PPE Translation Lookaside Buffer Index Hint Register
union uPPE_TLB_Index_Hint {
  u64 hexValue;
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 TSH : 4;  // TLB set hint
    u64 TIH : 8;  // PPE TLB index hint
    u64 : 52;
  };
#else
  struct {
    u64 : 52;
    u64 TIH : 8;  // PPE TLB index hint
    u64 TSH : 4;  // TLB set hint
  };
#endif
};

// PPE Translation Lookaside Buffer Index Register
union uPPE_TLB_Index {
  u64 hexValue;
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 TS : 4;     // TLB set
    u64 TI : 8;     // PPE TLB index
    u64 : 33;
    u64 LVPN : 3;   // Lower virtual page number
    u64 : 16;
  };
#else
  struct {
    u64 : 16;
    u64 LVPN : 3;   // Lower virtual page number
    u64 : 33;
    u64 TI : 8;     // PPE TLB index
    u64 TS : 4;     // TLB set
  };
#endif
};

// PPE Translation Lookaside Buffer Virtual-Page Number Register
union uPPE_TLB_VPN {
  u64 hexValue;
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 V : 1;      // Valid bit
    u64 : 1;        // TLB set
    u64 L : 1;      // Large-page mode
    u64 : 4;
    u64 AVPN : 42;  // Abbreviated virtual page number
    u64 : 15;
  };
#else
  struct {
    u64 : 15;
    u64 AVPN : 42;  // Abbreviated virtual page number
    u64 : 4;
    u64 L : 1;      // Large-page mode
    u64 : 1;        // TLB set
    u64 V : 1;      // Valid bit
  };
#endif
};

// PPE Translation Lookaside Buffer Real-Page Number Register
union uPPE_TLB_RPN {
  u64 hexValue;
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 PP2 : 1;      // Page-Protection bit 2 for tags inactive mode
    u64 PP1 : 1;      // Page-Protection bit 1 for tags inactive mode
    u64 N : 1;        // No execute
    u64 G : 1;        // Guarded
    u64 M : 1;        // Memory coherency bit
    u64 I : 1;        // Caching inhibited
    u64 W : 1;        // Write-through
    u64 C : 1;        // Change
    u64 R : 1;        // Reference
    u64 AC : 1;       // Address compare
    u64 : 2;
    u64 LP : 1;       // Large page size selector
    u64 ARPN : 29;    // Abbreviated real page number
    u64 : 22;
  };
#else
  struct {
    u64 : 22;
    u64 ARPN : 29;    // Abbreviated real page number
    u64 LP : 1;       // Large page size selector
    u64 : 2;
    u64 AC : 1;       // Address compare
    u64 R : 1;        // Reference
    u64 C : 1;        // Change
    u64 W : 1;        // Write-through
    u64 I : 1;        // Caching inhibited
    u64 M : 1;        // Memory coherency bit
    u64 G : 1;        // Guarded
    u64 N : 1;        // No execute
    u64 PP1 : 1;      // Page-Protection bit 1 for tags inactive mode
    u64 PP2 : 1;      // Page-Protection bit 2 for tags inactive mode
  };
#endif
};

// PPE Translation Lookaside BufferRMT Register
union uPPE_TLB_RMT {
  u64 hexValue;
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 RMT7 : 4;      //  Entry 7 of the RMT
    u64 RMT6 : 4;      //  Entry 6 of the RMT
    u64 RMT5 : 4;      //  Entry 5 of the RMT
    u64 RMT4 : 4;      //  Entry 4 of the RMT
    u64 RMT3 : 4;      //  Entry 3 of the RMT
    u64 RMT2 : 4;      //  Entry 2 of the RMT
    u64 RMT1 : 4;      //  Entry 1 of the RMT
    u64 RMT0 : 4;      //  Entry 0 of the replacement management table (RMT)
    u64 : 32;
  };
#else
  struct {
    u64 : 32;
    u64 RMT0 : 4;      //  Entry 0 of the replacement management table (RMT)
    u64 RMT1 : 4;      //  Entry 1 of the RMT
    u64 RMT2 : 4;      //  Entry 2 of the RMT
    u64 RMT3 : 4;      //  Entry 3 of the RMT
    u64 RMT4 : 4;      //  Entry 4 of the RMT
    u64 RMT5 : 4;      //  Entry 5 of the RMT
    u64 RMT6 : 4;      //  Entry 6 of the RMT
    u64 RMT7 : 4;      //  Entry 7 of the RMT
  };
#endif
};

// Hardware Implementation Register 0
union uHID0 {
  u64 hexValue;
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 : 32;
    u64 en_attn : 1;              // Enable attention instruction (enable support processor attn instruction)
    u64 en_syserr : 1;            // Enable system errors 
    u64 therm_intr_en : 1;        // Master thermal management interrupt enable 
    u64 qattn_mode : 1;           // Service processor control 
    u64 Rsvd_l0 : 1;              // Reserved. Latch bit is implemented
    u64 en_prec_mchk : 1;         // Enable precise machine check
    u64 extr_hsrr : 1;            // Enable extended external interrupt
    u64 syserr_wakeup : 1;        // Enable system error interrupt to wakeup suspended thread 
    u64 Rsvd_l1 : 1;              // Reserved. Latch bit is implemented
    u64 therm_wakeup : 1;         // Enable thermal management interrupt to wakeup suspended thread
    u64 Rsvd_l2 : 13;             // Reserved. Latch bit is implemented
    u64 address_trace_mode : 1;   // Address trace mode
    u64 op1_flush : 1;            // Opcode compare 1 causes an internal flush to that thread
    u64 op0_flush : 1;            // Opcode compare 0 causes an internal flush to that thread
    u64 op1_debug : 1;            // Opcode compare 1 takes a maintenance interrupt on an opcode compare match
    u64 op0_debug : 1;            // Opcode compare 0 takes a maintenance interrupt on an opcode compare match
    u64 issue_serialize : 1;      // Issue serialize mode
    u64 Rsvd_l3 : 3;              // Reserved. Latch bit is implemented
  };
#else
  struct {
    u64 Rsvd_l3 : 3;              // Reserved. Latch bit is implemented
    u64 issue_serialize : 1;      // Issue serialize mode
    u64 op0_debug : 1;            // Opcode compare 0 takes a maintenance interrupt on an opcode compare match
    u64 op1_debug : 1;            // Opcode compare 1 takes a maintenance interrupt on an opcode compare match
    u64 op0_flush : 1;            // Opcode compare 0 causes an internal flush to that thread
    u64 op1_flush : 1;            // Opcode compare 1 causes an internal flush to that thread
    u64 address_trace_mode : 1;   // Address trace mode
    u64 Rsvd_l2 : 13;             // Reserved. Latch bit is implemented
    u64 therm_wakeup : 1;         // Enable thermal management interrupt to wakeup suspended thread
    u64 Rsvd_l1 : 1;              // Reserved. Latch bit is implemented
    u64 syserr_wakeup : 1;        // Enable system error interrupt to wakeup suspended thread 
    u64 extr_hsrr : 1;            // Enable extended external interrupt
    u64 en_prec_mchk : 1;         // Enable precise machine check
    u64 Rsvd_l0 : 1;              // Reserved. Latch bit is implemented
    u64 qattn_mode : 1;           // Service processor control 
    u64 therm_intr_en : 1;        // Master thermal management interrupt enable 
    u64 en_syserr : 1;            // Enable system errors 
    u64 en_attn : 1;              // Enable attention instruction (enable support processor attn instruction)
    u64 : 32;
  };
#endif
};

// Hardware Implementation Register 1
union uHID1 {
  u64 hexValue;
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 pu_trace_ctrl6 : 2;     // PPU trace bus [32:63] output control
    u64 pu_trace_ctrl5 : 2;     // PPU trace bus [0:31] output control
    u64 mmu_trace_ctrl1 : 1;    // MMU ramp controls for PPU trace bus [64:95]
    u64 mmu_trace_ctrl0 : 1;    // MMU ramp controls for PPU trace bus [0:31]
    u64 iu_trigger_en : 4;      // IU trigger bus enable
    u64 iu_trigger_ctrl : 4;    // IU trigger bus control
    u64 pu_trace_ctrl1 : 1;     // PPU trace bus [64:127] output control
    u64 pu_trace_ctrl0 : 1;     // PPU trace bus [0:63] output control
    u64 pu_trace_ctrl4 : 3;     // PPU trace bus [96:127] output control
    u64 pu_trace_ctrl3 : 3;     // PPU trace bus [64:95] output control
    u64 pu_trace_ctrl2 : 2;     // PPU trace bus [0:63] output control
    u64 pu_trace_byte_ctrl : 8; // Byte enables for PPU performance monitor bus/global debug bus
    u64 pu_trace_en : 1;        // Enable PPU performance monitor/debug bus 
    u64 Rsvd_l4 : 5;            // Reserved. Latch bit is implemented
    u64 en_i_prefetch : 1;      // Enable instruction prefetch
    u64 Rsvd_l3 : 5;            // Reserved. Latch bit is implemented
    u64 dis_sysrst_reg : 1;     // Disable configuration ring system reset interrupt address register
    u64 Rsvd_l2 : 4;            // Reserved. Latch bit is implemented
    u64 ic_pe : 1;              // Force instruction cache parity error
    u64 dis_pe : 1;             // Disable parity error reporting and recovery
    u64 grap_md : 1;            // Graphics rounding mode
    u64 sel_cach_rule : 1;      // Select which cacheability control rule to use
    u64 en_if_cach : 1;         // Enable instruction fetch cacheability control
    u64 en_icbi : 1;            // Enable forced icbi match mode
    u64 Rsvd_l1 : 2;            // Reserved. Latch bit is implemented
    u64 flush_ic : 1;           // Flush instruction cache
    u64 en_icway : 2;           // Enable instruction cache way (one bit per way)
    u64 en_ls : 1;              // Enable link stack
    u64 Rsvd_l0 : 1;            // Reserved. Latch bit is implemented
    u64 dis_gshare : 1;         // Disable global branch history branch prediction
    u64 bht_pm : 1;             // Branch history table prediction mode
  };
#else
  struct {
    u64 bht_pm : 1;             // Branch history table prediction mode
    u64 dis_gshare : 1;         // Disable global branch history branch prediction
    u64 Rsvd_l0 : 1;            // Reserved. Latch bit is implemented
    u64 en_ls : 1;              // Enable link stack
    u64 en_icway : 2;           // Enable instruction cache way (one bit per way)
    u64 flush_ic : 1;           // Flush instruction cache
    u64 Rsvd_l1 : 2;            // Reserved. Latch bit is implemented
    u64 en_icbi : 1;            // Enable forced icbi match mode
    u64 en_if_cach : 1;         // Enable instruction fetch cacheability control
    u64 sel_cach_rule : 1;      // Select which cacheability control rule to use
    u64 grap_md : 1;            // Graphics rounding mode
    u64 dis_pe : 1;             // Disable parity error reporting and recovery
    u64 ic_pe : 1;              // Force instruction cache parity error
    u64 Rsvd_l2 : 4;            // Reserved. Latch bit is implemented
    u64 dis_sysrst_reg : 1;     // Disable configuration ring system reset interrupt address register
    u64 Rsvd_l3 : 5;            // Reserved. Latch bit is implemented
    u64 en_i_prefetch : 1;      // Enable instruction prefetch
    u64 Rsvd_l4 : 5;            // Reserved. Latch bit is implemented
    u64 pu_trace_en : 1;        // Enable PPU performance monitor/debug bus 
    u64 pu_trace_byte_ctrl : 8; // Byte enables for PPU performance monitor bus/global debug bus
    u64 pu_trace_ctrl2 : 2;     // PPU trace bus [0:63] output control
    u64 pu_trace_ctrl3 : 3;     // PPU trace bus [64:95] output control
    u64 pu_trace_ctrl4 : 3;     // PPU trace bus [96:127] output control
    u64 pu_trace_ctrl0 : 1;     // PPU trace bus [0:63] output control
    u64 pu_trace_ctrl1 : 1;     // PPU trace bus [64:127] output control
    u64 iu_trigger_ctrl : 4;    // IU trigger bus control
    u64 iu_trigger_en : 4;      // IU trigger bus enable
    u64 mmu_trace_ctrl0 : 1;    // MMU ramp controls for PPU trace bus [0:31]
    u64 mmu_trace_ctrl1 : 1;    // MMU ramp controls for PPU trace bus [64:95]
    u64 pu_trace_ctrl5 : 2;     // PPU trace bus [0:31] output control
    u64 pu_trace_ctrl6 : 2;     // PPU trace bus [32:63] output control
  };
#endif
};

// Hardware Implementation Register 4
union uHID4 {
  u64 hexValue;
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 : 32;
    u64 Rsvd_l4 : 8;        // Reserved. Latch bit is implemented
    u64 en_dway : 4;        // Enable L1 data cache way (one bit per way)
    u64 enb_force_ci : 1;   // Enable force_data_cache_inhibit (only valid if dis_force_ci = 0)
    u64 dis_force_ci : 1;   // Force data cache inhibit, unless HID4[19] disables
    u64 Rsvd_l3 : 2;        // Reserved. Latch bit is implemented
    u64 lmq_size : 3;       // Maximum number of outstanding demand requests to the memory subsystem
    u64 tch_nop : 1;        // Force data cache block touch x-form (dcbt) and data cache block touch for store (dcbtst)
    // instructions to function like nop instructions
    u64 force_ai : 1;       // Force an alignment interrupt instead of microcode on unaligned operations
    u64 force_geq1 : 1;     // Force all load instructions to be treated as if being loaded to guarded storage
    u64 Rsvd_l2 : 3;        // Reserved. Latch bit is implemented
    u64 l1dc_flsh : 1;      // L1 data cache flash invalidate
    u64 dis_hdwe_recov : 1; // Disable data cache parity error hardware recovery
    u64 Rsvd_l1 : 1;        // Reserved. Latch bit is implemented
    u64 hdwe_err_inj : 1;   // Inject parity error into cache
    u64 dis_dcpc : 1;       // Disable data cache parity checking
    u64 Rsvd_l0 : 2;        // Reserved. Latch bit is implemented
  };
#else
  struct {
    u64 Rsvd_l0 : 2;        // Reserved. Latch bit is implemented
    u64 dis_dcpc : 1;       // Disable data cache parity checking
    u64 hdwe_err_inj : 1;   // Inject parity error into cache
    u64 Rsvd_l1 : 1;        // Reserved. Latch bit is implemented
    u64 dis_hdwe_recov : 1; // Disable data cache parity error hardware recovery
    u64 l1dc_flsh : 1;      // L1 data cache flash invalidate
    u64 Rsvd_l2 : 3;        // Reserved. Latch bit is implemented
    u64 force_geq1 : 1;     // Force all load instructions to be treated as if being loaded to guarded storage
    u64 force_ai : 1;       // Force an alignment interrupt instead of microcode on unaligned operations
    u64 tch_nop : 1;        // Force data cache block touch x-form (dcbt) and data cache block touch for store (dcbtst)
    // instructions to function like nop instructions
    u64 lmq_size : 3;       // Maximum number of outstanding demand requests to the memory subsystem
    u64 Rsvd_l3 : 2;        // Reserved. Latch bit is implemented
    u64 dis_force_ci : 1;   // Force data cache inhibit, unless HID4[19] disables
    u64 enb_force_ci : 1;   // Enable force_data_cache_inhibit (only valid if dis_force_ci = 0)
    u64 en_dway : 4;        // Enable L1 data cache way (one bit per way)
    u64 Rsvd_l4 : 8;        // Reserved. Latch bit is implemented
    u64 : 32;
  };
#endif
};

// Hardware Implementation Register 6
union uHID6 {
  u64 hexValue;
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 : 34;
    u64 RMSC : 4;           // PPE real-mode storage control facility
    u64 : 6;
    u64 LB : 4;             // Large page bit table
    u64 tb_enable : 1;      // Time-base and decrementer facility enable
    u64 Rsvd_l1 : 4;
    u64 debug_enable : 1;   // Turn on the performance or debug bus for the MMU
    u64 Rsvd_l0 : 2;
    u64 debug : 8;          // Debug and performance select
  };
#else
  struct {
    u64 debug : 8;          // Debug and performance select
    u64 Rsvd_l0 : 2;
    u64 debug_enable : 1;   // Turn on the performance or debug bus for the MMU
    u64 Rsvd_l1 : 4;
    u64 tb_enable : 1;      // Time-base and decrementer facility enable
    u64 LB : 4;             // Large page bit table
    u64 : 6;
    u64 RMSC : 4;           // PPE real-mode storage control facility
    u64 : 34;
  };
#endif
};

// Data Address Beakpoint Register
union uDABR {
  u64 hexValue;
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 DR : 1;    // Data Read
    u64 DW : 1;    // Data Write
    u64 BT : 1;    // Breakpoint Translation
    u64 DAB : 61;  // Data Address Breakpoint
  };
#else
  struct {
    u64 DAB : 61; // Data Address Breakpoint
    u64 BT : 1;   // Breakpoint Translation
    u64 DW : 1;   // Data Write
    u64 DR : 1;   // Data Read
  };
#endif
};

// Data Address Beakpoint Register Extension
union uDABRX {
  u64 hexValue;
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 PRIVM : 3; // Privilege Mask
    // Bit 61: Hypervisor State
    // Bit 62: Privileged but Non-Hypervisor State
    // Bit 63: Problem State
    u64 BT : 1;    // Breakpoint Translation Ignore
    u64 : 60;
  };
#else
  struct {
    u64 : 60;
    u64 BT : 1;    // Breakpoint Translation Ignore
    u64 PRIVM : 3; // Privilege Mask
    // Bit 61: Hypervisor State
    // Bit 62: Privileged but Non-Hypervisor State
    // Bit 63: Problem State
  };
#endif
};

// Machine State Register
union uMSR {
  u64 hexValue;
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

// Processor Identification Register
typedef u32 PIR_t;

// Vector Status and Control Register
union uVSCR {
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
struct sSLBEntry {
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

// Xenon Special Purpose Registers
// This is mainly taken from CBE_Public_Registers.v1_5.02APR2007.pdf
// All registers are R/W unless specified otherwise.
enum eXenonSPR : u16 {
  // SPR Name | Decimal ID | Description
  XER = 1,      // Fixed-Point Exception Register
  LR = 8,       // Link Register
  CTR = 9,      // Count Register
  DSISR = 18,   // Data Storage Interrupt Status Register
  DAR = 19,     // Data Address Register
  DEC = 22,     // Decrementer Register
  SDR1 = 25,    // Storage Description Register 1 
  SRR0 = 26,    // Machine Status Save/Restore Register 0 
  SRR1 = 27,    // Machine Status Save/Restore Register 1
  CFAR = 28,    // Not described in the manual but used in Linux
  ACCR = 29,    // Address Compare Control Register
  CTRLRD = 136, // Control Register - Read Only
  CTRLWR = 152, // Control Register - Write Only
  VRSAVE = 256, // VXU Register Save
  SPRG3RD = 259,  // Software Use Special Purpose Register 3 - Read Only
  TBLRO = 268,  // Time Base Register (Lower 32 bits) - Read Only
  TBURO = 269,  // Time Base Register (Upper 32 bits) - Read Only
  SPRG0 = 272,  // Software Use Special Purpose Register 0 
  SPRG1 = 273,  // Software Use Special Purpose Register 1 
  SPRG2 = 274,  // Software Use Special Purpose Register 2 
  SPRG3 = 275,  // Software Use Special Purpose Register 3 
  TBLWO = 284,  // Time Base Register (Lower 32 bits) - Write Only
  TBUWO = 285,  // Time Base Register (Upper 32 bits) - Write Only
  PVR = 287,    // PPE Processor Version Register
  HSPRG0 = 304, // Hypervisor Software Use Special Purpose Register 0 
  HSPRG1 = 305, // Hypervisor Software Use Special Purpose Register 1
  HDEC = 310,   // Hypervisor Decrementer Register
  RMOR = 312,   // Real Mode Offset Register
  HRMOR = 313,  // Hypervisor Real Mode Offset Register
  HSRR0 = 314,  // Hypervisor Machine Status Save / Restore Register 0
  HSRR1 = 315,  // Hypervisor Machine Status Save / Restore Register 1
  LPCR = 318,   // Logical Partition Control Register
  LPIDR = 319,  // Logical Partition Identity Register
  TSRL = 896,   // Thread Status Register Local
  TSRR = 897,   // Thread Status Register Remote - Read Only
  TSCR = 921,   // Thread Switch Control Register
  TTR = 922,    // Thread Switch Timeout Register
  PPE_TLB_Index_Hint = 946, // PPE Translation Lookaside Buffer Index Hint Register - Read Only
  PPE_TLB_Index = 947,      // PPE Translation Lookaside Buffer Index Register
  PPE_TLB_VPN = 948,        // PPE Translation Lookaside Buffer Virtual-Page Number Register
  PPE_TLB_RPN = 949,        // PPE Translation Lookaside Buffer Real-Page Number Register
  PPE_TLB_RMT = 951,        // PPE Translation Lookaside Buffer RMT Register
  DRSR0 = 952,  // Data Range Start Register 0
  DRMR0 = 953,  // Data Range Mask Register 0
  DCIDR0 = 954, // Data Class ID Register 0
  DRSR1 = 955,  // Data Range Start Register 1
  DRMR1 = 956,  // Data Range Mask Register 1
  DCIDR1 = 957, // Data Class ID Register 1
  IRSR0 = 976,  // Instruction Range Start Register 0
  IRMR0 = 977,  // Instruction Range Mask Register 0
  ICIDR0 = 978, // Instruction Class ID Register 0
  IRSR1 = 979,  // Instruction Range Start Register 1
  IRMR1 = 980,  // Instruction Range Mask Register 1
  ICIDR1 = 981, // Instruction Class ID Register 1
  HID0 = 1008,  // Hardware Implementation Register 0
  HID1 = 1009,  // Hardware Implementation Register 1
  HID4 = 1012,  // Hardware Implementation Register 4
  DABR = 1013,  // Data Address Breakpoint Register
  DABRX = 1015, // Data Address Breakpoint Register Extension
  HID6 = 1017,  // Hardware Implementation Register 6
  BP_VR = 1022, // CBEA-Compliant Processor Version Register - Read Only
  PIR = 1023    // Processor Identification Register - Read Only
};

//
// PPU Thread definition and related structures
//

// Contains the Per PPU Thread, 'duplicated' Special Purpose Registers.
struct sPPUThreadSPRs {
  uXER XER;         // Fixed-Point Exception Register
  LR_t LR;          // Link Register
  CTR_t CTR;        // Count Register
  u64 CFAR;         // Used in Linux, unknown definition atm.
  DSISR_t DSISR;    // Data Storage Interrupt Status Register
  DAR_t DAR;        // Data Address Register
  DEC_t DEC;        // Decrementer Register
  SRR_t SRR0;       // Machine Status Save/Restore Register 0
  SRR_t SRR1;       // Machine Status Save/Restore Register 1
  uACCR ACCR;       // Address Compare Control Register
  VRSAVE_t VRSAVE;  // VXU Register Save
  SPRG_t SPRG0;     // Software Use Special Purpose Register 0
  SPRG_t SPRG1;     // Software Use Special Purpose Register 1
  SPRG_t SPRG2;     // Software Use Special Purpose Register 2
  SPRG_t SPRG3;     // Software Use Special Purpose Register 3
  SPRG_t HSPRG0;    // Hypervisor Software Use Special Purpose Register 0
  SPRG_t HSPRG1;    // Hypervisor Software Use Special Purpose Register 1
  SRR_t HSRR0;      // Hypervisor Save Restore Register 0
  SRR_t HSRR1;      // Hypervisor Save Restore Register 1
  uTSR TSRL;        // Thread Status Register Local
  uTSR TSRR;        // Thread Status Register Remote
  uPPE_TLB_Index_Hint PPE_TLB_Index_Hint;   // PPE Translation Lookaside Buffer Index Hint Register
  uDABR DABR;       // Data Address Beakpoint Register
  uDABRX DABRX;     // Data Address Beakpoint Register Extension
  uMSR MSR;         // Machine State Register
  PIR_t PIR;        // Processor Identification Register
};

// Contains the per PPU Core, 'non duplicated' Special Purpose Registers.
struct sPPUGlobalSPRs {
  uSDR1 SDR1;   // Storage Description Register 1
  uCTRL CTRL;   // Control Register
  uTB TB;       // Time Base Register
  uPVR PVR;     // Processor Version Register
  DEC_t HDEC;   // Hypervisor Decrementer Register
  uRMOR RMOR;   // Real Mode Offset Register
  uRMOR HRMOR;  // Hypervisor Real Mode Offset Register
  uLPCR LPCR;   // 'Global' Logical Partition Control Register
  uLPIDR LPIDR; // Logical Partition Identity Register
  uTSCR TSCR;   // Thread Switch Control Register
  uTTR TTR;     // Thread Switch Timeout Register
  uPPE_TLB_Index PPE_TLB_Index; // PPE Translation Lookaside Buffer Index Register
  uPPE_TLB_VPN PPE_TLB_VPN;     // PPE Translation Lookaside Buffer Virtual-Page Number Register
  uPPE_TLB_RPN PPE_TLB_RPN;     // PPE Translation Lookaside Buffer Real-Page Number Register
  uPPE_TLB_RMT PPE_TLB_RMT;     // PPE Translation Lookaside Buffer RMT Register 
  uHID0 HID0;   // Hardware Implementation Register 0
  uHID1 HID1;   // Hardware Implementation Register 1
  uHID4 HID4;   // Hardware Implementation Register 4
  uHID6 HID6;   // Hardware Implementation Register 6 
};

// Basic Execution Thread inside each PPU Core.
struct sPPUThread {
  //
  // Instructions Pointers and data
  //

  // Previous Instruction Address (Useful for debugging purposes)
  u64 PIA;
  // Current Instruction Address
  u64 CIA;
  // Next Instruction Address
  u64 NIA;
  // Current instruction data and bitfields
  uPPCInstr CI;
  // True if the current data fetch is for an instruction
  bool instrFetch = false;

  //
  // Registers
  //

  // General-Purpose Registers
  GPR_t GPR[32]{};
  // Floating-Point Registers
  sFPR FPR[32]{};
  // Per-thread 'duplicated' Special Purpose Registers
  sPPUThreadSPRs SPR;
  // Vector Registers
  Base::Vector128 VR[128]{};
  // Condition Register
  uCR CR;
  // Floating-Point Status Control Register
  uFPSCR FPSCR;
  // Vector Status and Control Register
  uVSCR VSCR;

  //
  // Misc Registers and Variables
  //

  // Segment Lookaside Buffer (MMU)
  sSLBEntry SLB[64]{};

  // ERAT's (MMU)
  LRUCache iERAT{}; // Instruction effective to real address cache.
  LRUCache dERAT{}; // Data effective to real address cache.

  // Exception Register
  u16 exceptReg = 0;
  // Program Exception Type
  u16 progExceptionType = 0;
  // SystemCall Type (Hypervisor syscall)
  bool exHVSysCall = false;
  // PPU reservations for PPC atomic load/store operations.
  std::unique_ptr<PPU_RES> ppuRes{};
};

// The structure of the Xenon CPU differs from that on the CELL/BE in that instead of having one PPE and 8 SPE's
// it contains 3 parallel PPE's each managing two threads (one physical and one logical).
// Should be depicted as follows:
/*
* Xenon XCPU --->PPE 0 --- PPU Thread 0
*             |            PPU Thread 1
*             |->PPE 1 --- PPU Thread 2
*             |            PPU Thread 3
*             |->PPE 2 --- PPU Thread 4
*                          PPU Thread 5
*/

// Thread IDs for ease of handling
enum ePPUThreadID : u8 {
  ePPUThread_Zero = 0,
  ePPUThread_One,
  ePPUThread_None
};
enum ePPUThreadBit : u8 {
  ePPUThreadBit_None = 0,
  ePPUThreadBit_Zero,
  ePPUThreadBit_One
};

// Power Processor Element (PPE)
struct sPPEState {
  ~sPPEState() {
    for (u8 i = 0; i < 2; ++i) {
      // Clear reservations.
      ppuThread[i].ppuRes.reset();
    }
  }
  // Power Processing Unit Threads
  sPPUThread ppuThread[2] = {};
  // Current executing thread ID.
  ePPUThreadID currentThread = ePPUThread_Zero;
  // Shared Special Purpose Registers.
  sPPUGlobalSPRs SPR{};
  // Translation Lookaside Buffer
  TLB_Reg TLB{};
  // Current PPU Name, for ease of debugging.
  std::string ppuName{};
  // PPU ID
  u8 ppuID = 0;
};

// Exception Bitmasks for Exception Register
enum eExceptionBitmask {
  ppuNone = 0x0,                      // No Exception
  ppuSystemResetEx = 0x1,             // System Reset Exception
  ppuMachineCheckEx = 0x2,            // Machine Check Exception
  ppuDataStorageEx = 0x4,             // Data Storage Exception
  ppuDataSegmentEx = 0x8,             // Data Segment Exception
  ppuInstrStorageEx = 0x10,           // Instruction Storage Exception
  ppuInstrSegmentEx = 0x20,           // Instruction segment Exception
  ppuExternalEx = 0x40,               // External Exception
  ppuAlignmentEx = 0x80,              // Alignment Exception
  ppuProgramEx = 0x100,               // Program Exception
  ppuFPUnavailableEx = 0x200,         // Floating-Point Unavailable Exception
  ppuDecrementerEx = 0x400,           // Decrementer Exception
  ppuHypervisorDecrementerEx = 0x800, // Hypervisor Decrementer Exception
  ppuVXUnavailableEx = 0x1000,        // Vector Execution Unit Unavailable Exception
  ppuSystemCallEx = 0x2000,           // System Call Exception
  ppuTraceEx = 0x4000,                // Trace Exception
  ppuPerformanceMonitorEx = 0x8000,   // Performance Monitor Exception
};

// Program Exception types
enum ePPUProgramExType {
 ppuProgExTypeFPU = 43,   // Floating Point Exception
 ppuProgExTypeILL = 44,   // Illegal instruction Exception
 ppuProgExTypePRIV = 45,  // Priviliged instruction Exception
 ppuProgExTypeTRAP = 46,  // TRAP instruction type Exception
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