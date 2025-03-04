// Copyright 2025 Xenon Emulator Project

#include "NAND.h"

#include "Base/Logging/Log.h"

/********************Responsible for loading the NAND file********************/
NAND::NAND(const char* deviceName, const std::string filePath,
  u64 startAddress, u64 endAddress,
  bool isSOCDevice) : SystemDevice(deviceName, startAddress, endAddress, isSOCDevice) {
  rawNANDData.resize(0x4000000);

  LOG_INFO(System, "NAND: Loading file {}", filePath.c_str());

  inputFile.open(filePath, std::ios_base::in | std::ios_base::binary);
  if (!inputFile.is_open()) {
    LOG_CRITICAL(System, "NAND: Unable to load file!");
    SYSTEM_PAUSE();
  }

  rawFileSize = std::filesystem::file_size(filePath);

  if (rawFileSize > 0x4000000) {
    LOG_ERROR(System, "NAND: Nand size exceeds 64MB! This may cause unintended behaviour");
    SYSTEM_PAUSE();
    rawNANDData.resize(rawFileSize);
  }

  LOG_INFO(System, "NAND: File size = {:#x} bytes.", rawFileSize);

  u8 magic[2]{};
  if (!CheckMagic(magic)) {
    LOG_ERROR(System, "NAND: Wrong magic found! Your nand contained {:#x} while the expected is 0xFF4F (retail) or 0x0F4F (devkit)", magic);
    SYSTEM_PAUSE();
  }

  const u32 blockSize = 0x4000;

  for (int currentBlock = 0; currentBlock < rawFileSize;
       currentBlock += blockSize) {
    inputFile.read(reinterpret_cast<char*>(rawNANDData.data() + currentBlock), blockSize);
  }

  inputFile.close();

  CheckSpare();

  if (hasSpare) {
    LOG_INFO(System, "NAND: Image has spare.");

    // Check Meta Type
    imageMetaType = DetectSpareType();
  }
}

NAND::~NAND() {
  rawNANDData.clear();
}

/************Responsible for reading the NAND************/
void NAND::Read(u64 readAddress, u64 *data, u8 byteCount) {
  u32 offset = (u32)readAddress & 0xFFFFFF;
  offset = 1 ? ((offset / 0x200) * 0x210) + offset % 0x200 : offset;
  LOG_DEBUG(SFCX, "Reading raw data at {:#x} (offset {:#x}) for {:#x} bytes", readAddress, offset, byteCount);
  memcpy(data, rawNANDData.data() + offset, byteCount);
}

/************Responsible for writing the NAND************/
void NAND::Write(u64 writeAddress, u64 data, u8 byteCount) {
  u32 offset = (u32)writeAddress & 0xFFFFFF;
  offset = 1 ? ((offset / 0x200) * 0x210) + offset % 0x200 : offset;
  LOG_DEBUG(SFCX, "Writing raw data at {:#x} (offset {:#x}) for {:#x} bytes", writeAddress, offset, byteCount);
  u8* NANDData = rawNANDData.data();
  memcpy(rawNANDData.data() + offset, &data, byteCount);
}

//*Checks ECD Page.
bool NAND::CheckPageECD(u8 *data, s32 offset) {
  u8 actualData[4]{};
  u8 calculatedECD[4]{};

  // This directly takes the NAND block's metadata
  // and skips to the last 4 ECC bytes, calculates the error corrrecting data on it, 
  // and checks against what the nand provided
  memcpy(actualData, rawNANDData.data() + (offset + 0x200 + 0xC), sizeof(actualData));

  CalculateECD(data, offset, calculatedECD);

  return (
      calculatedECD[0] == actualData[0] && calculatedECD[1] == actualData[1] &&
      calculatedECD[2] == actualData[2] && calculatedECD[3] == actualData[3]);
}

//*Calculates the ECD.
void NAND::CalculateECD(u8 *data, int offset, u8 ret[]) {
  u32 i, val = 0, v = 0;
  u32 count = 0;
  for (i = 0; i < 0x1066; i++) {
    if ((i & 31) == 0) {
      u32 value = u32((u8)(data[count + offset]) << 24 |
                      (u8)(data[count + offset + 1]) << 16 |
                      (u8)(data[count + offset + 2]) << 8 |
                      (u8)(data[count + offset + 3]));
      value = std::byteswap<u32>(value);
      v = ~value;
      count += 4;
    }
    val ^= v & 1;
    v >>= 1;
    if (val & 1)
      val ^= 0x6954559;
    val >>= 1;
  }
  val = ~val;
  ret[0] = (val << 6);
  ret[1] = (val >> 2) & 0xFF;
  ret[2] = (val >> 10) & 0xFF;
  ret[3] = (val >> 18) & 0xFF;
}

// Check if the nand provided to us contains a valid metadata or not
bool NAND::CheckMagic(u8 magicOut[2]) {
  u8 magic[2]{};

  inputFile.read(reinterpret_cast<char*>(magic), sizeof(magic));
  inputFile.seekg(0, std::ios::beg);
  if (magicOut) {
    memcpy(magicOut, magic, sizeof(magic));
  }

  if ((magic[0] == 0xFF || magic[0] == 0x0F) &&
      (magic[1] == 0x3F || magic[1] == 0x4F)) {
    return true;
  }
  return false;
}

//*Checks Spare.
void NAND::CheckSpare() {
  // Each sectors of data is 512 bytes of data,
  // and 16 bytes of space (0x210)
  // We are taking those 16 bytes, which is the NAND's metadata structure,
  // and checking the ECD on it
  // 
  // We take 3 blocks, and verify that they all have no invalid data
  // If we have no invalid data, then we do not have any spare
  u8 data[0x630]{};
  memcpy(data, rawNANDData.data(), sizeof(data));
  hasSpare = true;
  u8 *spare = nullptr;

  for (int idx = 0; idx < sizeof(data); idx += 0x210) {
    if (!CheckPageECD(data, idx)) {
      hasSpare = false;
    }
  }
}

//*Detects Spare Type.
MetaType NAND::DetectSpareType(bool firstTry) {
  if (!hasSpare) {
    return metaTypeNone;
  }

  u8 tmp[0x10]{};
  memcpy(tmp, rawNANDData.data() + (firstTry ? 0x4400 : (u32)rawFileSize - 0x4400), sizeof(tmp));

  return metaTypeNone;
}
