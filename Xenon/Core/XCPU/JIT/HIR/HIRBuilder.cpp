/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project   *
 ******************************************************************************
 * Copyright 2021 Ben Vanik. All rights reserved.     *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "Core/XCPU/JIT/HIR/HIRBuilder.h"

#include <cinttypes>
#include <cstdarg>
#include <cstring>

#include "Base/Assert.h"

#include "Core/XCPU/JIT/HIR/Block.h"
#include "Core/XCPU/JIT/HIR/Instr.h"
#include "Core/XCPU/JIT/HIR/Label.h"
#include "Core/XCPU/PPU/PowerPC.h"

namespace Xe::XCPU::HIR {

#define ASSERT_ADDRESS_TYPE(value) ASSERT((value->type) == INT32_TYPE || (value->type) == INT64_TYPE)
#define ASSERT_CALL_ADDRESS_TYPE(value) ASSERT((value->type) == INT32_TYPE || (value->type) == INT64_TYPE)
#define ASSERT_INTEGER_TYPE(value) ASSERT((value->type) == INT8_TYPE || (value->type) == INT16_TYPE || \
  (value->type) == INT32_TYPE || (value->type) == INT64_TYPE)
#define ASSERT_FLOAT_TYPE(value) ASSERT((value->type) == FLOAT32_TYPE || (value->type) == FLOAT64_TYPE)
#define ASSERT_NON_FLOAT_TYPE(value) ASSERT((value->type) != FLOAT32_TYPE && (value->type) != FLOAT64_TYPE)
#define ASSERT_NON_VECTOR_TYPE(value) assert_false((value->type) == VEC128_TYPE)
#define ASSERT_VECTOR_TYPE(value) ASSERT((value->type) == VEC128_TYPE)
#define ASSERT_FLOAT_OR_VECTOR_TYPE(value) ASSERT((value->type) == FLOAT32_TYPE || \
  (value->type) == FLOAT64_TYPE || (value->type) == VEC128_TYPE)
#define ASSERT_TYPES_EQUAL(value1, value2) ASSERT((value1->type) == (value2->type))

// Constructor
HIRBuilder::HIRBuilder() {
  arena_ = new Arena();
  Reset();
}

// Destructor
HIRBuilder::~HIRBuilder() {
  Reset();
  delete arena_;
}

// Reset builder
void HIRBuilder::Reset() {
  currentBlock = NULL;
  nextValueOrdinal = 0;
#if SCRIBBLE_ARENA_ON_RESET
  arena_->DebugFill();
#endif
  // Reset arena
  arena_->Reset();
}

// Returns a pointer to the current block
HIRBlock *HIRBuilder::getCurrentBlock() const { return currentBlock; }

// Appends a block onto code arena
HIRBlock *HIRBuilder::AppendBlock() {
  // Allocate new block in block arena
  HIRBlock *block = arena_->Alloc<HIRBlock>();
  block->ordinal = UINT16_MAX;
  block->incomingValues = nullptr;
  block->arena = arena_;
  currentBlock = block;
  block->instrHead = block->instrTail = NULL;
  return block;
}

//
// Main Operations
//

// Appends a new instruction into the current block, creates it of none exists.
Instr *HIRBuilder::AppendInstr(const OpcodeInfo &opcodeInfo, u16 flags, Value *dest) {
  // Create new bloc if none exists
  if (!currentBlock) {
    AppendBlock();
  }
  HIRBlock *block = currentBlock;

  Instr *instr = arena_->Alloc<Instr>();
  instr->next = NULL;
  instr->prev = block->instrTail;
  if (block->instrTail) {
    block->instrTail->next = instr;
  }
  block->instrTail = instr;
  if (!block->instrHead) {
    block->instrHead = instr;
  }
  instr->ordinal = UINT32_MAX;
  instr->block = block;
  instr->opcode = &opcodeInfo;
  instr->flags = flags;
  instr->dest = dest;
  instr->src1.value = instr->src2.value = instr->src3.value = NULL;
  instr->src1_use = instr->src2_use = instr->src3_use = NULL;
  if (dest) { dest->def = instr; }
  // Rely on callers to set src args.
  // This prevents redundant stores.
  return instr;
}

// Allocates a new value of the given type
Value *HIRBuilder::AllocValue(TypeName type) {
  Value *value = arena_->Alloc<Value>();
  value->ordinal = nextValueOrdinal++;
  value->type = type;
  value->flags = 0;
  value->def = NULL;
  value->use_head = NULL;
  value->last_use = NULL;
  value->local_slot = NULL;
  value->tag = NULL;
  value->reg.index = -1;
  return value;
}

// Allocates a new value equal to the source value
Value *HIRBuilder::CloneValue(Value *source) {
  Value *value = arena_->Alloc<Value>();
  value->ordinal = nextValueOrdinal++;
  value->type = source->type;
  value->flags = source->flags;
  value->constant.v128 = source->constant.v128;
  value->def = NULL;
  value->use_head = NULL;
  value->last_use = NULL;
  value->local_slot = NULL;
  value->tag = NULL;
  value->reg.index = -1;
  return value;
}

// Constants definitions

// Loads a zero filled constant Value from the given type
Value *HIRBuilder::LoadZero(TypeName type) {
  // TODO(benvanik): cache zeros per block/fn? Prevents tons of dupes.
  Value *dest = AllocValue();
  dest->set_zero(type);
  return dest;
}

// Loads constant values onto a new Value 
Value *HIRBuilder::LoadConstantInt8(s8 value) {
  Value *dest = AllocValue();
  dest->set_constant(value);
  return dest;
}
Value *HIRBuilder::LoadConstantUint8(u8 value) {
  Value *dest = AllocValue();
  dest->set_constant(value);
  return dest;
}
Value *HIRBuilder::LoadConstantInt16(s16 value) {
  Value *dest = AllocValue();
  dest->set_constant(value);
  return dest;
}
Value *HIRBuilder::LoadConstantUint16(u16 value) {
  Value *dest = AllocValue();
  dest->set_constant(value);
  return dest;
}
Value *HIRBuilder::LoadConstantInt32(s32 value) {
  Value *dest = AllocValue();
  dest->set_constant(value);
  return dest;
}
Value *HIRBuilder::LoadConstantUint32(u32 value) {
  Value *dest = AllocValue();
  dest->set_constant(value);
  return dest;
}
Value *HIRBuilder::LoadConstantInt64(s64 value) {
  Value *dest = AllocValue();
  dest->set_constant(value);
  return dest;
}
Value *HIRBuilder::LoadConstantUint64(u64 value) {
  Value *dest = AllocValue();
  dest->set_constant(value);
  return dest;
}
Value *HIRBuilder::LoadConstantFloat32(f32 value) {
  Value *dest = AllocValue();
  dest->set_constant(value);
  return dest;
}
Value *HIRBuilder::LoadConstantFloat64(f64 value) {
  Value *dest = AllocValue();
  dest->set_constant(value);
  return dest;
}
Value *HIRBuilder::LoadConstantVec128(const Base::Vector128 &value) {
  Value *dest = AllocValue();
  dest->set_constant(value);
  return dest;
}

//
// Instruction definitions
//

// System and register Operations

// Condition Register
void HIRBuilder::UpdateCR0(Value *value, bool isSigned, bool usesMSR) {
  UpdateCRx(0, value, isSigned, usesMSR);
}

