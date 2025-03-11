// Copyright 2025 Xenon Emulator Project

#pragma once

#include <memory>

#include "Base/LRUCache.h"
#include "Core/XCPU/IIC/IIC.h"
#include "Core/XCPU/Bitfield.h"
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
};

//
// PowerPC Register definitions
//

/*
Condition Register

The CR fields can be set in one of the following ways:
• Specified fields of the CR can be set from a GPR by using the mtcrf and mtocrf
instruction. • The contents of the XER[0–3] can be moved to another CR field by
using the mcrf instruction. • A specified field of the XER can be copied to a
specified field of the CR by using the mcrxr instruction. • A specified field of
the FPSCR can be copied to a specified field of the CR by using the mcrfs
instruction. • Logical instructions of the condition register can be used to
perform logical operations on specified bits in the condition register. • CR0
can be the implicit result of an integer instruction. • CR1 can be the implicit
result of a floating-point instruction. • A specified CR field can indicate the
result of either an integer or floating-point compare instruction
*/
union CRegister {
  u32 CR_Hex;
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

/*
Floating-Point Status and Control Register (FPSCR)
*/
union FPSCRegister {
  u32 FPSCR_Hex;
  struct {
    u32 RN : 2;
    u32 NI : 1;
    u32 XE : 1;
    u32 ZE : 1;
    u32 UE : 1;
    u32 OE : 1;
    u32 VE : 1;
    u32 VXCVI : 1;
    u32 VXSQRT : 1;
    u32 VXSOFT : 1;
    u32 R0 : 1;
    u32 FPRF : 5;
    u32 FI : 1;
    u32 FR : 1;
    u32 VXVC : 1;
    u32 VXIMZ : 1;
    u32 VXZDZ : 1;
    u32 VXIDI : 1;
    u32 VXISI : 1;
    u32 VXSNAN : 1;
    u32 XX : 1;
    u32 ZX : 1;
    u32 UX : 1;
    u32 OX : 1;
    u32 VX : 1;
    u32 FEX : 1;
    u32 FX : 1;
  };
};

/*
 XER Register (XER)
*/
union XERegister {
  u32 XER_Hex;
  struct {
    u32 ByteCount : 7;
    u32 R0 : 22;
    u32 CA : 1;
    u32 OV : 1;
    u32 SO : 1;
  };
};

/*
Time Base (TB)
*/
union TB {
  u64 TB_Hex;
  struct {
    u64 TBL : 32;
    u64 TBU : 32;
  };
};

/*
Machine State Register (MSR)
*/
union MSRegister {
  u64 MSR_Hex;
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
};

/*
Processor Version Register (PVR)
*/
union PVRegister {
  u32 PVR_Hex;
  struct {
    u32 Revision : 16;
    u32 Version : 16;
  };
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

// Traslation lookaside buffer
struct TLBEntry {
  bool V;  // Entry valid.
  u8 p;    // Page Size
  u64 RPN; // Real page Number
  u64 VPN; // Virtual Page Number
  bool L;  // Large pages
  bool LP; // Large page selector
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

// Thread ID's for ease of handling.
enum PPU_THREAD {
  PPU_THREAD_0,
  PPU_THREAD_1,
  PPU_THREAD_BOTH,
  PPU_THREAD_NONE
};

//
// Security Engine Related Structures
//

#define XE_SECENG_ADDR 0x24000
#define XE_SECENG_SIZE 0x2000

enum SECENG_REGION_TYPE {
  SECENG_REGION_PHYS = 0,
  SECENG_REGION_HASHED = 1,
  SECENG_REGION_SOC = 2,
  SECENG_REGION_ENCRYPTED = 3
};

struct SECENG_ADDRESS_INFO {
  // Real address we're accesing on the Bus.
  u32 accesedAddr;
  // Region This address belongs to.
  SECENG_REGION_TYPE regionType;
  // Key used to hash/encrypt this address.
  u8 keySelected;
};

typedef union _SECENG_FAULT_ISOLATION {
  u64 AsULONGLONG; // 0x0 sz:0x8
  struct {
    u64 IntegrityViolation : 1; // 0x0 bfo:0x63
    u64 Reserved1 : 63;         // 0x0 bfo:0x0
  } AsBits;
} SECENG_FAULT_ISOLATION, *PSECENG_FAULT_ISOLATION; // size 8

typedef struct _SECENG_KEYS {
  u64 WhiteningKey0High;      // 0x0 sz:0x8
  u64 WhiteningKey0Low;       // 0x8 sz:0x8
  u64 WhiteningKey1High;      // 0x10 sz:0x8
  u64 WhiteningKey1Low;       // 0x18 sz:0x8
  u64 WhiteningKey2High;      // 0x20 sz:0x8
  u64 WhiteningKey2Low;       // 0x28 sz:0x8
  u64 WhiteningKey3High;      // 0x30 sz:0x8
  u64 WhiteningKey3Low;       // 0x38 sz:0x8
  u64 AESKey0High;            // 0x40 sz:0x8
  u64 AESKey0Low;             // 0x48 sz:0x8
  u64 AESKey1High;            // 0x50 sz:0x8
  u64 AESKey1Low;             // 0x58 sz:0x8
  u64 AESKey2High;            // 0x60 sz:0x8
  u64 AESKey2Low;             // 0x68 sz:0x8
  u64 AESKey3High;            // 0x70 sz:0x8
  u64 AESKey3Low;             // 0x78 sz:0x8
  u64 HashKey0High;           // 0x80 sz:0x8
  u64 HashKey0Low;            // 0x88 sz:0x8
  u64 HashKey1High;           // 0x90 sz:0x8
  u64 HashKey1Low;            // 0x98 sz:0x8
} SECENG_KEYS, *PSECENG_KEYS; // size 160

typedef struct SOCSECENG_BLOCK {       // Addr = 80000200_00024000
  SECENG_KEYS WritePathKeys;           // 0x0 sz:0xA0
  u64 TraceLogicArrayWritePathControl; // 0xA0 sz:0x8
  u64 qwUnkn1;
  u64 Reserved1[0x1EA];                      // 0xA8 sz:0xF58
  SECENG_KEYS ReadPathKeys;                  // 0x1000 sz:0xA0
  u64 TraceLogicArrayReadPathControl;        // 0x10A0 sz:0x8
  SECENG_FAULT_ISOLATION FaultIsolationMask; // 0x10A8 sz:0x8
  SECENG_FAULT_ISOLATION FaultIsolation; // 0x10B0 sz:0x8 - set to zero in CB
  u64 IntegrityViolationSignature;       // 0x10B8 sz:0x8
  u64 qwUnkn2;
  u64 Reserved2[0x1E7];               // 0x10C0 sz:0xF40
} SOCSECENG_BLOCK, *PSOCSECENG_BLOCK; // size 8192

#define XE_RESET_VECTOR 0x100
#define XE_SROM_ADDR 0x0
#define XE_SROM_SIZE 0x8000
#define XE_SRAM_ADDR 0x10000
#define XE_SRAM_SIZE 0x10000
#define XE_FUSESET_LOC 0x20000
#define XE_FUSESET_SIZE 0x17FF
#define XE_L2_CACHE_SIZE 0x100000
#define XE_PVR 0x00710500 // Corona: 0x00710800 Jasper: 0x00710500

// Exception Bitmasks for Exception Register.

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
#define PPU_EX_SC 0x1000
#define PPU_EX_TRACE 0x2000
#define PPU_EX_PERFMON 0x4000

// Floating Point Register

union SFPRegister { // Single Precision
  float valueAsFloat;
  u32 valueAsU32;
};

union FPRegister { // Double Precision
  double valueAsDouble;
  u64 valueAsU64;
};

//
// PowerPC State definition
//

// This contains all registers that are duplicated per thread.
struct PPU_THREAD_REGISTERS {
  PPU_THREAD_SPRS SPR;
  // Current Instruction Address
  u64 CIA;
  // Next Instruction Address
  u64 NIA;
  // Current instruction data
  PPCOpcode CI;
  // Instruction fetch flag
  bool iFetch = false;
  // General-Purpose Registers (32)
  u64 GPR[32]{};
  // Floating-Point Registers (32)
  FPRegister FPR[32]{};
  // Condition Register
  CRegister CR;
  // Floating-Point Status Control Register
  FPSCRegister FPSCR;
  // Segment Lookaside Buffer
  SLBEntry SLB[64]{};

  // ERAT's

  LRUCache iERAT; // Instruction effective to real address cache.
  LRUCache dERAT; // Data effective to real address cache.

  // Interrupt Register
  u16 exceptReg = 0;
  // Tells wheter we're currently processing an exception.
  bool exceptionTaken = false;
  // For use with Data/Instruction Storage/Segment exceptions.
  u64 exceptEA = 0;
  // Trap type
  u16 exceptTrapType = 0;
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
  // Thread Specific State.
  PPU_THREAD_REGISTERS ppuThread[2]{};
  // Current executing thread.
  PPU_THREAD currentThread = PPU_THREAD_0;
  // Shared Special Purpose Registers.
  PPU_STATE_SPRS SPR{};
  // Translation Lookaside Buffer
  TLB_Reg TLB{};
  // Address Traslation Flag
  bool traslationInProgress = false;
  // Current PPU Name, for ease of debugging.
  const char *ppuName = "";
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

  // Security engine Context
  u8 *secEngData = new u8[XE_SECENG_SIZE];
  SOCSECENG_BLOCK secEngBlock = {};
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
#define SPR_ESR 62
#define SPR_IVPR 63
#define SPR_PID 48
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

// Macros for ease of coding.
