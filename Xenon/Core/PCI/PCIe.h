/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"

// The type 0 config header every function on the bus presents, as unions over its dwords.

// Common PCI config registers

union PCI_ID_REG {
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

union PCI_COMMAND_REG {
  u32 hexData;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 status  : 16;
    u32 command : 16;
  };
#else
  struct {
    u32 command : 16;
    u32 status  : 16;
  };
#endif
};

union PCI_CLASS_REG {
  u32 hexData;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 revID     : 8;
    u32 progIF    : 8;
    u32 subclass  : 8;
    u32 classCode : 8;
  };
#else
  struct {
    u32 classCode : 8;
    u32 subclass  : 8;
    u32 progIF    : 8;
    u32 revID     : 8;
  };
#endif
};

union PCI_HEADER_REG {
  u32 hexData;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 cacheLineSize : 8;
    u32 latencyTimer  : 8;
    u32 headerType    : 8;
    u32 BIST          : 8;
  };
#else
  struct {
    u32 BIST          : 8;
    u32 headerType    : 8;
    u32 latencyTimer  : 8;
    u32 cacheLineSize : 8;
  };
#endif
};

// General PCI device config registers (type 0x0)

union PCI_SUBSYSTEM_REG {
  u32 hexData;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 subsystemVendorID : 16;
    u32 subsystemID       : 16;
  };
#else
  struct {
    u32 subsystemID       : 16;
    u32 subsystemVendorID : 16;
  };
#endif
};

union PCI_CAPABILITIES_REG {
  u32 hexData;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 pointerCapabilities : 8;
    u32 res                 : 24;
  };
#else
  struct {
    u32 res                 : 24;
    u32 pointerCapabilities : 8;
  };
#endif
};

union PCI_INTERRUPT_REG {
  u32 hexData;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 interruptLine : 8;
    u32 interruptPin  : 8;
    u32 minGrant      : 8;
    u32 maxLatency    : 8;
  };
#else
  struct {
    u32 maxLatency    : 8;
    u32 minGrant      : 8;
    u32 interruptPin  : 8;
    u32 interruptLine : 8;
  };
#endif
};

struct PCI_TYPE0_HEADER {
  // Offset 0x0
  PCI_ID_REG id;
  // Offset 0x4
  PCI_COMMAND_REG commandStatus;
  // Offset 0x8
  PCI_CLASS_REG classRev;
  // Offset 0xC
  PCI_HEADER_REG headerInfo;
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
  PCI_SUBSYSTEM_REG subsystem;
  // Offset 0x30
  u32 expansionROMBaseAddr;
  // Offset 0x34
  PCI_CAPABILITIES_REG capabilities;
  // Offset 0x38
  u32 reserved;
  // Offset 0x3C
  PCI_INTERRUPT_REG interrupt;
};

union PCI_TYPE0_CONFIG_SPACE {
  u8 data[0x100];
  PCI_TYPE0_HEADER configSpaceHeader;
};

static_assert(sizeof(PCI_TYPE0_HEADER) == 0x40, "The type 0 header is 16 dwords");
static_assert(sizeof(PCI_TYPE0_CONFIG_SPACE) == 0x100, "Configuration space is 256 bytes");