void HIRBuilder::UpdateCRx(u8 crIndex, Value *value, bool isSigned, bool usesMSR) {
  // Arithmetic Rc=1 path: backend must check MSR[SF] to pick 32/64 compare.
  // Flag 1 signals this to the backend emitter.
  Value *zero = LoadZero(value->type);
  ASSERT_TYPES_EQUAL(value, zero);
  const OpcodeInfo &buildOp = isSigned ? OPCODE_BUILD_CR_SIGNED_info
   : OPCODE_BUILD_CR_UNSIGNED_info;
  Instr *i = AppendInstr(buildOp, usesMSR, AllocValue(INT8_TYPE));
  i->set_src1(value);
  i->set_src2(zero);
  i->src3.value = NULL;
  Value *crField = i->dest;

  Instr *j = AppendInstr(OPCODE_SET_CR_FIELD_info, 0);
  j->src1.offset = static_cast<u64>(crIndex);
  j->set_src2(crField);
  j->src3.value = NULL;
  return;
}

void HIRBuilder::UpdateCRx(u8 crIndex, Value *lhs, Value *rhs, bool isSigned, bool usesMSR) {
  ASSERT_TYPES_EQUAL(lhs, rhs);
  // Explicit compare path (cmp/cmpl/cmpi/cmpli): L bit already determined width.
  // Flag 0 tells the backend to compare at the input type's native size.
  const OpcodeInfo &buildOp = isSigned ? OPCODE_BUILD_CR_SIGNED_info
   : OPCODE_BUILD_CR_UNSIGNED_info;
  Instr *i = AppendInstr(buildOp, usesMSR, AllocValue(INT8_TYPE));
  i->set_src1(lhs);
  i->set_src2(rhs);
  i->src3.value = NULL;
  Value *crField = i->dest;

  // Set the CR field at the given index
  Instr *j = AppendInstr(OPCODE_SET_CR_FIELD_info, 0);
  j->src1.offset = static_cast<u64>(crIndex);
  j->set_src2(crField);
  j->src3.value = NULL;
}

void HIRBuilder::UpdateCR6(Value *value) {
  Value *allEqual = IsFalse(Not(value));
  Value *noneEqual = IsFalse(value);

  // Build 4-bit CR field value
  Value *allBit = Shl(allEqual, (s8)3);
  Value *noneBit = Shl(noneEqual, (s8)1);
  Value *cr6Val = Or(allBit, noneBit);

  // Set CR field 6
  Instr *j = AppendInstr(OPCODE_SET_CR_FIELD_info, 0);
  j->src1.offset = static_cast<u64>(6);
  j->set_src2(cr6Val);
  j->src3.value = NULL;
}

void HIRBuilder::UpdateFPSCR(Value *value, bool updateCR1) {
  // Classify the floating-point result and update FPSCR FPRF bits.
  // FPRF is a 5-bit field at FPSCR bits 15-19 (C, FL, FG, FE, FU).
  //
  // For arithmetic results, FPCC (the lower 4 bits of FPRF) is set as:
  //   FL (bit 3): result < 0  (negative)
  //   FG (bit 2): result > 0  (positive)
  //   FE (bit 1): result == 0 (zero)
  //   FU (bit 0): result is NaN (unordered)
  // And C (class bit) is set for denormals/infinities.
  //
  // We build FPCC using HIR comparisons against zero, then update the
  // FPSCR field in-place.

  Value *fpscr = LoadFPSCR();
  Value *zero = LoadZeroFloat64();

  // Check for NaN
  Value *isNan = IsNan(value);

  // Check comparisons (only valid when not NaN)
  Value *notNan = Xor(isNan, LoadConstantInt8(0x01));
  Value *isNeg = And(notNan, CompareSLT(value, zero));
  Value *isPos = And(notNan, CompareSGT(value, zero));
  Value *isZero = And(notNan, CompareEQ(value, zero));

  // Build FPCC: FL=8, FG=4, FE=2, FU=1
  Value *fpcc = LoadZeroInt8();
  fpcc = Or(fpcc, Shl(isNeg, (s8)3));   // FL at bit 3
  fpcc = Or(fpcc, Shl(isPos, (s8)2));   // FG at bit 2
  fpcc = Or(fpcc, Shl(isZero, (s8)1));  // FE at bit 1
  fpcc = Or(fpcc, isNan);    // FU at bit 0

  // FPRF is at bits 15-19 in FPSCR (5-bit field).
  // For now we only update FPCC (bits 16-19, the lower 4 bits of FPRF).
  // C bit (bit 15) is left as 0 for basic classification.
  // FPRF mask: bits 15-19 = 0x0001F000
  // FPCC mask: bits 16-19 = 0x000F0000 ... 
  // Actually in the PowerPC FPSCR layout used here (PPCBitfield<u32, 15, 5> FPRF):
  // FPRF occupies bits [15..19] counting from MSB, which in a 32-bit LE value
  // is at bit positions (31-15)=16 down to (31-19)=12.
  // So FPCC (4 bits) is at bits 12-15 and C is at bit 16.
  constexpr u32 FPCC_MASK = 0xFu << 12;   // bits 12-15
  constexpr u32 FPCC_CLEAR = ~FPCC_MASK;

  // Clear existing FPCC bits
  Value *clearedFPSCR = And(fpscr, LoadConstantUint32(FPCC_CLEAR));

  // Shift FPCC into position and merge
  Value *fpcc32 = ZeroExtend(fpcc, INT32_TYPE);
  Value *shiftedFPCC = Shl(fpcc32, LoadConstantInt8(12));
  Value *newFPSCR = Or(clearedFPSCR, shiftedFPCC);
  StoreFPSCR(newFPSCR);

  // Optionally update CR1 from FPSCR
  if (updateCR1) {
    UpdateCR1();
  }
}

void HIRBuilder::UpdateCR1() {
  // CR1 <- FPSCR[FX] || FPSCR[FEX] || FPSCR[VX] || FPSCR[OX]
  // FX  = bit 0 (BE) = bit 31 (LE value)
  // FEX = bit 1 (BE) = bit 30
  // VX  = bit 2 (BE) = bit 29
  // OX  = bit 3 (BE) = bit 28
  Value *fpscr = LoadFPSCR();

  // Extract FX (bit 31), FEX (bit 30), VX (bit 29), OX (bit 28)
  Value *fx  = And(Shr(fpscr, LoadConstantInt8(31)), LoadConstantUint32(1));
  Value *fex = And(Shr(fpscr, LoadConstantInt8(30)), LoadConstantUint32(1));
  Value *vx  = And(Shr(fpscr, LoadConstantInt8(29)), LoadConstantUint32(1));
  Value *ox  = And(Shr(fpscr, LoadConstantInt8(28)), LoadConstantUint32(1));

  // Build CR1 value: FX at bit 3, FEX at bit 2, VX at bit 1, OX at bit 0
  Value *cr1 = Or(Shl(fx, LoadConstantInt8(3)),
   Or(Shl(fex, LoadConstantInt8(2)),
   Or(Shl(vx, LoadConstantInt8(1)), ox)));
  Value *cr1_8 = Truncate(cr1, INT8_TYPE);

  // Set CR field 1
  Instr *j = AppendInstr(OPCODE_SET_CR_FIELD_info, 0);
  j->src1.offset = static_cast<u64>(1);
  j->set_src2(cr1_8);
  j->src3.value = NULL;
}

