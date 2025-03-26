// Copyright 2025 Xenon Emulator Project

#pragma once

#include <fstream>
#include <filesystem>
#include <vector>

#include "Base/SystemDevice.h"

#define NAND_START_ADDR 0xC8000000
#define NAND_END_ADDR 0xCC000000 // 64 Mb region

enum MetaType {
  metaType0 = 0,             // Pre Jasper (0x01198010)
  metaType1 = 1,             // Jasper, Trinity & Corona (0x00023010 [Jasper
                             // & Trinity] and 0x00043000 [Corona])
  metaType2 = 2,             // BigBlock Jasper (0x008A3020 and 0x00AA3020)
  metaTypeUninitialized = 3, // Really old JTAG XeLL images
  metaTypeNone = 4           // No spare type or unknown
};

struct NANDMetadata {
  u8 BlockID1; // LBA/ID = (((BlockID0 & 0xF) << 8) + (BlockID1))
  u8 BlockID0 : 4;
  u8 FsUnused0 : 4;
  u8 FsSequence0; // Not reversed
  u8 FsSequence1;
  u8 FsSequence2;
  u8 BadBlock;
  u8 FsSequence3;
  u8 FsSize1; // ((FsSize0 << 8) + FsSize1) = cert size
  u8 FsSize0;
  u8 FsPageCount; // Free pages left in block (ie: If 3 pages are used by cert then this would be 29:0x1D)
  u8 FsUnused1[0x2];
  u8 FsBlockType : 6;
  u8 ECC3 : 2;
  u8 ECC2; // 14 bit ECD
  u8 ECC1;
  u8 ECC0;
};

class NAND : public SystemDevice {
public:
  NAND(const char *deviceName, const std::string filePath,
    u64 startAddress, u64 endAddress,
    bool isSOCDevice);
  ~NAND();

  void Read(u64 readAddress, u8 *data, u8 byteCount) override;
  void Write(u64 writeAddress, const u8 *data, u8 byteCount) override;

private:
  std::ifstream inputFile;

  // 64 Mb NAND Data
  std::vector<u8> rawNANDData{};

  bool CheckMagic(u8 magicOut[2] = nullptr);
  void CheckSpare();
  bool CheckPageECD(u8 *data, s32 offset);
  void CalculateECD(u8 *data, int offset, NANDMetadata& metadata);
  MetaType DetectSpareType(bool firstTry = true);

  // void SeekPos(s32 address);

  size_t rawFileSize = 0;
  bool hasSpare = false;
  MetaType imageMetaType = MetaType::metaTypeNone;
};
