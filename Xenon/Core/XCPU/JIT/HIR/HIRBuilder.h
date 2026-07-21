/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2021 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#pragma once

#include <vector>

#include "Core/XCPU/JIT/HIR/Arena.h"
#include "Core/XCPU/JIT/HIR/Block.h"
#include "Core/XCPU/JIT/HIR/Instr.h"
#include "Core/XCPU/JIT/HIR/Label.h"
#include "Core/XCPU/JIT/HIR/Opcodes.h"
#include "Core/XCPU/JIT/HIR/Value.h"

#include "Core/HLE/HLEFunction.h"

namespace Xe::XCPU::HIR {

// HIR Builder
class HIRBuilder {
public:
  HIRBuilder();
  ~HIRBuilder();

  void Reset();

  Arena *getArena() const { return arena_; }

  HIRBlock *getCurrentBlock() const;

  void Comment(const std::string_view &value);
  //void Comment(const Base::StringBuffer &value);

  template <typename... Args>
  void CommentFormat(const std::string_view format, const Args&... args) {
    static const u32 kMaxCommentSize = 1024;
    char *p = reinterpret_cast<char *>(arena_->Alloc(kMaxCommentSize, 1));
    auto result = fmt::format_to_n(p, kMaxCommentSize - 1, fmt::runtime(format), args...);
    p[result.size] = '\0';
    size_t rewind = kMaxCommentSize - 1 - result.size;
    arena_->Rewind(rewind);
    CommentBuffer(p);
  }

  void Nop();

  // Source offset annotation for per-instruction CIA/NIA/CI writes.
  // |addr| = PPC address, |rawWord| = raw instruction word,
  // |needsFullPrologue| = whether to write CIA/NIA in addition to CI.
  void SourceOffset(u64 addr, u32 rawWord, bool needsFullPrologue);

  // trace info/etc
  void DebugBreak();
  void DebugBreakTrue(Value *cond);

  // General Purpose Registers
  Value *LoadGPR(u32 reg);
  void StoreGPR(u32 reg, Value *value);

  // Floating-Point Registers
  Value *LoadFPR(u32 reg);
  void StoreFPR(u32 reg, Value *value);

  // Vector Registers
  Value *LoadVR(u32 reg);
  void StoreVR(u32 reg, Value *value);

  // Special Purpose Registers

  // Link Register (64-bit)
  Value *LoadLR();
  void StoreLR(Value *value);

  // Count Register (64-bit)
  Value *LoadCTR();
  void StoreCTR(Value *value);

  // Fixed-Point Exception Register
  Value *LoadXER();
  void StoreXER(Value *value);

  // XER individual bits
  Value *LoadCA();   // Carry
  void StoreCA(Value *value);
  Value *LoadSO();   // Summary Overflow
  Value *LoadOV();   // Overflow

  // MSR
  Value *LoadMSR();
  void StoreMSR(Value *value);

  // Floating-Point Status and Control Register
  Value *LoadFPSCR();
  void StoreFPSCR(Value *value);

  // Condition Register
  Value *LoadCR();
  Value *LoadCR(u8 crField); // Load a single 4-bit CR field (0-7) as INT8
  void StoreCR(Value *value);

  // Updates CR0 performing a comparisson against a zero filled value
  void UpdateCR0(Value *value, bool isSigned = true, bool usesMSR = true);
  // Updates CRx performing a comparisson against a zero filled value
  void UpdateCRx(u8 crIndex, Value *value, bool isSigned = true, bool usesMSR = true);
  // Updates CRx performing a comparisson against both values
  void UpdateCRx(u8 crIndex, Value *lhs, Value *rhs, bool isSigned = true, bool usesMSR = true);
  // Updates CR field 6 based on a vector compare result (VEC128).
  // CR6[0] = all elements true, CR6[2] = no elements true.
  void UpdateCR6(Value *value);
  // Updates FPSCR FPRF bits based on the floating-point result value.
  // Classifies the result and sets FPRF accordingly.
  // If updateCR1 is true, also updates CR1 from FPSCR[FX,FEX,VX,OX].
  void UpdateFPSCR(Value *value, bool updateCR1 = false);
  // Updates CR1 field from FPSCR's FX, FEX, VX and OX bits.
  void UpdateCR1();
  // Stores the saturation flag into VSCR.SAT (sticky OR).
  // |satFlag| should be INT8 (0 or 1) from DidSaturate().
  void StoreSAT(Value *satFlag);

  // Main Operations

  Value *AllocValue(TypeName type = INT64_TYPE);
  Value *CloneValue(Value *source);

