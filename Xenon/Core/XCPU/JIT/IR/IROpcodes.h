/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"

namespace Xe::XCPU::JIT::IR {

  // Rounding mode for FP Instructions
  enum RoundingMode {
    RoundToZero,        // Round To Zero
    RoundToNear,        // Round to Near
    RoundToMinusInf,    // Round to Minus Infinity
    RoundToPlusInf,     // Round to Positive Infinity
    RoundUsingFPSCR,    // Use FPSCR[RN]
  };

  // Cache control type for DCBT/DCBTST/etc...
  enum CacheControlType {
    CacheControlTypeDataBlockTouch,
    CacheControlTypeDataBlockTouchForStore,
    CacheControlTypeDataBlockStore,
    CacheControlTypeDataBlockStoreAndFlush,
  };

  //=============================================================================
  // IR Opcodes
  //=============================================================================

  // Basic operation performed by the IR
  // PPC Instructions should be easily mapped to this
  // These get converted onto Host instrucitons on our backend emitter.
  enum class IROp : u16 {
    // System And Control
    Return,
    // Memory Operations
    MemLoad,        // Load from memory (MMU Read 8/16/32/64)
    MemStore,       // Store to memory (MMU Write 8/16/32/64)
    MemSet,         // Memory Memset
    AtomicMemLoad,  // Conditional load (Atomic PPC Ops) (lwarx/ldwarx)
    AtomicMemStore, // Conditional Store (Atomic PPC Ops) (stwcx/stdcx)
    CacheControl,   // Cache related instructions (dcbt/dcbtst/etc)
    MemoryBarrier,  // Memory barrier

    // High-Level Register Operations (for optimization passes)
    LoadGPR,        // Load from GPR[index]
    StoreGPR,       // Store to GPR[index]
    LoadFPR,        // Load from FPR[index]
    StoreFPR,       // Store to FPR[index]
    LoadVR,         // Load from VR[index]
    StoreVR,        // Store to VR[index]
    LoadSPR,        // Load from SPR (LR, CTR, XER, etc.)
    StoreSPR,       // Store to SPR

    Comment,        // Comment for debugging
    Nop,            // No operation
    Trap,           // Always Trap
    TrapTrue,       // Trap if Condition is true
    Branch,         // Branch
    BranchTrue,     // Branch if condition true
    BranchFalse,    // Branch if condition false
    Assign, 
    Cast,
    ZeroExtend,
    SignExtend,
    Truncate,
    Convert,
    Round,
    VectorConvertIntToFloat,
    VectorConvertFloatToInt,
    LoadVectorShiftLeft,
    LoadVectorShiftRight,
    Max,
    VectorMax,
    Min,
    VectorMin,
    Select,
    IsTrue,
    IsFalse,
    IsNaN,
    CompareEQ,
    CompareNE,
    CompareSLT,
    CompareSLE,
    CompareSGT,
    CompareSGE,
    CompareULT,
    CompareULE,
    CompareUGT,
    CompareUGE,
    DidSaturate,
    VectorCompareEQ,
    VectorCompareSGT,
    VectorCompareSGE,
    VectorCompareUGT,
    VectorCompareUGE,
    Add,
    AddWithCarry,
    VectorAdd,
    Sub,
    VectorSub,
    Mul,
    MulHi,
    Div,
    MulAdd,
    MulSub,
    Neg,
    Abs,
    Sqrt,
    Rsqrt,
    Recip,
    Pow2,
    Log2,
    DotProduct3,
    DotProduct4,
    And,
    AndNot,
    Or,
    Xor,
    Not,
    ShiftLeft,
    VectorShiftLeft,
    ShiftRight,
    VectorShiftRight,
    ShiftAlgebraic,
    VectorShiftAlgebraic,
    RotateLeft,
    VectorRotateLeft,
    VectorAverage,
    ByteSwap,
    Cntlz,
    Insert,
    Extract,
    Splat,
    Permute,
    Swizzle,
    Pack,
    Unpack,
    AtomicExchange,
    AtomicCompareExchange,
    SetRoundingMode,
  };

