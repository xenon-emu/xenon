/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

/*
* Most of the information in this file was gathered from
* various sources across the internet.
*
* In particular, this work is based heavily on research by
* the Xenia developers - especially Triang3l -
* whose dedication and deep understanding of the hardware
* made accurate Xenos emulation possible.
*
* Huge thanks to everyone who contributed to uncovering and
* documenting this complex system.
*/

#pragma once

#include <string>
#include <unordered_map>

#include "Base/Types.h"
#include <Base/StringUtil.h>

namespace Xe::XGPU {
  
// Masks for RingBuffer registers.
#define CP_RB_CNTL_RB_BUFSZ_MASK  0x0000003FL

// The Command Processor has 4 types of packets.
// The type of packet can be extracted from the packet data, as it's the upper 2 bits.
// So Packet type = CPPacket >> 30.
// More info on: https://github.com/freedreno/amd-gpu/blob/master/include/api/gsl_pm4types.h
enum CPPacketType : u8 {
  // Packet type 0: Writes x amount of registers in sequence starting at
  // the specified base index.
  // Register Count = ((CPPacket >> 16) & 0x3FFF) + 1.
  // Base Index = CPPacket & 0x7FFF.
  CPPacketType0,
  // Packet type 1: Writes only two registers. Uncommon.
  // Register 0 Index = CPPacket & 0x7FF.
  // Register 1 Index = (CPPacket >> 11) & 0x7FF.
  CPPacketType1,
  // Packet type 2: Basically is a No-Op packet.
  CPPacketType2,
  // Packet type 3: Executes PM4 commands.
  CPPacketType3
};

// Opcodes for CP Packet Type 3 based on:
// 1 -> https://github.com/freedreno/amd-gpu/blob/master/include/api/gsl_pm4types.h
// 2 -> https://github.com/freedreno/freedreno/blob/master/includes/adreno_pm4.xml.h

enum CPPacketType3Opcode : u8 {
  PM4_NOP                       = 0x10, // Skip N 32-bit words to get to the next packet.
  PM4_RECORD_PFP_TIMESTAMP      = 0x11,
  PM4_WAIT_MEM_WRITES           = 0x12,
  PM4_WAIT_FOR_ME               = 0x13,
  PM4_UNKNOWN_19                = 0x19,
  PM4_UNKNOWN_1A                = 0x1A,
  PM4_PREEMPT_ENABLE            = 0x1C,
  PM4_SKIP_IB2_ENABLE_GLOBAL    = 0x1D,
  PM4_PREEMPT_TOKEN             = 0x1E,
  PM4_REG_RMW                   = 0x21, // Perform Register Read/Modify/Write.
  PM4_DRAW_INDX                 = 0x22, // Initiate fetch of index buffer and draw it.
  PM4_VIZ_QUERY                 = 0x23, // Begin/end initiator for VIZ query extent processing.
  PM4_DRAW_AUTO                 = 0x24,
  PM4_SET_STATE                 = 0x25, // Fetch state sub-blocks and initiate shader code DMA's.
  PM4_WAIT_FOR_IDLE             = 0x26, // Wait for the IDLE state of the engine.
  PM4_IM_LOAD                   = 0x27, // Load sequencer instruction memory (pointer-based).
  PM4_DRAW_INDIRECT             = 0x28,
  PM4_DRAW_INDX_INDIRECT        = 0x29,
  PM4_IM_LOAD_IMMEDIATE         = 0x2B, // Load sequencer instruction memory (code embedded in packet).
  PM4_IM_STORE                  = 0x2C, // Copy sequencer instruction memory to system memory.
  PM4_SET_CONSTANT              = 0x2D, // Load constant into chip and to memory.
  PM4_LOAD_CONSTANT_CONTEXT     = 0x2E, // Load constants from a location in memory.
  PM4_LOAD_ALU_CONSTANT         = 0x2F, // Load constants from memory.
  PM4_LOAD_STATE                = 0x30,
  PM4_RUN_OPENCL                = 0x31,
  PM4_COND_INDIRECT_BUFFER_PFD  = 0x32,
  PM4_EXEC_CS                   = 0x33,
  PM4_DRAW_INDX_BIN             = 0x34, // Initiate fetch of index buffer and binIDs and draw.
  PM4_DRAW_INDX_2_BIN           = 0x35, // Initiate fetch of bin IDs and draw using supplied indices.
  PM4_DRAW_INDX_2               = 0x36, // Draw using supplied indices in packet.
  PM4_INDIRECT_BUFFER_PFD       = 0x37, // Indirect buffer dispatch. Same as IB, but init is pipelined.
  PM4_DRAW_INDX_OFFSET          = 0x38,
  PM4_UNK_39                    = 0x39,
  PM4_COND_INDIRECT_BUFFER_PFE  = 0x3A,
  PM4_INVALIDATE_STATE          = 0x3B, // Selective invalidation of state pointers.
  PM4_WAIT_REG_MEM              = 0x3C, // Wait until a register or memory location is a specific value.
  PM4_MEM_WRITE                 = 0x3D, // Write N 32-bit words to memory.
  PM4_REG_TO_MEM                = 0x3E, // Reads register in chip and writes to memory.
  PM4_INDIRECT_BUFFER           = 0x3F, // Indirect buffer dispatch. Prefetch parser uses this packet type to determine whether to pre-fetch the IB.
  PM4_EXEC_CS_INDIRECT          = 0x41,
  PM4_MEM_TO_REG                = 0x42,
  PM4_SET_DRAW_STATE            = 0x43,
  PM4_COND_EXEC                 = 0x44, // Conditional execution of a sequence of packets.
  PM4_COND_WRITE                = 0x45, // Conditional write to memory or register.
  PM4_EVENT_WRITE               = 0x46, // Henerate an event that creates a write to memory when completed.
  PM4_COND_REG_EXEC             = 0x47,
  PM4_ME_INIT                   = 0x48, // Initialize CP's Micro-Engine.
  PM4_SET_SHADER_BASES          = 0x4A, // Dynamically changes shader instruction memory partition.
  PM4_SET_BIN_BASE_OFFSET       = 0x4B, // Program an offset that will added to the BIN_BASE value of the 3D_DRAW_INDX_BIN packet.
  PM4_SET_BIN                   = 0x4C,
  PM4_SCRATCH_TO_REG            = 0x4D,
  PM4_UNKNOWN_4E                = 0x4E,
  PM4_MEM_WRITE_CNTR            = 0x4F, // Write CP_PROG_COUNTER value to memory.
  PM4_SET_BIN_MASK              = 0x50, // Sets the 64-bit BIN_MASK register in the PFP.
  PM4_SET_BIN_SELECT            = 0x51, // Sets the 64-bit BIN_SELECT register in the PFP.
  PM4_WAIT_REG_EQ               = 0x52, // Wait until a register location is equal to a specific value.
  PM4_WAIT_REG_GTE              = 0x53, // Wait until a register location is >= a specific value.
  PM4_INTERRUPT                 = 0x54, // Generate interrupt from the command stream.
  PM4_SET_CONSTANT2             = 0x55, // INCR_UPDATE_STATE.
  PM4_SET_SHADER_CONSTANTS      = 0x56, // INCR_UPDT_CONST.
  PM4_EVENT_WRITE_SHD           = 0x58, // Generate a VS|PS_done event.
  PM4_EVENT_WRITE_CFL           = 0x59, // Generate a cache flush done event.
  PM4_EVENT_WRITE_EXT           = 0x5A, // Generate a screen extent event.
  PM4_EVENT_WRITE_ZPD           = 0x5B, // Generate a z_pass done event.
  PM4_WAIT_UNTIL_READ           = 0x5C, // Wait until a read completes.
  PM4_WAIT_IB_PFD_COMPLETE      = 0x5D, // Wait until all base/size writes from an IB_PFD packet have completed.
  PM4_CONTEXT_UPDATE            = 0x5E, // Updates the current context, if needed.
  PM4_SET_PROTECTED_MODE        = 0x5F,
  // Tiled rendering:
  // Display screen subsection rendering apparatus and method.
  // https://patents.google.com/patent/US20060055701
  PM4_SET_BIN_MASK_LO           = 0x60,
  PM4_SET_BIN_MASK_HI           = 0x61,
  PM4_SET_BIN_SELECT_LO         = 0x62,
  PM4_SET_BIN_SELECT_HI         = 0x63,