void HIRBuilder::StoreSAT(Value *satFlag) {
  // satFlag is INT8 (0 or 1) from DidSaturate().
  // VSCR.SAT is bit 0 of VSCR.hexValue (u32). It's a sticky bit (OR).
  Value *vscr = LoadContext(offsetof(sPPUThread, VSCR), INT32_TYPE);
  Value *satExt = ZeroExtend(satFlag, INT32_TYPE);
  StoreContext(offsetof(sPPUThread, VSCR), Or(vscr, satExt));
}

// General Purpose Registers
Value *HIRBuilder::LoadGPR(u32 reg) {
  return LoadContext(offsetof(sPPUThread, GPR) + sizeof(sPPUThread::GPR[0]) * reg,
                     INT64_TYPE);
}
void HIRBuilder::StoreGPR(u32 reg, Value *value) {
  StoreContext(offsetof(sPPUThread, GPR) + sizeof(sPPUThread::GPR[0]) * reg,
               value);
}

// Floating-Point Registers
Value *HIRBuilder::LoadFPR(u32 reg) {
  return LoadContext(offsetof(sPPUThread, FPR) + sizeof(sPPUThread::FPR[0]) * reg, FLOAT64_TYPE);
}
void HIRBuilder::StoreFPR(u32 reg, Value *value) {
  if (value->type != FLOAT64_TYPE) {
    UNREACHABLE();
  }
  StoreContext(offsetof(sPPUThread, FPR) + sizeof(sPPUThread::FPR[0]) * reg, value);
}

// Vector Registers
Value *HIRBuilder::LoadVR(u32 reg) {
  return LoadContext(offsetof(sPPUThread, VR) + sizeof(sPPUThread::VR[0]) * reg, VEC128_TYPE);
}
void HIRBuilder::StoreVR(u32 reg, Value *value) {
  StoreContext(offsetof(sPPUThread, VR) + sizeof(sPPUThread::VR[0]) * reg, value);
}

// Link Register
Value *HIRBuilder::LoadLR() {
  return LoadContext(offsetof(sPPUThread, SPR.LR), INT64_TYPE);
}
void HIRBuilder::StoreLR(Value *value) {
  StoreContext(offsetof(sPPUThread, SPR.LR), value);
}

// Count Register
Value *HIRBuilder::LoadCTR() {
  return LoadContext(offsetof(sPPUThread, SPR.CTR), INT64_TYPE);
}
void HIRBuilder::StoreCTR(Value *value) {
  StoreContext(offsetof(sPPUThread, SPR.CTR), value);
}

// Fixed-Point Exception Register
Value *HIRBuilder::LoadXER() {
  return LoadContext(offsetof(sPPUThread, SPR.XER), INT32_TYPE);
}
void HIRBuilder::StoreXER(Value *value) {
  StoreContext(offsetof(sPPUThread, SPR.XER), value);
}

// XER Carry bit
Value *HIRBuilder::LoadCA() {
  Value *xer = LoadXER();
  Value *shifted = Shr(xer, LoadConstantInt8(29));
  Value *ca = And(shifted, LoadConstantUint32(1));
  return Truncate(ca, INT8_TYPE);
}
void HIRBuilder::StoreCA(Value *value) {
  Value *xer = LoadXER();
  // Clear CA bit (bit 29)
  Value *cleared = And(xer, LoadConstantUint32(~(1u << 29)));
  // Set new CA value — value is INT8 (0 or 1), widen to INT32 for shift/or
  Value *caBit = ZeroExtend(value, INT32_TYPE);
  caBit = And(caBit, LoadConstantUint32(1));
  Value *shifted = Shl(caBit, LoadConstantInt8(29));
  Value *newXer = Or(cleared, shifted);
  StoreXER(newXer);
}

// XER Summary Overflow
Value *HIRBuilder::LoadSO() {
  Value *xer = LoadXER();
  Value *shifted = Shr(xer, LoadConstantInt8(31));
  return And(shifted, LoadConstantUint32(1));
}

// XER Overflow
Value *HIRBuilder::LoadOV() {
  Value *xer = LoadXER();
  Value *shifted = Shr(xer, LoadConstantInt8(30));
  return And(shifted, LoadConstantUint32(1));
}

// MSR
Value *HIRBuilder::LoadMSR() {
  return LoadContext(offsetof(sPPUThread, SPR.MSR), INT64_TYPE);
}
void HIRBuilder::StoreMSR(Value *value) {
  StoreContext(offsetof(sPPUThread, SPR.MSR), value);
}

// FPSCR
Value *HIRBuilder::LoadFPSCR() {
  return LoadContext(offsetof(sPPUThread, FPSCR), INT32_TYPE);
}
void HIRBuilder::StoreFPSCR(Value *value) {
  StoreContext(offsetof(sPPUThread, FPSCR), value);
}

// Condition Register
Value *HIRBuilder::LoadCR() {
  return LoadContext(offsetof(sPPUThread, CR), INT32_TYPE);
}
Value *HIRBuilder::LoadCR(u8 crField) {
  // CR layout: CR0 is bits 28-31 (MSB), CR7 is bits 0-3 (LSB).
  // Extract: (CR_Hex >> ((7 - crField) * 4)) & 0xF
  Value *cr = LoadCR();
  u8 sh = static_cast<u8>((7 - crField) * 4);
  if (sh)
    cr = Shr(cr, LoadConstantInt8(static_cast<s8>(sh)));

  cr = And(cr, LoadConstantUint32(0xF));
  return Truncate(cr, INT8_TYPE);
}

void HIRBuilder::StoreCR(Value *value) {
  StoreContext(offsetof(sPPUThread, CR), value);
}

// Branch Control

void HIRBuilder::Branch(uPPCInstr currentInstr) {
  Instr *i = AppendInstr(OPCODE_BRANCH_info, 0);
  i->src1.value = i->src2.value = i->src3.value = NULL;
  i->currentInstrData = currentInstr;
}

void HIRBuilder::BranchConditional(uPPCInstr currentInstr) {
  Instr *i = AppendInstr(OPCODE_BRANCH_CONDITIONAL_info, 0);
  i->src1.value = i->src2.value = i->src3.value = NULL;
  i->currentInstrData = currentInstr;
}

void HIRBuilder::BranchConditionalToCTR(uPPCInstr currentInstr) {
  Instr *i = AppendInstr(OPCODE_BRANCH_CONDITIONAL_CTR_info, 0);
  i->src1.value = i->src2.value = i->src3.value = NULL;
  i->currentInstrData = currentInstr;
}

void HIRBuilder::BranchConditionalToLR(uPPCInstr currentInstr) {
  Instr *i = AppendInstr(OPCODE_BRANCH_CONDITIONAL_LR_info, 0);
  i->src1.value = i->src2.value = i->src3.value = NULL;
  i->currentInstrData = currentInstr;
}

void HIRBuilder::Comment(const std::string_view &value) {
  if (value.empty()) {
    return;
  }
  auto size = value.size();
  auto p = reinterpret_cast<char *>(arena_->Alloc(size + 1, 1));
  std::memcpy(p, value.data(), size);
  p[size] = '\0';
  Instr *i = AppendInstr(OPCODE_COMMENT_info, 0);
  i->src1.offset = (u64)p;
  i->src2.value = i->src3.value = NULL;
}

/*void HIRBuilder::Comment(const Base::StringBuffer &value) {
  if (!value.length()) {
    return;
  }
  auto size = value.length();
  auto p = reinterpret_cast<char *>(arena_->Alloc(size + 1, 1));
  std::memcpy(p, value.buffer(), size);
  p[size] = '\0';
  Instr *i = AppendInstr(OPCODE_COMMENT_info, 0);
  i->src1.offset = (u64)p;
  i->src2.value = i->src3.value = NULL;
}*/

