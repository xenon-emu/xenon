// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Base/Types.h"

//
// Common PCI Config Registers
//

union PCI_CONFIG_HDR_REG0 {
  u32 hexData;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 vendorID : 16;
    u32 deviceID : 16;
  };
#else
  struct {
    u32 deviceID : 16;
    u32 vendorID : 16;
  };
#endif
};

union PCI_CONFIG_HDR_REG1_COMMAND_REG {
  u16 hexData;
#ifdef __LITTLE_ENDIAN__
  struct {
    u16 ioSpace : 1;
    u16 memorySpace : 1;
    u16 busMaster : 1;
    u16 specialCycles : 1;
    u16 writeMemAndInvEn : 1;
    u16 vgaPaletteSnoop : 1;
    u16 parityErrorResp : 1;
    u16 res0 : 1;
    u16 SERR_En : 1;
    u16 fastBackToBackEn : 1;
    u16 interruptDisable : 1;
    u16 res1 : 5;
  };
#else
  struct {
    u16 res1 : 5;
    u16 interruptDisable : 1;
    u16 fastBackToBackEn : 1;
    u16 SERR_En : 1;
    u16 res0 : 1;
    u16 parityErrorResp : 1;
    u16 vgaPaletteSnoop : 1;
    u16 writeMemAndInvEn : 1;
    u16 specialCycles : 1;
    u16 busMaster : 1;
    u16 memorySpace : 1;
    u16 ioSpace : 1;
  };
#endif
};

union PCI_CONFIG_HDR_REG1 {
  u32 hexData;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 status : 16;
    u32 command : 16;
  };
#else
  struct {
    u32 command : 16;
    u32 status : 16;
  };
#endif
};

union PCI_CONFIG_HDR_REG2 {
  u32 hexData;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 revID : 8;
    u32 progIF : 8;
    u32 subclass : 8;
    u32 classCode : 8;
  };
#else
  struct {
    u32 classCode : 8;
    u32 subclass : 8;
    u32 progIF : 8;
    u32 revID : 8;
  };
#endif
};

union PCI_CONFIG_HDR_REG3 {
  u32 hexData;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 cacheLineSize : 8;
    u32 latencyTimer : 8;
    u32 headerType : 8;
    u32 BIST : 8;
  };
#else
  struct {
    u32 BIST : 8;
    u32 headerType : 8;
    u32 latencyTimer : 8;
    u32 cacheLineSize : 8;
  };
#endif
};

//
// General PCI Device Config Space Registers. Type 0x0
//

union GENERAL_PCI_CONFIG_HDR_REGB {
  u32 hexData;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 subsystemVenID : 16;
    u32 susbsystemID : 16;
  };
#else
  struct {
    u32 susbsystemID : 16;
    u32 subsystemVenID : 16;
  };
#endif
};

union GENERAL_PCI_CONFIG_HDR_REGD {
  u32 hexData;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 pointerCapabilities : 8;
    u32 res : 24;
  };
#else
  struct {
    u32 res : 24;
    u32 pointerCapabilities : 8;
  };
#endif
};

union GENERAL_PCI_CONFIG_HDR_REGF {
  u32 hexData;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 interruptLine : 8;
    u32 interruptPin : 8;
    u32 minGrant : 8;
    u32 maxLatency : 8;
  };
#else
  struct {
    u32 maxLatency : 8;
    u32 minGrant : 8;
    u32 interruptPin : 8;
    u32 interruptLine : 8;
  };
#endif
};

struct GENRAL_PCI_DEVICE_CONFIG_HDR {
  // Offset 0x0
  PCI_CONFIG_HDR_REG0 reg0;
  // Offset 0x4
  PCI_CONFIG_HDR_REG1 reg1;
  // Offset 0x8
  PCI_CONFIG_HDR_REG2 reg2;
  // Offset 0xC
  PCI_CONFIG_HDR_REG3 reg3;
  // Offset 0x10
  u32 BAR0;
  // Offset 0x14
  u32 BAR1;
  // Offset 0x18
  u32 BAR2;
  // Offset 0x1C
  u32 BAR3;
  // Offset 0x20
  u32 BAR4;
  // Offset 0x24
  u32 BAR5;
  // Offset 0x28
  u32 cardBusCisPointer;
  // Offset 0x2C
  GENERAL_PCI_CONFIG_HDR_REGB regB;
  // Offset 0x30
  u32 expansionROMBaseAddr;
  // Offset 0x34
  GENERAL_PCI_CONFIG_HDR_REGD regD;
  // Offset 0x38
  u32 reserved;
  // Offset 0x3C
  GENERAL_PCI_CONFIG_HDR_REGF regF;
};

union GENRAL_PCI_DEVICE_CONFIG_SPACE {
  u8 data[0x100];
  GENRAL_PCI_DEVICE_CONFIG_HDR configSpaceHeader;
};