  PM4_SET_VISIBILITY_OVERRIDE   = 0x64,
  PM4_SET_SECURE_MODE           = 0x66,
  PM4_PREEMPT_ENABLE_GLOBAL     = 0x69,
  PM4_PREEMPT_ENABLE_LOCAL      = 0x6A,
  PM4_CONTEXT_SWITCH_YIELD      = 0x6B,
  PM4_SET_RENDER_MODE           = 0x6C,
  PM4_COMPUTE_CHECKPOINT        = 0x6E,
  PM4_TEST_TWO_MEMS             = 0x71,
  PM4_MEM_TO_MEM                = 0x73,
  PM4_WIDE_REG_WRITE            = 0x74,
  PM4_REG_WR_NO_CTXT            = 0x78
};

// Microcode Type
enum eCPMicrocodeType : u8 {
  uCodeTypeME,
  uCodeTypePFP
};

struct OpcodePair {
  u32 opcode;
  std::string_view name;
};
constexpr OpcodePair opcodeNames[] = {
  { 0x10, "PM4_NOP" },
  { 0x11, "PM4_RECORD_PFP_TIMESTAMP" },
  { 0x12, "PM4_WAIT_MEM_WRITES" },
  { 0x13, "PM4_WAIT_FOR_ME" },
  { 0x19, "PM4_UNKNOWN_19" },
  { 0x1A, "PM4_UNKNOWN_1A" },
  { 0x1C, "PM4_PREEMPT_ENABLE" },
  { 0x1D, "PM4_SKIP_IB2_ENABLE_GLOBAL" },
  { 0x1E, "PM4_PREEMPT_TOKEN" },
  { 0x21, "PM4_REG_RMW" },
  { 0x22, "PM4_DRAW_INDX" },
  { 0x23, "PM4_VIZ_QUERY" },
  { 0x24, "PM4_DRAW_AUTO" },
  { 0x25, "PM4_SET_STATE" },
  { 0x26, "PM4_WAIT_FOR_IDLE" },
  { 0x27, "PM4_IM_LOAD" },
  { 0x28, "PM4_DRAW_INDIRECT" },
  { 0x29, "PM4_DRAW_INDX_INDIRECT" },
  { 0x2B, "PM4_IM_LOAD_IMMEDIATE" },
  { 0x2C, "PM4_IM_STORE" },
  { 0x2D, "PM4_SET_CONSTANT" },
  { 0x2E, "PM4_LOAD_CONSTANT_CONTEXT" },
  { 0x2F, "PM4_LOAD_ALU_CONSTANT" },
  { 0x30, "PM4_LOAD_STATE" },
  { 0x31, "PM4_RUN_OPENCL" },
  { 0x32, "PM4_COND_INDIRECT_BUFFER_PFD" },
  { 0x33, "PM4_EXEC_CS" },
  { 0x34, "PM4_DRAW_INDX_BIN" },
  { 0x35, "PM4_DRAW_INDX_2_BIN" },
  { 0x36, "PM4_DRAW_INDX_2" },
  { 0x37, "PM4_INDIRECT_BUFFER_PFD" },
  { 0x38, "PM4_DRAW_INDX_OFFSET" },
  { 0x39, "PM4_UNK_39" },
  { 0x3A, "PM4_COND_INDIRECT_BUFFER_PFE" },
  { 0x3B, "PM4_INVALIDATE_STATE" },
  { 0x3C, "PM4_WAIT_REG_MEM" },
  { 0x3D, "PM4_MEM_WRITE" },
  { 0x3E, "PM4_REG_TO_MEM" },
  { 0x3F, "PM4_INDIRECT_BUFFER" },
  { 0x41, "PM4_EXEC_CS_INDIRECT" },
  { 0x42, "PM4_MEM_TO_REG" },
  { 0x43, "PM4_SET_DRAW_STATE" },
  { 0x44, "PM4_COND_EXEC" },
  { 0x45, "PM4_COND_WRITE" },
  { 0x46, "PM4_EVENT_WRITE" },
  { 0x47, "PM4_COND_REG_EXEC" },
  { 0x48, "PM4_ME_INIT" },
  { 0x4A, "PM4_SET_SHADER_BASES" },
  { 0x4B, "PM4_SET_BIN_BASE_OFFSET" },
  { 0x4C, "PM4_SET_BIN" },
  { 0x4D, "PM4_SCRATCH_TO_REG" },
  { 0x4E, "PM4_UNKNOWN_4E" },
  { 0x4F, "PM4_MEM_WRITE_CNTR" },
  { 0x50, "PM4_SET_BIN_MASK" },
  { 0x51, "PM4_SET_BIN_SELECT" },
  { 0x52, "PM4_WAIT_REG_EQ" },
  { 0x53, "PM4_WAIT_REG_GTE" },
  { 0x54, "PM4_INTERRUPT" },
  { 0x55, "PM4_SET_CONSTANT2" },
  { 0x56, "PM4_SET_SHADER_CONSTANTS" },
  { 0x58, "PM4_EVENT_WRITE_SHD" },
  { 0x59, "PM4_EVENT_WRITE_CFL" },
  { 0x5A, "PM4_EVENT_WRITE_EXT" },
  { 0x5B, "PM4_EVENT_WRITE_ZPD" },
  { 0x5C, "PM4_WAIT_UNTIL_READ" },
  { 0x5D, "PM4_WAIT_IB_PFD_COMPLETE" },
  { 0x5E, "PM4_CONTEXT_UPDATE" },
  { 0x5F, "PM4_SET_PROTECTED_MODE" },
  { 0x60, "PM4_SET_BIN_MASK_LO" },
  { 0x61, "PM4_SET_BIN_MASK_HI" },
  { 0x62, "PM4_SET_BIN_SELECT_LO" },
  { 0x63, "PM4_SET_BIN_SELECT_HI" },
  { 0x64, "PM4_SET_VISIBILITY_OVERRIDE" },
  { 0x66, "PM4_SET_SECURE_MODE" },
  { 0x69, "PM4_PREEMPT_ENABLE_GLOBAL" },
  { 0x6A, "PM4_PREEMPT_ENABLE_LOCAL" },
  { 0x6B, "PM4_CONTEXT_SWITCH_YIELD" },
  { 0x6C, "PM4_SET_RENDER_MODE" },
  { 0x6E, "PM4_COMPUTE_CHECKPOINT" },
  { 0x71, "PM4_TEST_TWO_MEMS" },
  { 0x73, "PM4_MEM_TO_MEM" },
  { 0x74, "PM4_WIDE_REG_WRITE" },
  { 0x78, "PM4_REG_WR_NO_CTXT" }
};

inline std::string GetPM4Opcode(u8 opcode) {
  for (const auto &reg : opcodeNames) {
    if (reg.opcode == opcode) {
      return std::string(reg.name);
    }
  }
  return std::format("UNK_OP_0x{:X}", opcode);
}

} // namespace Xe::XGPU
