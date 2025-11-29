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

// Page Code for Xbox Security Challenges
#define XMODE_PAGE_XBOX_SECURITY 0x3E

// Standard SCSI Mode Sense/Select header (10-byte)
struct sMODE_PARAMETER_HEADER10 {
  u8 ModeDataLength[2];
  u8 MediumType;
  u8 DeviceSpecificParameter;
  u8 Reserved[2];
  u8 BlockDescriptorLength[2];
};

// https://xboxdevwiki.net/DVD_Drive
struct sXBOX_DVD_SECURITY_PAGE {
  u8 PageCode;
  u8 PageLength;
  u8 Partition;           // Current partition, 0 = DVDVideo, 1 = Xbox Game Data
  u8 Unk1;
  u8 Authenticated;       // Is the drive authenticated?
  u8 BookTypeAndVersion;
  u8 Unk2;
  u8 ChallengeID;
  u32 ChallengeValue;
  u32 ResponseValue;
  u32 Unk3;
};

typedef struct sXBOX_DVD_SECURITY {
  sMODE_PARAMETER_HEADER10 header;
  sXBOX_DVD_SECURITY_PAGE securityPage;
};

// https://web.archive.org/web/20240316195746/https://multimedia.cx/eggs/xbox-sphinx-protocol/
struct sXBOX_DVD_CHALLENGE {
  u8 type;
  u8 id;
  u32 challenge;
  u8 reserved;
  u32 response;
};