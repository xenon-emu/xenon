/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <stdlib.h>

#include "Base/Types.h"
#include "Base/Logging/Log.h"

namespace Xe::XGPU {

// In reality the higher register i've seen in use is 0x1403, but just for safety i'll set this to a higher value.
// As usual, actual register offset is value * 4.
#define MAX_EDRAM_REGS 0x500F

// So EDRAM (SBI in xboxkrnl) Register Read goes like this:
// 1. Read a certain amount of times to RB_SIDEBAND_BUSY. In xboxkrnl the limit is 100 reads,
// for libxenon, it will get stuck indefinitely waiting for the register to clear.
// If the register isn't cleared within 100 reads, xboxkrnl will complain and fail the read/write op.
// else it will continue to step 2.
// 2. For Reads, it will set RB_SIDEBAND_RD_ADDR to the desired EDRAM register ID/Offset. After it will again
// spin reading the RB_SIDEBAND_BUSY register until it is cleared, or complain otherwise.
// After the register is again cleared, it will read RB_SIDEBAND_DATA to get the actual data.
// Done reading.
// For Writes, it will set RB_SIDEBAND_WR_ADDR. After it will again spin reading the 
// RB_SIDEBAND_BUSY register until it is cleared, or complain otherwise.
// After the status is cleared, it will write to RB_SIDEBAND_DATA to set the data meant to be written to
// the specified register.
// Done writing.
// NOTE: For writes it will then wait again for the status register to clear, and perform a write to AZ0_LOOPBACK_LFSR,
// with the contents being set to 0x1A, unknown as of now why this is done, as the register is at the same offset of 
// RB_SIDEBAND_WR_ADDR, so presumably its the same register behaving differently, and again it will wait for
// a status clear and then issue a write to AZ0_LOOPBACK_LFSR, wich is aagain at the same offset than RB_SIDEBAND_DATA,
// with the contents that where meant to be written to the specified register. This could possibly be done for redundancy checks
// although that's just speculation at this point.

// EDRAM State, tracks all internal registers such as EDRAM Version and Revision.
struct EDRAMState {
  // Status of the EDRAM.
  bool edramBusy = false;
  // Current register to be read from.
  u32 readRegisterIndex = 0;
  // Current register to be written to.
  u32 writeRegisterIndex = 0;
  // Data to be read from last read command.
  u32 readData = 0;
  // Register set.
  std::vector<u32> edramRegs = {};
  // CRC's registers indexes.
  u32 az0BCRegIndex = 0;
  u32 az1BCRegIndex = 0;
  u32 reg41Index = 0;
  u32 reg1041Index = 0;
  // CRC AZ0 Data.
  std::vector<u32> az0Data = {};
  // CRC AZ1 Data.
  std::vector<u32> az1Data = {};
  // CRC Reg @ 0x41 Data.
  std::vector<u32> reg41Data = {};
  // CRC Reg @ 0x1041 Data.
  std::vector<u32> reg1041Data = {};
};

// Specifies wich register index to modify, either read index or write index.
enum eRegIndexType {
  readIndex,
  writeIndex
};

class EDRAM {
public:
  EDRAM();
  ~EDRAM();
  // Basic Registers R/W functions.
  void SetRWRegIndex(eRegIndexType indexType, u32 index);
  u32 ReadReg();
  void WriteReg(u32 data);
  // CRC regs.
  u32 ReadCRC_AZ0_BC();
  u32 ReadCRC_AZ1_BC();

  // Returns true if the edram is currently busy with work.
  bool isEdramBusy() { return edramState.get()->edramBusy; };
private:
  std::unique_ptr<EDRAMState> edramState = {};
};

} // namespace Xe::XGPU