void HIRBuilder::CommentBuffer(const char *p) {
  Instr *i = AppendInstr(OPCODE_COMMENT_info, 0);
  i->src1.offset = (u64)p;
  i->src2.value = i->src3.value = NULL;
}

void HIRBuilder::Nop() {
  Instr *i = AppendInstr(OPCODE_NOP_info, 0);
  i->src1.value = i->src2.value = i->src3.value = NULL;
}

void HIRBuilder::SourceOffset(u64 addr, u32 rawWord, bool needsFullPrologue) {
  Instr *i = AppendInstr(OPCODE_SOURCE_OFFSET_info, needsFullPrologue ? 1 : 0);
  i->src1.offset = addr;
  i->src2.value = i->src3.value = NULL;
  i->currentInstrData.opcode = rawWord;
}

void HIRBuilder::DebugBreak() {
  Instr *i = AppendInstr(OPCODE_DEBUG_BREAK_info, 0);
  i->src1.value = i->src2.value = i->src3.value = NULL;

}

void HIRBuilder::DebugBreakTrue(Value *cond) {
  if (cond->IsConstant()) {
    if (cond->IsConstantTrue()) {
DebugBreak();
    }
    return;
  }

  Instr *i = AppendInstr(OPCODE_DEBUG_BREAK_TRUE_info, 0);
  i->set_src1(cond);
  i->src2.value = i->src3.value = NULL;
}

void HIRBuilder::StorageExceptionCheck() {
  Instr *i = AppendInstr(OPCODE_STORAGE_EXCEPTION_CHECK_info, 0);
  i->src1.value = i->src2.value = i->src3.value = NULL;
}

void HIRBuilder::SyncExceptionCheck() {
  Instr *i = AppendInstr(OPCODE_SYNC_EXCEPTION_CHECK_info, 0);
  i->src1.value = i->src2.value = i->src3.value = NULL;
}

void HIRBuilder::Trap(u16 trapCode) {
  Instr *i = AppendInstr(OPCODE_TRAP_info, trapCode);
  i->src1.value = i->src2.value = i->src3.value = NULL;
}

void HIRBuilder::TrapTrue(Value *cond, u16 trapCode) {
  if (cond->IsConstant()) {
    if (cond->IsConstantTrue()) {
Trap(trapCode);
    }
    return;
  }

  Instr *i = AppendInstr(OPCODE_TRAP_TRUE_info, trapCode);
  i->set_src1(cond);
  i->src2.value = i->src3.value = NULL;

}

void HIRBuilder::CallInterpreter(uPPCInstr instrData, void *interpFn) {
  Instr *i = AppendInstr(OPCODE_CALL_INTERPRETER_info, 0);
  i->src1.offset = reinterpret_cast<u64>(interpFn);
  i->src2.value = i->src3.value = NULL;
  i->currentInstrData = instrData;
}

