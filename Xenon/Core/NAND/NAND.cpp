// Copyright 2025 Xenon Emulator Project

#include "NAND.h"
#include "Base/Logging/Log.h"

//#define NAND_DEBUG

/********************Responsible for loading the NAND file********************/
NAND::NAND(const char* deviceName, const std::string filePath,
  u64 startAddress, u64 endAddress,
  bool isSOCDevice) : SystemDevice(deviceName, startAddress, endAddress, isSOCDevice) {
  rawNANDData.resize(0x4200000);

  LOG_INFO(System, "NAND: Loading file {}", filePath.c_str());

  inputFile.open(filePath, std::ios_base::in | std::ios_base::binary);
  if (!inputFile.is_open()) {
    LOG_CRITICAL(System, "NAND: Unable to load file!");
    SYSTEM_PAUSE();
  }

  rawFileSize = std::filesystem::file_size(filePath);

  if (rawFileSize > 0x4200000) {
    LOG_ERROR(System, "NAND: Nand size exceeds 64MB! This may cause unintended behaviour");
    SYSTEM_PAUSE();
    rawNANDData.resize(rawFileSize);
  }

  LOG_INFO(System, "NAND: File size = {:#x} bytes.", rawFileSize);

  u8 magic[2]{};
  if (!CheckMagic(magic)) {
    u16 magicLE = (magic[0] << 8) | (magic[1] << 0);
    LOG_ERROR(System, "NAND: Wrong magic found! Your nand contained {:#x} while the expected is 0xFF4F (retail) or 0x0F4F (devkit)", magicLE);
    SYSTEM_PAUSE();
  }

  constexpr u32 blockSize = 0x4000;

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
  u32 offset = static_cast<u32>(readAddress & 0xFFFFFF);
  offset = 1 ? ((offset / 0x200) * 0x210) + offset % 0x200 : offset;
#ifdef NAND_DEBUG
    LOG_DEBUG(SFCX, "Reading raw data at {:#x} (offset {:#x}) for {:#x} bytes", readAddress, offset, byteCount);
#endif // NAND_DEBUG
  memcpy(data, rawNANDData.data() + offset, byteCount);
}

/************Responsible for writing the NAND************/
void NAND::Write(u64 writeAddress, u64 data, u8 byteCount) {
  u32 offset = static_cast<u32>(writeAddress & 0xFFFFFF);
  offset = 1 ? ((offset / 0x200) * 0x210) + offset % 0x200 : offset;
#ifdef NAND_DEBUG
    LOG_DEBUG(SFCX, "Writing raw data at {:#x} (offset {:#x}) for {:#x} bytes", writeAddress, offset, byteCount);
#endif // NAND_DEBUG
  u8* NANDData = rawNANDData.data();
  memcpy(rawNANDData.data() + offset, &data, byteCount);
}

//*Checks ECD Page.
bool NAND::CheckPageECD(u8 *data, s32 offset) {
  NANDMetadata metadata{};
  NANDMetadata calculatedMetadata{};

  // This directly takes the NAND block's metadata
  // and calculates the error corrrecting data on the ECC bytes 
  // and checks against what the nand provided
  memcpy(&metadata, rawNANDData.data() + (offset + 0x200), sizeof(metadata));

  CalculateECD(data, offset, calculatedMetadata);

  return (
    metadata.ECC3 == calculatedMetadata.ECC3 &&
    metadata.ECC2 == calculatedMetadata.ECC2 &&
    metadata.ECC1 == calculatedMetadata.ECC1 &&
    metadata.ECC0 == calculatedMetadata.ECC0
  );
}

//*Calculates the ECD.
void NAND::CalculateECD(u8 *data, int offset, NANDMetadata &metadata) {
  u32 i, val = 0, v = 0;
  u32 count = 0;
  for (i = 0; i < 0x1066; i++) {
    if ((i & 31) == 0) {
      v = ~byteswap_be<u32>(
        static_cast<u8>(data[count + offset + 0]) << 24 |
        static_cast<u8>(data[count + offset + 1]) << 16 |
        static_cast<u8>(data[count + offset + 2]) << 8  |
        static_cast<u8>(data[count + offset + 3])
      );
      count += 4;
    }
    val ^= v & 1;
    v >>= 1;
    if (val & 1)
      val ^= 0x6954559;
    val >>= 1;
  }
  val = ~val;
  // Shift ECC3 to the upper 2 bits
  metadata.ECC3 = (val << 6);
  metadata.ECC2 = (val >> 2) & 0xFF;
  metadata.ECC1 = (val >> 10) & 0xFF;
  metadata.ECC0 = (val >> 18) & 0xFF;
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
  memcpy(tmp, rawNANDData.data() + (firstTry ? 0x4400 : static_cast<u32>(rawFileSize) - 0x4400), sizeof(tmp));

  return metaTypeNone;
}