//
// PCI to PCI Bridge Config Header. Typo 0x1
//

union PCI_PCI_BRIDGE_CONFIG_HDR_REG6 {
  u32 hexData;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 primaryBusNum : 8;
    u32 secondaryBusNum : 8;
    u32 subordinateBusNum : 8;
    u32 secondaryLatencyTimer : 8;
  };
#else
  struct {
    u32 secondaryLatencyTimer : 8;
    u32 subordinateBusNum : 8;
    u32 secondaryBusNum : 8;
    u32 primaryBusNum : 8;
  };
#endif
};

union PCI_PCI_BRIDGE_CONFIG_HDR_REG7 {
  u32 hexData;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 ioBase : 8;
    u32 ioLimit : 8;
    u32 secondaryStatus : 16;
  };
#else
  struct {
    u32 secondaryStatus : 16;
    u32 ioLimit : 8;
    u32 ioBase : 8;
  };
#endif
};

union PCI_PCI_BRIDGE_CONFIG_HDR_REG8 {
  u32 hexData;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 memoryBase : 16;
    u32 memoryLimit : 16;
  };
#else
  struct {
    u32 memoryLimit : 16;
    u32 memoryBase : 16;
  };
#endif
};

union PCI_PCI_BRIDGE_CONFIG_HDR_REG9 {
  u32 hexData;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 prefetchableMemoryBase : 16;
    u32 prefetchableMemoryLimit : 16;
  };
#else
  struct {
    u32 prefetchableMemoryLimit : 16;
    u32 prefetchableMemoryBase : 16;
  };
#endif
};

union PCI_PCI_BRIDGE_CONFIG_HDR_REGC {
  u32 hexData;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 ioBaseUpper16bits : 16;
    u32 ioLimitUpper16bits : 16;
  };
#else
  struct {
    u32 ioLimitUpper16bits : 16;
    u32 ioBaseUpper16bits : 16;
  };
#endif
};

union PCI_PCI_BRIDGE_CONFIG_HDR_REGD {
  u32 hexData;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 capabilityPointer : 8;
    u32 reserved : 24;
  };
#else
  struct {
    u32 reserved : 24;
    u32 capabilityPointer : 8;
  };
#endif
};

union PCI_PCI_BRIDGE_CONFIG_HDR_REGF {
  u32 hexData;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 interruptLine : 8;
    u32 interruptPin : 8;
    u32 bridgeControl : 16;
  };
#else
  struct {
    u32 bridgeControl : 16;
    u32 interruptPin : 8;
    u32 interruptLine : 8;
  };
#endif
};

struct PCI_PCI_BRIDGE_CONFIG_HEADER {
  // Offset 0x0
  PCI_CONFIG_HDR_REG0 reg0;
  // Offset 0x4
  PCI_CONFIG_HDR_REG1 reg1;
  // Offset 0x8
  PCI_CONFIG_HDR_REG2 reg2;
  // Offset 0xC
  PCI_CONFIG_HDR_REG3 reg3;
  // Offset 0x10
  u32 BAR0;
  // Offset 0x14
  u32 BAR1;
  // Offset 0x18
  PCI_PCI_BRIDGE_CONFIG_HDR_REG6 reg6;
  // Offset 0x1C
  PCI_PCI_BRIDGE_CONFIG_HDR_REG7 reg7;
  // Offset 0x20
  PCI_PCI_BRIDGE_CONFIG_HDR_REG8 reg8;
  // Offset 0x24
  PCI_PCI_BRIDGE_CONFIG_HDR_REG9 reg9;
  // Offset 0x28
  u32 prefetchableBaseUpper32Bits;
  // Offset 0x2C
  u32 prefetchableLimitUpper32Bits;
  // Offset 0x30
  PCI_PCI_BRIDGE_CONFIG_HDR_REGC regC;
  // Offset 0x34
  PCI_PCI_BRIDGE_CONFIG_HDR_REGD regD;
  // Offset 0x38
  u32 expansionROMBaseAddress;
  // Offset 0x3C
  PCI_PCI_BRIDGE_CONFIG_HDR_REGF regF;
};

union PCI_PCI_BRIDGE_CONFIG_SPACE {
  u8 data[0xFF];
  PCI_PCI_BRIDGE_CONFIG_HEADER configSpaceHeader;
};

// PCIe Configuration Space Address decoding.
union PCIE_CONFIG_ADDR {
  u32 hexData;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 regOffset : 8;
    u32 extendedReg : 4;
    u32 functNum : 3;
    u32 devNum : 5;
    u32 busNum : 8;
    u32 res0 : 7;
    u32 enable : 1;
  };
#else
  struct {
    u32 enable : 1;
    u32 res0 : 7;
    u32 busNum : 8;
    u32 devNum : 5;
    u32 functNum : 3;
    u32 extendedReg : 4;
    u32 regOffset : 8;
  };
#endif
};
