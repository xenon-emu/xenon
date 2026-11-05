/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"

// Register offsets, bit definitions and command codes shared by the HDD and ODD: the southbridge's ATA/ATAPI
// controller and its SATA status and control registers (SCR).
//
// Sources: the Xbox 360 Linux Kernel patches for the SiS-style SCR block in PCI configuration space.

// ATAPI (ODD) register offsets

// Offsets from the command block base.
constexpr u32 ATAPI_REG_DATA = 0x0;         // read/write
constexpr u32 ATAPI_REG_ERROR = 0x1;        // read
constexpr u32 ATAPI_REG_FEATURES = 0x1;     // write
constexpr u32 ATAPI_REG_INT_REAS = 0x2;     // interrupt reason, read
constexpr u32 ATAPI_REG_SECTOR_COUNT = 0x2; // write
constexpr u32 ATAPI_REG_LBA_LOW = 0x3;
constexpr u32 ATAPI_REG_BYTE_COUNT_LOW = 0x4;
constexpr u32 ATAPI_REG_BYTE_COUNT_HIGH = 0x5;
constexpr u32 ATAPI_REG_DEVICE = 0x6;
constexpr u32 ATAPI_REG_STATUS = 0x7;  // read
constexpr u32 ATAPI_REG_COMMAND = 0x7; // write

// This controller puts the alternate status (read) and device control (write) registers at 0xA.
constexpr u32 ATAPI_REG_ALTERNATE_STATUS = 0xA;
constexpr u32 ATAPI_REG_DEVICE_CONTROL = 0xA;

// Offsets from the bus master (DMA) block base.
constexpr u32 ATAPI_DMA_REG_COMMAND = 0x0;
constexpr u32 ATAPI_DMA_REG_STATUS = 0x2;
constexpr u32 ATAPI_DMA_REG_TABLE_OFFSET = 0x4; // physical region descriptor table address

// Bus master bits.
constexpr u32 XE_ATAPI_DMA_ACTIVE = 0x1;
constexpr u32 XE_ATAPI_DMA_WR = 0x8;

// ATA (HDD) register offsets

// Command block registers.
constexpr u32 ATA_REG_DATA = 0x0;     // read/write
constexpr u32 ATA_REG_ERROR = 0x1;    // read
constexpr u32 ATA_REG_FEATURES = 0x1; // write
constexpr u32 ATA_REG_SECTORCOUNT = 0x2;
constexpr u32 ATA_REG_LBA_LOW = 0x3;
constexpr u32 ATA_REG_LBA_MED = 0x4;
constexpr u32 ATA_REG_LBA_HI = 0x5;
constexpr u32 ATA_REG_DEV_SEL = 0x6;
constexpr u32 ATA_REG_STATUS = 0x7; // read
constexpr u32 ATA_REG_CMD = 0x7;    // write

// Control block registers.
constexpr u32 ATA_REG_ALT_STATUS = 0xA; // read
constexpr u32 ATA_REG_DEV_CTRL = 0xA;   // write

// SStatus, SError, SControl and the hotplug register can also be read through the bridge registers, no need to go
// through config space.
constexpr u32 ATA_REG_SSTATUS = 0x10;

// Bus master registers.
constexpr u32 ATA_REG_DMA_COMMAND = 0x0;
constexpr u32 ATA_REG_DMA_STATUS = 0x2;
constexpr u32 ATA_REG_DMA_TABLE_OFFSET = 0x4;

// Bus master status bits.
constexpr u32 XE_ATA_DMA_ACTIVE = 0x1;
constexpr u32 XE_ATA_DMA_ERR = 0x2;
constexpr u32 XE_ATA_DMA_INTR = 0x4;

// A PRD table with no end-of-table entry is cut off here. The kernel builds at most 36 entries: it fills a fixed
// buffer and traps past its end, caps a transfer at 0x20000 bytes and an entry at 0x10000, and splits any entry
// that would cross a 64 KiB physical boundary.
constexpr u32 XE_ATA_PRD_MAX_ENTRIES = 4096;

// Sector size of ATA disks.
constexpr u32 ATA_SECTOR_SIZE = 512;

// Status, error and device control bits

// Status: ERR (CHK for the packet and service commands), the last command ended in error.
constexpr u32 ATA_STATUS_ERR_CHK = 0x1;
// Status: DRQ, the device is ready to move data to or from the host.
constexpr u32 ATA_STATUS_DRQ = 0x08;
// Status: DRDY, the device accepts commands.
constexpr u32 ATA_STATUS_DRDY = 0x40;
// Error: ABRT, the command was aborted (invalid or unsupported command or parameter, or another error).
constexpr u32 ATA_ERROR_ABRT = 0x4;
// Device Control: nIEN, INTRQ is asserted while it is clear.
constexpr u32 ATA_DEVICE_CONTROL_NIEN = 0x02;

// Commands

constexpr u32 ATA_COMMAND_PACKET = 0xA0;
constexpr u32 ATA_COMMAND_IDENTIFY_PACKET_DEVICE = 0xA1;
constexpr u32 ATA_COMMAND_STANDBY_IMMEDIATE = 0xE0;
constexpr u32 ATA_COMMAND_IDLE_IMMEDIATE = 0xE1;
constexpr u32 ATA_COMMAND_IDENTIFY_DEVICE = 0xEC;
constexpr u32 ATA_COMMAND_SET_FEATURES = 0xEF;

// SET FEATURES subcommand.
constexpr u32 ATA_SF_SUBCOMMAND_SET_TRANSFER_MODE = 0x3;

// The ODD takes 12 byte packet command descriptor blocks.
constexpr u32 XE_ATAPI_CDB_SIZE = 12;

// Interrupt reason of an ATAPI device. C/D is clear for data and set for command or status. I/O is clear when the
// transfer goes to the device and set when it goes to the host.
constexpr u32 ATA_INTERRUPT_REASON_CD = 0x01;
constexpr u32 ATA_INTERRUPT_REASON_IO = 0x02;

constexpr u32 ATAPI_CDROM_SECTOR_SIZE = 2048;

// SATA status and control registers (SCR)
//
// The SCR block in PCI config space and its layout look like a SiS controller's (the Xbox 360 Linux Kernel patches
// treat it that way). Whether the southbridge's SATA really is SiS-based is unknown.
// TODO: Verify this.
//
// The block is 16 dword registers, the first five defined. Register n sits at the SCR base + n * 4.

constexpr u32 XE_SIS_SCR_BASE = 0xC0; // SATA0 PHY SCR registers base

// SControl.DET: device detection and interface initialisation.
constexpr u32 SCONTROL_DET_BITS = 0xF;
constexpr u32 SCONTROL_DET_NO_DEV = 0;  // no action requested
constexpr u32 SCONTROL_DET_INIT = 1;    // run the initialisation sequence (a hard reset)
constexpr u32 SCONTROL_DET_DISABLE = 4; // disable the interface and take the PHY offline
