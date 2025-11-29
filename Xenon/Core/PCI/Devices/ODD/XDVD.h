/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"

//
// This file contains various definitions used in the Xbox Game Disc format and auth protocols
//

// Sources:
// https://web.archive.org/web/20230331163919/https://multimedia.cx/eggs/xbox-sphinx-protocol/
// https://xboxdevwiki.net/Xbox_Game_Disc
// https://xboxdevwiki.net/DVD_Drive
// https://github.com/XboxDev/extract-xiso
// https://github.com/xemu-project/xemu/pull/1659/
// http://abgx360.hadzz.com/

// Drive control page for XDVD.
#define XMODE_PAGE_DRIVE_CONTOL   0x20
// Page Code for Xbox 360 DVD Key based auth.
#define XMODE_PAGE_DVD_KEY_AUTH   0x3B
// Page Code for Xbox Security Challenges.
#define XMODE_PAGE_XBOX_SECURITY  0x3E
// XDVD Structure Layer and Block Num.
#define XDVD_STRUCTURE_LAYER 0xFE
#define XDVD_STRUCTURE_BLOCK_NUMBER 0xFF02FDFF
// XDVD Structure Lenght
#define XDVD_STRUCTURE_LEN 0x664

// Spindle speeds for XDVD drives
// According to abgx360.
enum eXDVDSpindleSpeed : u8 {
  spindleSpeedStopped = 0,
  spindleSpeed2x = 1,
  spindleSpeed5x = 2,
  spindleSpeed8x = 3,
  spindleSpeed12x = 4
};

// Publicly available Mode Sense/Select (10 byte)
struct sMODE_SENSE10 {
  u8 OperationCode;
  u8 Reserved1 : 3;
  u8 Dbd : 1;
  u8 Reserved2 : 1;
  u8 LogicalUnitNumber : 3;
  u8 PageCode : 6;
  u8 Pc : 2;
  u8 Reserved3[4];
  u8 AllocationLength[2];
  u8 Control;
};

struct sMODE_SELECT10 {
  u8 OperationCode;
  u8 SPBit : 1;
  u8 Reserved1 : 3;
  u8 PFBit : 1;
  u8 LogicalUnitNumber : 3;
  u8 Reserved2[5];
  u8 ParameterListLength[2];
  u8 Control;
};

struct sREAD_DVD_STRUCTURE {
  u8 OperationCode;    // 0xAD - SCSIOP_READ_DVD_STRUCTURE
  u8 Reserved1 : 5;
  u8 Lun : 3;
  u8 RMDBlockNumber[4];
  u8 LayerNumber;
  u8 Format;
  u8 AllocationLength[2];
  u8 Reserved3 : 6;
  u8 AGID : 2;
  u8 Control;
};

// Standard SCSI Mode Sense/Select header (10-byte)
struct sMODE_PARAMETER_HEADER10 {
  u8 ModeDataLength[2];
  u8 MediumType;
  u8 DeviceSpecificParameter;
  u8 Reserved[2];
  u8 BlockDescriptorLength[2];
};

// https://xboxdevwiki.net/DVD_Drive
struct sXBOX_DVD_AUTH_PAGE {
  u8 PageCode;                // 0x20
  u8 PageLength;
  u8 Partition;               // Current partition, 0 = DVDVideo, 1 = Xbox Game Data
  u8 Unk1;
  u8 Authenticated;           // Is the drive authenticated?
  u8 DiscCategoryAndVersion;  // Code inside xboxkrnl refers to this parameter, and shows category and version mismatch 
                              // when incorrect.
  u8 Unk2;
  u8 ChallengeID;
  u32 ChallengeValue;
  u32 ResponseValue;
  u32 reserved;
};

struct sXBOX_DVD_SECURITY {
  sMODE_PARAMETER_HEADER10 header;
  sXBOX_DVD_AUTH_PAGE authPage;
};

// https://web.archive.org/web/20240316195746/https://multimedia.cx/eggs/xbox-sphinx-protocol/
struct sXBOX_DVD_CHALLENGE {
  u8 type;
  u8 id;
  u32 challenge;
  u8 reserved;
  u32 response;
};