Value *HIRBuilder::LoadTimeBase() {
  Instr *i = AppendInstr(OPCODE_LOAD_TIME_BASE_info, 0, AllocValue(INT64_TYPE));
  i->src1.value = i->src2.value = i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::EmitMFSPR(uPPCInstr instrData) {
  Instr *i = AppendInstr(OPCODE_MFSPR_info, 0, AllocValue(INT64_TYPE));
  i->src1.value = i->src2.value = i->src3.value = NULL;
  i->currentInstrData = instrData;
  return i->dest;
}

void HIRBuilder::EmitMTSPR(Value *sprValue, uPPCInstr instrData) {
  Instr *i = AppendInstr(OPCODE_MTSPR_info, 0);
  i->set_src1(sprValue);
  i->src2.value = i->src3.value = NULL;
  i->currentInstrData = instrData;
}

void HIRBuilder::EmitMTMSRD(Value *sprValue, uPPCInstr instrData) {
  Instr *i = AppendInstr(OPCODE_MTMSRD_info, 0);
  i->set_src1(sprValue);
  i->src2.value = i->src3.value = NULL;
  i->currentInstrData = instrData;
}

void HIRBuilder::EmitSyscall(uPPCInstr instrData) {
  Instr *i = AppendInstr(OPCODE_SYSCALL_info, 0);
  i->src1.value = i->src2.value = i->src3.value = NULL;
  i->currentInstrData = instrData;
}

void HIRBuilder::EmitRFID(uPPCInstr instrData) {
  Instr *i = AppendInstr(OPCODE_RFID_info, 0);
  i->src1.value = i->src2.value = i->src3.value = NULL;
  i->currentInstrData = instrData;
}

void HIRBuilder::CallHLEFunction(Core::HLE::HLEFunctionPtr fnPtr) {
  // Encode the host function pointer in src1.offset (u64).
  // The backend will emit a call to fnPtr(ppeState).
  Instr *i = AppendInstr(OPCODE_CALL_HLE_FUNCTION_info, 0);
  i->src1.offset = reinterpret_cast<u64>(fnPtr);
  i->src2.value = nullptr;
  i->src3.value = nullptr;
}

Value *HIRBuilder::Assign(Value *value) {
  if (value->IsConstant()) {
    return value;
  }

  Instr *i = AppendInstr(OPCODE_ASSIGN_info, 0, AllocValue(value->type));
  i->set_src1(value);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::Cast(Value *value, TypeName targetType) {
  if (value->type == targetType) {
    return value;
  }
  else if (value->IsConstant()) {
    Value *dest = CloneValue(value);
    dest->Cast(targetType);
    return dest;
  }

  Instr *i = AppendInstr(OPCODE_CAST_info, 0, AllocValue(targetType));
  i->set_src1(value);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::ZeroExtend(Value *value, TypeName targetType) {
  if (value->type == targetType) {
    return value;
  }
  else if (value->IsConstant()) {
    Value *dest = CloneValue(value);
    dest->ZeroExtend(targetType);
    return dest;
  }

  Instr *i = AppendInstr(OPCODE_ZERO_EXTEND_info, 0, AllocValue(targetType));
  i->set_src1(value);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::SignExtend(Value *value, TypeName targetType) {
  if (value->type == targetType) {
    return value;
  }
  else if (value->IsConstant()) {
    Value *dest = CloneValue(value);
    dest->SignExtend(targetType);
    return dest;
  }

  Instr *i = AppendInstr(OPCODE_SIGN_EXTEND_info, 0, AllocValue(targetType));
  i->set_src1(value);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::Truncate(Value *value, TypeName targetType) {
  ASSERT_INTEGER_TYPE(value);
  ASSERT(targetType == INT8_TYPE || targetType == INT16_TYPE ||
    targetType == INT32_TYPE || targetType == INT64_TYPE);

  if (value->type == targetType) {
    return value;
  }
  else if (value->IsConstant()) {
    Value *dest = CloneValue(value);
    dest->Truncate(targetType);
    return dest;
  }

  Instr *i = AppendInstr(OPCODE_TRUNCATE_info, 0, AllocValue(targetType));
  i->set_src1(value);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::Convert(Value *value, TypeName targetType, RoundMode roundMode) {
  if (value->type == targetType) {
    return value;
  }
  else if (value->IsConstant()) {
    Value *dest = CloneValue(value);
    dest->Convert(targetType, roundMode);
    return dest;
  }

  Instr *i =
    AppendInstr(OPCODE_CONVERT_info, roundMode, AllocValue(targetType));
  i->set_src1(value);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::Round(Value *value, RoundMode roundMode) {
  ASSERT_FLOAT_OR_VECTOR_TYPE(value);

  if (value->IsConstant()) {
    Value *dest = CloneValue(value);
    dest->Round(roundMode);
    return dest;
  }

  Instr *i =
    AppendInstr(OPCODE_ROUND_info, roundMode, AllocValue(value->type));
  i->set_src1(value);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

void HIRBuilder::SetRoundingMode(Value *value) {
  ASSERT_INTEGER_TYPE(value);
  Instr *i = AppendInstr(OPCODE_SET_ROUNDING_MODE_info, 0);
  i->set_src1(value);
}

Value *HIRBuilder::Max(Value *value1, Value *value2) {
  ASSERT_TYPES_EQUAL(value1, value2);

  if (value1->type != VEC128_TYPE && value1->IsConstant() &&
    value2->IsConstant()) {
    return value1->Compare(OPCODE_COMPARE_SLT, value2) ? value2 : value1;
  }

  Instr *i = AppendInstr(OPCODE_MAX_info, 0, AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::Min(Value *value1, Value *value2) {
  ASSERT_TYPES_EQUAL(value1, value2);

  if (value1->type != VEC128_TYPE && value1->IsConstant() &&
    value2->IsConstant()) {
    return value1->Compare(OPCODE_COMPARE_SLT, value2) ? value1 : value2;
  }

  Instr *i = AppendInstr(OPCODE_MIN_info, 0, AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::Select(Value *cond, Value *value1, Value *value2) {
  ASSERT(cond->type == INT8_TYPE || cond->type == VEC128_TYPE);  // for now
  ASSERT_TYPES_EQUAL(value1, value2);

  if (cond->IsConstant()) {
    return cond->IsConstantTrue() ? value1 : value2;
  }

  Instr *i = AppendInstr(OPCODE_SELECT_info, 0, AllocValue(value1->type));
  i->set_src1(cond);
  i->set_src2(value1);
  i->set_src3(value2);
  return i->dest;
}

Value *HIRBuilder::IsTrue(Value *value) {
  if (value->IsConstant()) {
    return LoadConstantInt8(value->IsConstantTrue() ? 1 : 0);
  }

  Instr *i = AppendInstr(OPCODE_IS_TRUE_info, 0, AllocValue(INT8_TYPE));
  i->set_src1(value);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::IsFalse(Value *value) {
  if (value->IsConstant()) {
    return LoadConstantInt8(value->IsConstantFalse() ? 1 : 0);
  }

  Instr *i = AppendInstr(OPCODE_IS_FALSE_info, 0, AllocValue(INT8_TYPE));
  i->set_src1(value);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::IsNan(Value *value) {
  Instr *i = AppendInstr(OPCODE_IS_NAN_info, 0, AllocValue(INT8_TYPE));
  i->set_src1(value);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::CompareXX(const OpcodeInfo &opcode, Value *value1, Value *value2) {
  ASSERT_TYPES_EQUAL(value1, value2);
  if (value1->IsConstant() && value2->IsConstant()) {
    return LoadConstantInt8(value1->Compare(opcode.num, value2) ? 1 : 0);
  }

  Instr *i = AppendInstr(opcode, 0, AllocValue(INT8_TYPE));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::CompareEQ(Value *value1, Value *value2) {
  return CompareXX(OPCODE_COMPARE_EQ_info, value1, value2);
}

Value *HIRBuilder::CompareNE(Value *value1, Value *value2) {
  return CompareXX(OPCODE_COMPARE_NE_info, value1, value2);
}

Value *HIRBuilder::CompareSLT(Value *value1, Value *value2) {
  return CompareXX(OPCODE_COMPARE_SLT_info, value1, value2);
}

Value *HIRBuilder::CompareSLE(Value *value1, Value *value2) {
  return CompareXX(OPCODE_COMPARE_SLE_info, value1, value2);
}

Value *HIRBuilder::CompareSGT(Value *value1, Value *value2) {
  return CompareXX(OPCODE_COMPARE_SGT_info, value1, value2);
}

Value *HIRBuilder::CompareSGE(Value *value1, Value *value2) {
  return CompareXX(OPCODE_COMPARE_SGE_info, value1, value2);
}

Value *HIRBuilder::CompareULT(Value *value1, Value *value2) {
  return CompareXX(OPCODE_COMPARE_ULT_info, value1, value2);
}

Value *HIRBuilder::CompareULE(Value *value1, Value *value2) {
  return CompareXX(OPCODE_COMPARE_ULE_info, value1, value2);
}

Value *HIRBuilder::CompareUGT(Value *value1, Value *value2) {
  return CompareXX(OPCODE_COMPARE_UGT_info, value1, value2);
}

Value *HIRBuilder::CompareUGE(Value *value1, Value *value2) {
  return CompareXX(OPCODE_COMPARE_UGE_info, value1, value2);
}

Value *HIRBuilder::DidSaturate(Value *value) {
  Instr *i = AppendInstr(OPCODE_DID_SATURATE_info, 0, AllocValue(INT8_TYPE));
  i->set_src1(value);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::Add(Value *value1, Value *value2, u32 arithmeticFlags) {
  ASSERT_TYPES_EQUAL(value1, value2);

  // TODO(benvanik): optimize when flags set.
  if (!arithmeticFlags) {
    if (value1->IsConstantZero()) {
return value2;
    }
    else if (value2->IsConstantZero()) {
return value1;
    }
    else if (value1->IsConstant() && value2->IsConstant()) {
Value *dest = CloneValue(value1);
dest->Add(value2);
return dest;
    }
  }

  Instr *i = AppendInstr(OPCODE_ADD_info, arithmeticFlags, AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::AddWithCarry(Value *value1, Value *value2, Value *value3, u32 arithmeticFlags) {
  ASSERT_TYPES_EQUAL(value1, value2);
  ASSERT(value3->type == INT8_TYPE);

  Instr *i = AppendInstr(OPCODE_ADD_CARRY_info, arithmeticFlags,
    AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->set_src3(value3);
  return i->dest;
}

Value *HIRBuilder::Sub(Value *value1, Value *value2, u32 arithmeticFlags) {
  ASSERT_TYPES_EQUAL(value1, value2);

  if (!arithmeticFlags) {
    // x - 0 = x
    if (value2->IsConstantZero()) {
return value1;
    }
    // x - x = 0
    if (value1 == value2) {
return LoadZero(value1->type);
    }
    if (value1->IsConstant() && value2->IsConstant()) {
Value *dest = CloneValue(value1);
dest->Sub(value2);
return dest;
    }
  }

  Instr *i =
    AppendInstr(OPCODE_SUB_info, arithmeticFlags, AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::Mul(Value *value1, Value *value2, u32 arithmeticFlags) {
  ASSERT_TYPES_EQUAL(value1, value2);

  Instr *i =
    AppendInstr(OPCODE_MUL_info, arithmeticFlags, AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::MulHi(Value *value1, Value *value2, u32 arithmeticFlags) {
  ASSERT_TYPES_EQUAL(value1, value2);

  Instr *i = AppendInstr(OPCODE_MUL_HI_info, arithmeticFlags,
    AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::Div(Value *value1, Value *value2, u32 arithmeticFlags) {
  ASSERT_TYPES_EQUAL(value1, value2);

  Instr *i =
    AppendInstr(OPCODE_DIV_info, arithmeticFlags, AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::MulAdd(Value *value1, Value *value2, Value *value3) {
  ASSERT_TYPES_EQUAL(value1, value2);
  ASSERT_TYPES_EQUAL(value1, value3);

  bool c1 = value1->IsConstant();
  bool c2 = value2->IsConstant();
  if (c1 && c2) {
    Value *dest = CloneValue(value1);
    dest->Mul(value2);
    return Add(dest, value3);
  }

  Instr *i = AppendInstr(OPCODE_MUL_ADD_info, 0, AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->set_src3(value3);
  return i->dest;
}

Value *HIRBuilder::MulSub(Value *value1, Value *value2, Value *value3) {
  ASSERT_TYPES_EQUAL(value1, value2);
  ASSERT_TYPES_EQUAL(value1, value3);

  bool c1 = value1->IsConstant();
  bool c2 = value2->IsConstant();
  if (c1 && c2) {
    Value *dest = CloneValue(value1);
    dest->Mul(value2);
    return Sub(dest, value3);
  }

  Instr *i = AppendInstr(OPCODE_MUL_SUB_info, 0, AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->set_src3(value3);
  return i->dest;
}

Value *HIRBuilder::Neg(Value *value) {
  Instr *i = AppendInstr(OPCODE_NEG_info, 0, AllocValue(value->type));
  i->set_src1(value);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::Abs(Value *value) {
  ASSERT_FLOAT_OR_VECTOR_TYPE(value);

  Instr *i = AppendInstr(OPCODE_ABS_info, 0, AllocValue(value->type));
  i->set_src1(value);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::Sqrt(Value *value) {
  ASSERT_FLOAT_OR_VECTOR_TYPE(value);

  Instr *i = AppendInstr(OPCODE_SQRT_info, 0, AllocValue(value->type));
  i->set_src1(value);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::RSqrt(Value *value) {
  ASSERT_FLOAT_OR_VECTOR_TYPE(value);

  Instr *i = AppendInstr(OPCODE_RSQRT_info, 0, AllocValue(value->type));
  i->set_src1(value);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::Recip(Value *value) {
  ASSERT_FLOAT_OR_VECTOR_TYPE(value);

  Instr *i = AppendInstr(OPCODE_RECIP_info, 0, AllocValue(value->type));
  i->set_src1(value);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::Pow2(Value *value) {
  ASSERT_FLOAT_OR_VECTOR_TYPE(value);

  Instr *i = AppendInstr(OPCODE_POW2_info, 0, AllocValue(value->type));
  i->set_src1(value);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::Log2(Value *value) {
  ASSERT_FLOAT_OR_VECTOR_TYPE(value);

  Instr *i = AppendInstr(OPCODE_LOG2_info, 0, AllocValue(value->type));
  i->set_src1(value);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::DotProduct3(Value *value1, Value *value2) {
  ASSERT_VECTOR_TYPE(value1);
  ASSERT_VECTOR_TYPE(value2);
  ASSERT_TYPES_EQUAL(value1, value2);

  Instr *i =
    AppendInstr(OPCODE_DOT_PRODUCT_3_info, 0, AllocValue(FLOAT32_TYPE));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::DotProduct4(Value *value1, Value *value2) {
  ASSERT_VECTOR_TYPE(value1);
  ASSERT_VECTOR_TYPE(value2);
  ASSERT_TYPES_EQUAL(value1, value2);

  Instr *i =
    AppendInstr(OPCODE_DOT_PRODUCT_4_info, 0, AllocValue(FLOAT32_TYPE));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::And(Value *value1, Value *value2) {
  ASSERT_NON_FLOAT_TYPE(value1);
  ASSERT_NON_FLOAT_TYPE(value2);
  ASSERT_TYPES_EQUAL(value1, value2);

  if (value1 == value2) {
    return value1;
  }
  else if (value1->IsConstantZero()) {
    return value1;
  }
  else if (value2->IsConstantZero()) {
    return value2;
  }

  Instr *i = AppendInstr(OPCODE_AND_info, 0, AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::AndNot(Value *value1, Value *value2) {
  ASSERT_NON_FLOAT_TYPE(value1);
  ASSERT_NON_FLOAT_TYPE(value2);
  ASSERT_TYPES_EQUAL(value1, value2);

  if (value1 == value2) {
    return LoadZero(value1->type);
  }
  else if (value1->IsConstantZero()) {
    return value1;
  }
  else if (value2->IsConstantZero()) {
    return value1;
  }

  Instr *i = AppendInstr(OPCODE_AND_NOT_info, 0, AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::Or(Value *value1, Value *value2) {
  ASSERT_NON_FLOAT_TYPE(value1);
  ASSERT_NON_FLOAT_TYPE(value2);
  ASSERT_TYPES_EQUAL(value1, value2);

  if (value1 == value2) {
    return value1;
  }
  else if (value1->IsConstantZero()) {
    return value2;
  }
  else if (value2->IsConstantZero()) {
    return value1;
  }

  Instr *i = AppendInstr(OPCODE_OR_info, 0, AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::Xor(Value *value1, Value *value2) {
  ASSERT_NON_FLOAT_TYPE(value1);
  ASSERT_NON_FLOAT_TYPE(value2);
  ASSERT_TYPES_EQUAL(value1, value2);

  if (value1 == value2) {
    return LoadZero(value1->type);
  }

  Instr *i = AppendInstr(OPCODE_XOR_info, 0, AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::Not(Value *value) {
  ASSERT_NON_FLOAT_TYPE(value);

  if (value->IsConstant()) {
    Value *dest = CloneValue(value);
    dest->Not();
    return dest;
  }

  Instr *i = AppendInstr(OPCODE_NOT_info, 0, AllocValue(value->type));
  i->set_src1(value);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::Shl(Value *value1, Value *value2) {
  ASSERT_NON_FLOAT_TYPE(value1);
  ASSERT_INTEGER_TYPE(value2);

  // NOTE AND value2 with 0x3F for 64bit, 0x1F for 32bit, etc..

  if (value2->IsConstantZero()) {
    return value1;
  }
  if (value2->type != INT8_TYPE) {
    value2 = Truncate(value2, INT8_TYPE);
  }

  Instr *i = AppendInstr(OPCODE_SHL_info, 0, AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}
Value *HIRBuilder::Shl(Value *value1, s8 value2) {
  return Shl(value1, LoadConstantInt8(value2));
}

Value *HIRBuilder::Shr(Value *value1, Value *value2) {
  ASSERT_NON_FLOAT_TYPE(value1);
  ASSERT_INTEGER_TYPE(value2);

  if (value2->IsConstantZero()) {
    return value1;
  }
  if (value2->type != INT8_TYPE) {
    value2 = Truncate(value2, INT8_TYPE);
  }

  Instr *i = AppendInstr(OPCODE_SHR_info, 0, AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}
Value *HIRBuilder::Shr(Value *value1, s8 value2) {
  return Shr(value1, LoadConstantInt8(value2));
}

Value *HIRBuilder::Sha(Value *value1, Value *value2) {
  ASSERT_INTEGER_TYPE(value1);
  ASSERT_INTEGER_TYPE(value2);

  // Can't shift zero positions :)
  if (value2->IsConstantZero()) { return value1; }

  if (value2->type != INT8_TYPE) {
    value2 = Truncate(value2, INT8_TYPE);
  }

  Instr *i = AppendInstr(OPCODE_SHA_info, 0, AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}
Value *HIRBuilder::Sha(Value *value1, s8 value2) {
  return Sha(value1, LoadConstantInt8(value2));
}

Value *HIRBuilder::RotateLeft(Value *value1, Value *value2) {
  ASSERT_INTEGER_TYPE(value1);
  ASSERT_INTEGER_TYPE(value2);

  // Can't rotate zero positions :)
  if (value2->IsConstantZero()) { return value1; }

  if (value2->type != INT8_TYPE) {
    value2 = Truncate(value2, INT8_TYPE);
  }

  Instr *i = AppendInstr(OPCODE_ROTATE_LEFT_info, 0, AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::CountLeadingZeros(Value *value) {
  ASSERT_INTEGER_TYPE(value);

  if (value->IsConstantZero()) {
    static const u8 zeros[] = { 8, 16, 32, 64, };
    ASSERT(value->type <= INT64_TYPE);
    return LoadConstantUint8(zeros[value->type]);
  }

  Instr *i = AppendInstr(OPCODE_CNTLZ_info, 0, AllocValue(INT8_TYPE));
  i->set_src1(value);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::Insert(Value *value, Value *index, Value *part) {
  // TODO(benvanik): could do some of this as constants.

  Value *trunc_index = index->type != INT8_TYPE ? Truncate(index, INT8_TYPE) : index;

  Instr *i = AppendInstr(OPCODE_INSERT_info, 0, AllocValue(value->type));
  i->set_src1(value);
  i->set_src2(trunc_index);
  i->set_src3(part);
  return i->dest;
}

Value *HIRBuilder::Insert(Value *value, u64 index, Value *part) {
  return Insert(value, LoadConstantUint64(index), part);
}

Value *HIRBuilder::Extract(Value *value, Value *index, TypeName targetType) {
  // TODO(benvanik): could do some of this as constants.

  Value *trunc_index = index->type != INT8_TYPE ? Truncate(index, INT8_TYPE) : index;

  Instr *i = AppendInstr(OPCODE_EXTRACT_info, 0, AllocValue(targetType));
  i->set_src1(value);
  i->set_src2(trunc_index);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::Extract(Value *value, u8 index, TypeName targetType) {
  return Extract(value, LoadConstantUint8(index), targetType);
}

Value *HIRBuilder::Splat(Value *value, TypeName targetType) {
  // TODO(benvanik): could do some of this as constants.

  Instr *i = AppendInstr(OPCODE_SPLAT_info, 0, AllocValue(targetType));
  i->set_src1(value);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::Permute(Value *control, Value *value1, Value *value2, TypeName partType) {
  ASSERT_TYPES_EQUAL(value1, value2);
  ASSERT(partType >= INT8_TYPE && partType <= INT32_TYPE);

  // TODO(benvanik): could do some of this as constants.

  Instr *i = AppendInstr(OPCODE_PERMUTE_info, partType, AllocValue(value1->type));
  i->set_src1(control);
  i->set_src2(value1);
  i->set_src3(value2);
  return i->dest;
}

Value *HIRBuilder::Swizzle(Value *value, TypeName partType, u32 swizzleMask) {
  // For now.
  ASSERT(partType == INT32_TYPE || partType == FLOAT32_TYPE);

  if (swizzleMask == SWIZZLE_XYZW_TO_XYZW) {
    return Assign(value);
  }

  // TODO(benvanik): could do some of this as constants.

  Instr *i =
    AppendInstr(OPCODE_SWIZZLE_info, partType, AllocValue(value->type));
  i->set_src1(value);
  i->src2.offset = swizzleMask;
  i->src3.value = NULL;
  return i->dest;
}

// Memory Operations

Value *HIRBuilder::ByteSwap(Value *value) {
  // Bytes can't be byteswapped :)
  if (value->type == INT8_TYPE) { return value; }

  Instr *i = AppendInstr(OPCODE_BYTE_SWAP_info, 0, AllocValue(value->type));
  i->set_src1(value);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::AtomicExchange(Value *address, Value *newValue) {
  ASSERT_ADDRESS_TYPE(address);
  ASSERT_INTEGER_TYPE(newValue);
  Instr *i =
    AppendInstr(OPCODE_ATOMIC_EXCHANGE_info, 0, AllocValue(newValue->type));
  i->set_src1(address);
  i->set_src2(newValue);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::AtomicCompareExchange(Value *address, Value *oldValue, Value *newValue) {
  ASSERT_ADDRESS_TYPE(address);
  Instr *i = AppendInstr(OPCODE_ATOMIC_COMPARE_EXCHANGE_info, 0,
    AllocValue(INT8_TYPE));
  i->set_src1(address);
  i->set_src2(oldValue);
  i->set_src3(newValue);
  return i->dest;
}

Value *HIRBuilder::LoadContext(size_t offset, TypeName type) {
  Instr *i = AppendInstr(OPCODE_LOAD_CONTEXT_info, 0, AllocValue(type));
  i->src1.offset = offset;
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

void HIRBuilder::StoreContext(size_t offset, Value *value) {
  Instr *i = AppendInstr(OPCODE_STORE_CONTEXT_info, 0);
  i->src1.offset = offset;
  i->set_src2(value);
  i->src3.value = NULL;
}

void HIRBuilder::ContextBarrier() {
  AppendInstr(OPCODE_CONTEXT_BARRIER_info, 0);
}

Value *HIRBuilder::LoadOffset(Value *address, Value *offset, TypeName type, u32 loadFlags) {
  ASSERT_ADDRESS_TYPE(address);
  Instr *i = AppendInstr(OPCODE_LOAD_OFFSET_info, loadFlags, AllocValue(type));
  i->set_src1(address);
  i->set_src2(offset);
  i->src3.value = NULL;
  return i->dest;
}

void HIRBuilder::StoreOffset(Value *address, Value *offset, Value *value, u32 storeFlags) {
  ASSERT_ADDRESS_TYPE(address);
  Instr *i = AppendInstr(OPCODE_STORE_OFFSET_info, storeFlags);
  i->set_src1(address);
  i->set_src2(offset);
  i->set_src3(value);
}

Value *HIRBuilder::Load(Value *address, TypeName type, u32 loadFlags) {
  ASSERT_ADDRESS_TYPE(address);
  Instr *i = AppendInstr(OPCODE_LOAD_info, loadFlags, AllocValue(type));
  i->set_src1(address);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

void HIRBuilder::Store(Value *address, Value *value, u32 storeFlags) {
  ASSERT_ADDRESS_TYPE(address);
  Instr *i = AppendInstr(OPCODE_STORE_info, storeFlags);
  i->set_src1(address);
  i->set_src2(value);
  i->src3.value = NULL;
}

void HIRBuilder::Memset(Value *address, Value *value, Value *length) {
  ASSERT_ADDRESS_TYPE(address);
  ASSERT_TYPES_EQUAL(address, length);
  ASSERT(value->type == INT8_TYPE);
  Instr *i = AppendInstr(OPCODE_MEMSET_info, 0);
  i->set_src1(address);
  i->set_src2(value);
  i->set_src3(length);
}

void HIRBuilder::CacheControl(Value *address, size_t cacheLineSize, CacheControlType type) {
  ASSERT_ADDRESS_TYPE(address);
  Instr *i = AppendInstr(OPCODE_CACHE_CONTROL_info, u32(type));
  i->set_src1(address);
  i->src2.offset = cacheLineSize;
  i->src3.value = NULL;
}

void HIRBuilder::MemoryBarrier() { AppendInstr(OPCODE_MEMORY_BARRIER_info, 0); }

Value *HIRBuilder::LoadReserved() {
  return LoadContext(offsetof(sPPUThread, reservedValue), INT64_TYPE);
}

void HIRBuilder::StoreReserved(Value *value) {
  StoreContext(offsetof(sPPUThread, reservedValue), value);
}

// Vector128 Operations

Value *HIRBuilder::Pack(Value *value, u32 packFlags) {
  return Pack(value, LoadZeroVec128(), packFlags);
}

Value *HIRBuilder::Pack(Value *value1, Value *value2, u32 packFlags) {
  ASSERT_VECTOR_TYPE(value1);
  ASSERT_VECTOR_TYPE(value2);
  switch (packFlags & PACK_TYPE_MODE) {
  case PACK_TYPE_D3DCOLOR:
  case PACK_TYPE_FLOAT16_2:
  case PACK_TYPE_FLOAT16_4:
  case PACK_TYPE_SHORT_2:
    ASSERT(value2->IsConstantZero());
    break;
  }
  Instr *i = AppendInstr(OPCODE_PACK_info, packFlags, AllocValue(VEC128_TYPE));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::Unpack(Value *value, u32 packFlags) {
  ASSERT_VECTOR_TYPE(value);
  // TODO(benvanik): check if this is a constant - sometimes this is just used
  // to initialize registers.
  Instr *i = AppendInstr(OPCODE_UNPACK_info, packFlags, AllocValue(VEC128_TYPE));
  i->set_src1(value);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::VectorRotateLeft(Value *value1, Value *value2, TypeName partType) {
  ASSERT_VECTOR_TYPE(value1);
  ASSERT_VECTOR_TYPE(value2);

  Instr *i = AppendInstr(OPCODE_VECTOR_ROTATE_LEFT_info, partType,
    AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::VectorAverage(Value *value1, Value *value2, TypeName partType, u32 arithmeticFlags) {
  ASSERT_VECTOR_TYPE(value1);
  ASSERT_VECTOR_TYPE(value2);

  // This is shady.
  u32 flags = partType | (arithmeticFlags << 8);
  ASSERT_ZERO(flags >> 16);

  Instr *i = AppendInstr(OPCODE_VECTOR_AVERAGE_info, u16(flags), AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::VectorMax(Value *value1, Value *value2, TypeName partType, u32 arithmeticFlags) {
  ASSERT_TYPES_EQUAL(value1, value2);

  u16 flags = arithmeticFlags | (partType << 8);
  Instr *i = AppendInstr(OPCODE_VECTOR_MAX_info, flags, AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::VectorMin(Value *value1, Value *value2, TypeName partType, u32 arithmeticFlags) {
  ASSERT_TYPES_EQUAL(value1, value2);

  u16 flags = arithmeticFlags | (partType << 8);
  Instr *i = AppendInstr(OPCODE_VECTOR_MIN_info, flags, AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::VectorCompareXX(const OpcodeInfo &opcode, Value *value1, Value *value2, TypeName partType) {
  ASSERT_TYPES_EQUAL(value1, value2);

  // TODO(benvanik): check how this is used - sometimes I think it's used to
  //     load bitmasks and may be worth checking constants on.

  Instr *i = AppendInstr(opcode, partType, AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::VectorCompareEQ(Value *value1, Value *value2, TypeName partType) {
  return VectorCompareXX(OPCODE_VECTOR_COMPARE_EQ_info, value1, value2, partType);
}

Value *HIRBuilder::VectorCompareSGT(Value *value1, Value *value2, TypeName partType) {
  return VectorCompareXX(OPCODE_VECTOR_COMPARE_SGT_info, value1, value2, partType);
}

Value *HIRBuilder::VectorCompareSGE(Value *value1, Value *value2, TypeName partType) {
  return VectorCompareXX(OPCODE_VECTOR_COMPARE_SGE_info, value1, value2, partType);
}

Value *HIRBuilder::VectorCompareUGT(Value *value1, Value *value2, TypeName partType) {
  return VectorCompareXX(OPCODE_VECTOR_COMPARE_UGT_info, value1, value2, partType);
}

Value *HIRBuilder::VectorCompareUGE(Value *value1, Value *value2, TypeName partType) {
  return VectorCompareXX(OPCODE_VECTOR_COMPARE_UGE_info, value1, value2, partType);
}

Value *HIRBuilder::VectorAdd(Value *value1, Value *value2, TypeName partType, u32 arithmeticFlags) {
  ASSERT_VECTOR_TYPE(value1);
  ASSERT_VECTOR_TYPE(value2);

  // This is shady.
  u32 flags = partType | (arithmeticFlags << 8);
  ASSERT_ZERO(flags >> 16);

  Instr *i = AppendInstr(OPCODE_VECTOR_ADD_info, (u16)flags, AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::VectorSub(Value *value1, Value *value2, TypeName partType, u32 arithmeticFlags) {
  ASSERT_VECTOR_TYPE(value1);
  ASSERT_VECTOR_TYPE(value2);

  // This is shady.
  u32 flags = partType | (arithmeticFlags << 8);
  ASSERT_ZERO(flags >> 16);

  Instr *i = AppendInstr(OPCODE_VECTOR_SUB_info, (u16)flags, AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::VectorShl(Value *value1, Value *value2, TypeName partType) {
  ASSERT_VECTOR_TYPE(value1);
  ASSERT_VECTOR_TYPE(value2);

  Instr *i = AppendInstr(OPCODE_VECTOR_SHL_info, partType, AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::VectorShr(Value *value1, Value *value2, TypeName partType) {
  ASSERT_VECTOR_TYPE(value1);
  ASSERT_VECTOR_TYPE(value2);

  Instr *i = AppendInstr(OPCODE_VECTOR_SHR_info, partType, AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::VectorSha(Value *value1, Value *value2, TypeName partType) {
  ASSERT_VECTOR_TYPE(value1);
  ASSERT_VECTOR_TYPE(value2);

  Instr *i = AppendInstr(OPCODE_VECTOR_SHA_info, partType, AllocValue(value1->type));
  i->set_src1(value1);
  i->set_src2(value2);
  i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::VectorConvertI2F(Value *value, u32 arithmeticFlags) {
  ASSERT_VECTOR_TYPE(value);

  Instr *i = AppendInstr(OPCODE_VECTOR_CONVERT_I2F_info, arithmeticFlags,
    AllocValue(value->type));
  i->set_src1(value);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::VectorConvertF2I(Value *value, u32 arithmeticFlags) {
  ASSERT_VECTOR_TYPE(value);

  Instr *i = AppendInstr(OPCODE_VECTOR_CONVERT_F2I_info, arithmeticFlags,
    AllocValue(value->type));
  i->set_src1(value);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::LoadVectorShl(Value *sh) {
  ASSERT(sh->type == INT8_TYPE);
  Instr *i = AppendInstr(OPCODE_LOAD_VECTOR_SHL_info, 0, AllocValue(VEC128_TYPE));
  i->set_src1(sh);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}

Value *HIRBuilder::LoadVectorShr(Value *sh) {
  ASSERT(sh->type == INT8_TYPE);
  Instr *i = AppendInstr(OPCODE_LOAD_VECTOR_SHR_info, 0, AllocValue(VEC128_TYPE));
  i->set_src1(sh);
  i->src2.value = i->src3.value = NULL;
  return i->dest;
}
}  // namespace XE::CPU::HIR