  // Loads Zero filled Value of the given type
  Value *LoadZero(TypeName type);
  Value *LoadZeroInt8() { return LoadZero(INT8_TYPE); }
  Value *LoadZeroInt16() { return LoadZero(INT16_TYPE); }
  Value *LoadZeroInt32() { return LoadZero(INT32_TYPE); }
  Value *LoadZeroInt64() { return LoadZero(INT64_TYPE); }
  Value *LoadZeroFloat32() { return LoadZero(FLOAT32_TYPE); }
  Value *LoadZeroFloat64() { return LoadZero(FLOAT64_TYPE); }
  Value *LoadZeroVec128() { return LoadZero(VEC128_TYPE); }

  Value *LoadConstantInt8(s8 value);
  Value *LoadConstantUint8(u8 value);
  Value *LoadConstantInt16(s16 value);
  Value *LoadConstantUint16(u16 value);
  Value *LoadConstantInt32(s32 value);
  Value *LoadConstantUint32(u32 value);
  Value *LoadConstantInt64(s64 value);
  Value *LoadConstantUint64(u64 value);
  Value *LoadConstantFloat32(f32 value);
  Value *LoadConstantFloat64(f64 value);
  Value *LoadConstantVec128(const Base::Vector128 &value);

  // Memory Operations

  Value *LoadContext(size_t offset, TypeName type);
  void StoreContext(size_t offset, Value *value);
  void ContextBarrier();
  Value *LoadOffset(Value *address, Value *offset, TypeName type, u32 loadFlags = 0);
  void StoreOffset(Value *address, Value *offset, Value *value, u32 storeFlags = 0);
  Value *Load(Value *address, TypeName type, u32 loadFlags = 0);
  void Store(Value *address, Value *value, u32 storeFlags = 0);
  void Memset(Value *address, Value *value, Value *length);
  void CacheControl(Value *address, size_t cacheLineSize, CacheControlType type);
  void MemoryBarrier();
  Value *ByteSwap(Value *value);
  Value *AtomicExchange(Value *address, Value *newValue);
  Value *AtomicCompareExchange(Value *address, Value *oldValue, Value *newValue);

  // Atomic Load/Store operations
  Value *LoadReserved();
  void StoreReserved(Value *value);

  // System Operations

  // Emits a Storage (ISI/DSI) exception check.
  // Returns from executing block if any exception is found.
  void StorageExceptionCheck();
  // Emits a full Synchronous exception check.
  // Returns from executing block if any exception is found.
  void SyncExceptionCheck();
  void Trap(u16 trapCode = 0);
  void TrapTrue(Value *cond, u16 trapCode = 0);
  void CallInterpreter(uPPCInstr instrData, void *interpFn);
  Value *LoadTimeBase();
  Value *EmitMFSPR(uPPCInstr instrData);
  void EmitMTSPR(Value *sprValue, uPPCInstr instrData);
  void EmitMTMSRD(Value *sprValue, uPPCInstr instrData);
  void EmitSyscall(uPPCInstr instrData);
  void EmitRFID(uPPCInstr instrData);

  // Emits a direct call to the given HLE function pointer.
  // The backend is responsible for calling fnPtr(ppeState).
  // src1.offset stores the function pointer as u64.
  void CallHLEFunction(Core::HLE::HLEFunctionPtr fnPtr);

  // Branch Control
  void Branch(uPPCInstr currentInstr);
  void BranchConditional(uPPCInstr currentInstr);
  void BranchConditionalToCTR(uPPCInstr currentInstr);
  void BranchConditionalToLR(uPPCInstr currentInstr);

  // Backend - Specific Operations

  void SetRoundingMode(Value *value);

  // Regular Operations

