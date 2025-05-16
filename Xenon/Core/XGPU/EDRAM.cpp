#include "EDRAM.h"

Xe::XGPU::EDRAM::EDRAM() {
  // Setup the EDRAM State.
  edramState = std::make_unique<STRIP_UNIQUE(edramState)>();
  // Allocate our register space.
  edramState.get()->edramRegs.resize(MAX_EDRAM_REGS);

  // EDRAM Rev & ID.
  edramState.get()->edramRegs[0x2000] = 0x00d10020;
  // Set Valid CRC's in registers?
  edramState.get()->reg41Data.push_back(0xEBBCB7D0);
  edramState.get()->reg41Data.push_back(0xB7599E02);
  edramState.get()->reg41Data.push_back(0x0AEA2A7A);
  edramState.get()->reg41Data.push_back(0x2CABD6B8);
  edramState.get()->reg41Data.push_back(0xA5A5A5A5);
  edramState.get()->reg41Data.push_back(0xA5A5A5A5);

  edramState.get()->reg1041Data.push_back(0xE57C27BE);
  edramState.get()->reg1041Data.push_back(0x43FA90AA);
  edramState.get()->reg1041Data.push_back(0x9D065F66);
  edramState.get()->reg1041Data.push_back(0x360A6AD8);
  edramState.get()->reg1041Data.push_back(0xA5A5A5A5);
  edramState.get()->reg1041Data.push_back(0xA5A5A5A5);

  // CRC AZ0_BC Data.
  edramState.get()->az0Data.push_back(0xA5A5A5A5);
  edramState.get()->az0Data.push_back(0xEBBCB7D0);
  edramState.get()->az0Data.push_back(0xB7599E02);
  edramState.get()->az0Data.push_back(0x0AEA2A7A);
  edramState.get()->az0Data.push_back(0x2CABD6B8);
  edramState.get()->az0Data.push_back(0xA5A5A5A5);
  // CRC AZ1_BC Data.
  edramState.get()->az1Data.push_back(0xA5A5A5A5);
  edramState.get()->az1Data.push_back(0xE57C27BE);
  edramState.get()->az1Data.push_back(0x43FA90AA);
  edramState.get()->az1Data.push_back(0x9D065F66);
  edramState.get()->az1Data.push_back(0x360A6AD8);
  edramState.get()->az1Data.push_back(0xA5A5A5A5);
}

Xe::XGPU::EDRAM::~EDRAM() {
  edramState.reset();
}

void Xe::XGPU::EDRAM::SetRWRegIndex(eRegIndexType indexType, u32 index) {
  switch (indexType) {
  case Xe::XGPU::readIndex:
    edramState.get()->readRegisterIndex = index;
    break;
  case Xe::XGPU::writeIndex:
    edramState.get()->writeRegisterIndex = index;
    break;
  }
}

u32 Xe::XGPU::EDRAM::ReadReg() {
  // Set status to busy.
  edramState.get()->edramBusy = true;

  u32 regValue = 0;
  u32 regIdx = edramState.get()->readRegisterIndex & 0xFFFF;

  // Read the register at the specified read offset.
  // Sanity Check.
  if (regIdx <= edramState.get()->edramRegs.size()) {
    if (regIdx == 0x41) {
      regValue = edramState.get()->reg41Data[edramState.get()->reg41Index];
      edramState.get()->reg41Index++;
      if (edramState.get()->reg41Index >= 6) { edramState.get()->reg41Index = 0; }
    } else if (regIdx == 0x1041) {
      regValue = edramState.get()->reg1041Data[edramState.get()->reg1041Index];
      edramState.get()->reg1041Index++;
      if (edramState.get()->reg1041Index >= 6) { edramState.get()->reg1041Index = 0; }
    }
    else { regValue = byteswap_be<u32>(edramState.get()->edramRegs[regIdx]); }
  } else {
    LOG_ERROR(Xenos, "[EDRAM]: Read register index is bigger than MAX_EDRAM_REGS, index = {:#x}",
      edramState.get()->readRegisterIndex);
  }
  // Clear read register offset.
  edramState.get()->readRegisterIndex = 0;

  // Clear status.
  edramState.get()->edramBusy = false;

  return regValue;
}

void Xe::XGPU::EDRAM::WriteReg(u32 data) {
  // Set status to busy.
  edramState.get()->edramBusy = true;

  u32 regIdx = edramState.get()->writeRegisterIndex & 0xFFFF;

  // Read the register at the specified read offset.
  // Sanity Check.
  if (regIdx <= edramState.get()->edramRegs.size()) {
    edramState.get()->edramRegs[regIdx] = byteswap_be<u32>(data);
  } else {
    LOG_ERROR(Xenos, "[EDRAM]: Write register index is bigger than MAX_EDRAM_REGS, index = {:#x}",
      edramState.get()->writeRegisterIndex);
  }
  // Clear Write register offset.
  edramState.get()->writeRegisterIndex = 0;

  // Clear status.
  edramState.get()->edramBusy = false;
}

u32 Xe::XGPU::EDRAM::ReadCRC_AZ0_BC() {
  u32 data = edramState.get()->az0Data[edramState.get()->az0BCRegIndex];
  edramState.get()->az0BCRegIndex++;
  if (edramState.get()->az0BCRegIndex >= 6) { edramState.get()->az0BCRegIndex = 0; }
  return byteswap_be<u32>(data);
}

u32 Xe::XGPU::EDRAM::ReadCRC_AZ1_BC() {
  u32 data = edramState.get()->az1Data[edramState.get()->az1BCRegIndex];
  edramState.get()->az1BCRegIndex++;
  if (edramState.get()->az1BCRegIndex >= 6) { edramState.get()->az1BCRegIndex = 0; }
  return byteswap_be<u32>(data);
}
