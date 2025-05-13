#include "EDRAM.h"

Xe::XGPU::EDRAM::EDRAM() {
  // Setup the EDRAM State.
  edramState = std::make_unique<STRIP_UNIQUE(edramState)>();
  // Allocate our register space.
  edramState.get()->edramRegs.resize(MAX_EDRAM_REGS);

  // EDRAM Rev & ID.
  edramState.get()->edramRegs[0x2000] = 0x00d10020;
}

Xe::XGPU::EDRAM::~EDRAM() {
  edramState.reset();
}

void Xe::XGPU::EDRAM::SetRWRegIndex(eRegIndexType indexType, u32 index) {
  switch (indexType)
  {
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
    regValue = byteswap_be<u32>(edramState.get()->edramRegs[regIdx]);
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
  }
  else {
    LOG_ERROR(Xenos, "[EDRAM]: Write register index is bigger than MAX_EDRAM_REGS, index = {:#x}",
      edramState.get()->writeRegisterIndex);
  }
  // Clear read register offset.
  edramState.get()->writeRegisterIndex = 0;

  // Clear status.
  edramState.get()->edramBusy = false;
}