  Value *Assign(Value *value);
  Value *Cast(Value *value, TypeName targetType);
  Value *ZeroExtend(Value *value, TypeName targetType);
  Value *SignExtend(Value *value, TypeName targetType);
  Value *Truncate(Value *value, TypeName targetType);
  Value *Convert(Value *value, TypeName targetType, RoundMode roundMode = ROUND_TO_ZERO);
  Value *Round(Value *value, RoundMode roundMode);
  Value *VectorConvertI2F(Value *value, u32 arithmeticFlags = 0);
  Value *VectorConvertF2I(Value *value, u32 arithmeticFlags = 0);
  Value *LoadVectorShl(Value *sh);
  Value *LoadVectorShr(Value *sh);
  Value *Max(Value *value1, Value *value2);
  Value *VectorMax(Value *value1, Value *value2, TypeName partType, u32 arithmeticFlags = 0);
  Value *Min(Value *value1, Value *value2);
  Value *VectorMin(Value *value1, Value *value2, TypeName partType, u32 arithmeticFlags = 0);
  Value *Select(Value *cond, Value *value1, Value *value2);
  Value *IsTrue(Value *value);
  Value *IsFalse(Value *value);
  Value *IsNan(Value *value);
  Value *CompareEQ(Value *value1, Value *value2);
  Value *CompareNE(Value *value1, Value *value2);
  Value *CompareSLT(Value *value1, Value *value2);
  Value *CompareSLE(Value *value1, Value *value2);
  Value *CompareSGT(Value *value1, Value *value2);
  Value *CompareSGE(Value *value1, Value *value2);
  Value *CompareULT(Value *value1, Value *value2);
  Value *CompareULE(Value *value1, Value *value2);
  Value *CompareUGT(Value *value1, Value *value2);
  Value *CompareUGE(Value *value1, Value *value2);
  Value *DidSaturate(Value *value);
  Value *VectorCompareEQ(Value *value1, Value *value2, TypeName partType);
  Value *VectorCompareSGT(Value *value1, Value *value2, TypeName partType);
  Value *VectorCompareSGE(Value *value1, Value *value2, TypeName partType);
  Value *VectorCompareUGT(Value *value1, Value *value2, TypeName partType);
  Value *VectorCompareUGE(Value *value1, Value *value2, TypeName partType);
  Value *Add(Value *value1, Value *value2, u32 arithmeticFlags = 0);
  Value *AddWithCarry(Value *value1, Value *value2, Value *value3, u32 arithmeticFlags = 0);
  Value *VectorAdd(Value *value1, Value *value2, TypeName partType, u32 arithmeticFlags = 0);
  Value *Sub(Value *value1, Value *value2, u32 arithmeticFlags = 0);
  Value *VectorSub(Value *value1, Value *value2, TypeName partType, u32 arithmeticFlags = 0);
  Value *Mul(Value *value1, Value *value2, u32 arithmeticFlags = 0);
  Value *MulHi(Value *value1, Value *value2, u32 arithmeticFlags = 0);
  Value *Div(Value *value1, Value *value2, u32 arithmeticFlags = 0);
  Value *MulAdd(Value *value1, Value *value2, Value *value3);  // (1 * 2) + 3
  Value *MulSub(Value *value1, Value *value2, Value *value3);  // (1 * 2) - 3
  Value *Neg(Value *value);
  Value *Abs(Value *value);
  Value *Sqrt(Value *value);
  Value *RSqrt(Value *value);
  Value *Recip(Value *value);
  Value *Pow2(Value *value);
  Value *Log2(Value *value);
  Value *DotProduct3(Value *value1, Value *value2);
  Value *DotProduct4(Value *value1, Value *value2);
  Value *And(Value *value1, Value *value2);
  Value *AndNot(Value *value1, Value *value2);
  Value *Or(Value *value1, Value *value2);
  Value *Xor(Value *value1, Value *value2);
  Value *Not(Value *value);
  Value *Shl(Value *value1, Value *value2);
  Value *Shl(Value *value1, s8 value2);
  Value *VectorShl(Value *value1, Value *value2, TypeName partType);
  Value *Shr(Value *value1, Value *value2);
  Value *Shr(Value *value1, s8 value2);
  Value *VectorShr(Value *value1, Value *value2, TypeName partType);
  Value *Sha(Value *value1, Value *value2);
  Value *Sha(Value *value1, s8 value2);
  Value *VectorSha(Value *value1, Value *value2, TypeName partType);
  Value *RotateLeft(Value *value1, Value *value2);
  Value *VectorRotateLeft(Value *value1, Value *value2, TypeName partType);
  Value *VectorAverage(Value *value1, Value *value2, TypeName partType, u32 arithmeticFlags);
  Value *CountLeadingZeros(Value *value);
  Value *Insert(Value *value, Value *index, Value *part);
  Value *Insert(Value *value, u64 index, Value *part);
  Value *Extract(Value *value, Value *index, TypeName targetType);
  Value *Extract(Value *value, u8 index, TypeName targetType);
  Value *Splat(Value *value, TypeName targetType);
  Value *Permute(Value *control, Value *value1, Value *value2, TypeName partType);
  Value *Swizzle(Value *value, TypeName partType, u32 swizzleMask);
  Value *Pack(Value *value, u32 pack_flags = 0);
  Value *Pack(Value *value1, Value *value2, u32 pack_flags = 0);
  Value *Unpack(Value *value, u32 pack_flags = 0);

private:
  HIRBlock *AppendBlock();
  Instr *AppendInstr(const OpcodeInfo &opcode, u16 flags, Value *dest = 0);
  void CommentBuffer(const char *p);
  Value *CompareXX(const OpcodeInfo &opcode, Value *value1, Value *value2);
  Value *VectorCompareXX(const OpcodeInfo &opcode, Value *value1, Value *value2, TypeName partType);

protected:
  Arena *arena_;

  u32 nextValueOrdinal;
  // Current block being constructed
  HIRBlock *currentBlock;
};

}  // namespace Xe::XCPU::HIR
