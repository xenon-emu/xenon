// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "NAND.h"
#include "Base/Logging/Log.h"

NAND::NAND(const std::string &deviceName, Xe::PCIDev::SFCX *sfcx, bool isSOCDevice) :
  SystemDevice(deviceName, NAND_MEMORY_MAPPED_ADDR, NAND_MEMORY_MAPPED_ADDR + NAND_MEMORY_MAPPED_SIZE, isSOCDevice),
  sfcxDevice(sfcx)
{}

NAND::~NAND()
{}

void NAND::Read(u64 readAddress, u8 *data, u64 size) {
  sfcxDevice->ReadRaw(readAddress, data, size);
}

void NAND::Write(u64 writeAddress, const u8 *data, u64 size) {
  sfcxDevice->WriteRaw(writeAddress, data, size);
}

void NAND::MemSet(u64 writeAddress, s32 data, u64 size) {
  sfcxDevice->MemSetRaw(writeAddress, data, size);
}
