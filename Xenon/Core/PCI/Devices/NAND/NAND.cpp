/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "NAND.h"

NAND::NAND(std::weak_ptr<Xe::PCIDev::SFCX> sfcx) :
  SystemDevice(__func__, NAND_MEMORY_MAPPED_ADDR, NAND_MEMORY_MAPPED_ADDR + NAND_MEMORY_MAPPED_SIZE, true),
  sfcxDevice(sfcx)
{}

NAND::~NAND() {
  RetireAndWait();
}

void NAND::Read(u64 address, u8 *data, u64 size) {
  auto lease = GetLease();
  if (!lease) {
    return;
  }
  if (auto sfcx = sfcxDevice.lock()) {
    auto sfcxLease = sfcx->GetLease();
    if (sfcxLease) {
      sfcx->ReadRaw(address, data, size);
    }
  }
}

void NAND::Write(u64 address, const u8 *data, u64 size) {
  auto lease = GetLease();
  if (!lease) {
    return;
  }
  if (auto sfcx = sfcxDevice.lock()) {
    auto sfcxLease = sfcx->GetLease();
    if (sfcxLease) {
      sfcx->WriteRaw(address, data, size);
    }
  }
}

void NAND::MemSet(u64 address, s32 data, u64 size) {
  auto lease = GetLease();
  if (!lease) {
    return;
  }
  if (auto sfcx = sfcxDevice.lock()) {
    auto sfcxLease = sfcx->GetLease();
    if (sfcxLease) {
      sfcx->MemSetRaw(address, data, size);
    }
  }
}