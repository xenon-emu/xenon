/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "NAND.h"

NAND::NAND(std::weak_ptr<Xe::PCIDev::SFCX> sfcx) :
  SystemDevice(__func__, NAND_MEMORY_MAPPED_ADDR, NAND_MEMORY_MAPPED_ADDR + NAND_MEMORY_MAPPED_SIZE, true),
  sfcxDevice(sfcx)
{}

NAND::~NAND()
{}

void NAND::Read(u64 address, u8 *data, u64 size) {
  if (auto sfcx = sfcxDevice.lock())
    sfcx->ReadRaw(address, data, size);
}

void NAND::Write(u64 address, const u8 *data, u64 size) {
  if (auto sfcx = sfcxDevice.lock())
    sfcx->WriteRaw(address, data, size);
}

void NAND::MemSet(u64 address, s32 data, u64 size) {
  if (auto sfcx = sfcxDevice.lock())
    sfcx->MemSetRaw(address, data, size);
}