  inline const char *IROpToString(IROp op) {
    switch (op) {
      // System And Control
    case IROp::Return: return "return";
      // Memory Operations
    case IROp::MemLoad: return "mem_load";
    case IROp::MemStore: return "mem_store";
    case IROp::MemSet: return "memset";
    case IROp::CacheControl: return "cache_control";
    case IROp::MemoryBarrier: return "memory_barrier";
      // High-level register operations
    case IROp::LoadGPR: return "load_gpr";
    case IROp::StoreGPR: return "store_gpr";
    case IROp::LoadFPR: return "load_fpr";
    case IROp::StoreFPR: return "store_fpr";
    case IROp::LoadVR: return "load_vr";
    case IROp::StoreVR: return "store_vr";
    case IROp::LoadSPR: return "load_spr";
    case IROp::StoreSPR: return "store_spr";
      // Control Flow
    case IROp::Comment: return "comment";
    case IROp::Nop: return "nop";
    case IROp::Trap: return "trap";
    case IROp::TrapTrue: return "trap_true";
    case IROp::Branch: return "br";
    case IROp::BranchTrue: return "br_true";
    case IROp::BranchFalse: return "br_false";
      // Type Conversions
    case IROp::Assign: return "assign";
    case IROp::Cast: return "cast";
    case IROp::ZeroExtend: return "zext";
    case IROp::SignExtend: return "sext";
    case IROp::Truncate: return "trunc";
    case IROp::Convert: return "convert";
    case IROp::Round: return "round";
    case IROp::VectorConvertIntToFloat: return "vec_cvt_i2f";
    case IROp::VectorConvertFloatToInt: return "vec_cvt_f2i";
      // Vector Operations
    case IROp::LoadVectorShiftLeft: return "lvsl";
    case IROp::LoadVectorShiftRight: return "lvsr";
    case IROp::VectorMax: return "vec_max";
    case IROp::VectorMin: return "vec_min";
    case IROp::VectorAdd: return "vec_add";
    case IROp::VectorSub: return "vec_sub";
    case IROp::VectorShiftLeft: return "vec_shl";
    case IROp::VectorShiftRight: return "vec_shr";
    case IROp::VectorShiftAlgebraic: return "vec_sha";
    case IROp::VectorRotateLeft: return "vec_rol";
    case IROp::VectorAverage: return "vec_avg";
    case IROp::VectorCompareEQ: return "vec_cmpeq";
    case IROp::VectorCompareSGT: return "vec_cmpsgt";
    case IROp::VectorCompareSGE: return "vec_cmpsge";
    case IROp::VectorCompareUGT: return "vec_cmpugt";
    case IROp::VectorCompareUGE: return "vec_cmpuge";
      // Min/Max & Select
    case IROp::Max: return "max";
    case IROp::Min: return "min";
    case IROp::Select: return "select";
      // Boolean Tests
    case IROp::IsTrue: return "is_true";
    case IROp::IsFalse: return "is_false";
    case IROp::IsNaN: return "is_nan";
    case IROp::DidSaturate: return "did_saturate";
      // Comparisons (Generic)
    case IROp::CompareEQ: return "cmpeq";
    case IROp::CompareNE: return "cmpne";
    case IROp::CompareSLT: return "cmpslt";
    case IROp::CompareSLE: return "cmpsle";
    case IROp::CompareSGT: return "cmpsgt";
    case IROp::CompareSGE: return "cmpsge";
    case IROp::CompareULT: return "cmpult";
    case IROp::CompareULE: return "cmpule";
    case IROp::CompareUGT: return "cmpugt";
    case IROp::CompareUGE: return "cmpuge";
      // Arithmetic
    case IROp::Add: return "add";
    case IROp::AddWithCarry: return "addc";
    case IROp::Sub: return "sub";
    case IROp::Mul: return "mul";
    case IROp::MulHi: return "mulhi";
    case IROp::Div: return "div";
    case IROp::MulAdd: return "madd";
    case IROp::MulSub: return "msub";
    case IROp::Neg: return "neg";
    case IROp::Abs: return "abs";
    case IROp::Sqrt: return "sqrt";
    case IROp::Rsqrt: return "rsqrt";
    case IROp::Recip: return "recip";
    case IROp::Pow2: return "pow2";
    case IROp::Log2: return "log2";
    case IROp::DotProduct3: return "dp3";
    case IROp::DotProduct4: return "dp4";
      // Bitwise Operations
    case IROp::And: return "and";
    case IROp::AndNot: return "andn";
    case IROp::Or: return "or";
    case IROp::Xor: return "xor";
    case IROp::Not: return "not";
    case IROp::ShiftLeft: return "shl";
    case IROp::ShiftRight: return "shr";
    case IROp::ShiftAlgebraic: return "sha";
    case IROp::RotateLeft: return "rol";
    case IROp::ByteSwap: return "bswap";
    case IROp::Cntlz: return "cntlz";
      // Bit Manipulation
    case IROp::Insert: return "insert";
    case IROp::Extract: return "extract";
    case IROp::Splat: return "splat";
    case IROp::Permute: return "permute";
    case IROp::Swizzle: return "swizzle";
    case IROp::Pack: return "pack";
    case IROp::Unpack: return "unpack";
      // Atomic Operations
    case IROp::AtomicExchange: return "atomic_xchg";
    case IROp::AtomicCompareExchange: return "atomic_cmpxchg";
      // Floating Point Control
    case IROp::SetRoundingMode: return "set_rounding_mode";
    default: return "unknown";
    }
  }
}