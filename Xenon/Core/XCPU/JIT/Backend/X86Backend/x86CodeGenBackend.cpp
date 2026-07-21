/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Base/Arch.h"

#if defined(ARCH_X86) || defined(ARCH_X86_64)

#include "Base/Logging/Log.h"
#include "Core/XeMain.h"
#include "Core/XCPU/Interpreter/PPCInterpreter.h"
#include "Core/XCPU/PPU/PPCInternal.h"
#include "Core/XCPU/JIT/Backend/X86Backend/x86CodeGenBackend.h"
#include "Core/XCPU/JIT/Backend/X86Backend/x86CodeGenHelpers.h"
#include "Core/XCPU/JIT/x86VectorConstants.h"

namespace Xe::XCPU::JIT {

// Global emitter key list, filled at static-init time by x86EmitterRegister
// (via the REGISTER_EMITTER macro) before x86CodeGenBackend::Initialize()
// consumes it in BuildEmitTable().
std::vector<x86EmitterKey> &GetEmitterKeyList() {
  static std::vector<x86EmitterKey> keys;
  return keys;
}

// Helper utilities

// Move an INT64 GP register (containing raw double bits) into an XMM.
static x86::Vec GpToXmmDouble(x86CodeGenBackend *b, x86::Gp src) {
  x86::Vec xmm = newXMM();
  COMP->vmovq(xmm, src.r64());
  return xmm;
}

// Move an XMM double back to an INT64 GP register.
static x86::Gp XmmDoubleToGp(x86CodeGenBackend *b, x86::Vec src) {
  x86::Gp gp = newGP64();
  COMP->vmovq(gp, src);
  return gp;
}


//
// Condition Register
//

// Emits a CR field comparison.
// Uses jg/jl for signed, ja/jb for unsigned
// When instr->flags == 1 (arithmetic Rc=1): checks MSR[SF] to pick 32/64-bit.
// When instr->flags == 0 (explicit cmp/cmpl): compares at input's native size.
static void EmitBuildCR(x86CodeGenBackend *b, const HIR::Instr *instr, bool isSigned) {
  x86::Gp lhs = LoadValueGp(b, instr->src1.value);
  x86::Gp rhs = LoadValueGp(b, instr->src2.value);

  x86::Gp crValue = newGP32();
  x86::Gp tmp = newGP8();

  COMP->xor_(crValue, crValue);

  Label end = newLabel();

  bool useMSRCheck = (instr->flags == 1);

  if (useMSRCheck) {
    // Arithmetic Rc=1 path: check MSR[SF] for comparison mode
    Label sfBitMode = newLabel();

    x86::Gp tempMSR = newGP64();
    COMP->mov(tempMSR, SPRPtr(MSR));
    COMP->bt(tempMSR, asmjit::Imm(63)); // SF is bit 63 on LE
    COMP->jc(sfBitMode);

    // 32-bit mode
    {
      Label gt32 = newLabel();
      Label lt32 = newLabel();

      COMP->cmp(lhs.r32(), rhs.r32());
      if (isSigned) { COMP->jg(gt32); COMP->jl(lt32); }
      else          { COMP->ja(gt32); COMP->jb(lt32); }
      COMP->mov(tmp, asmjit::Imm(2));
      COMP->or_(crValue.r8(), tmp.r8());
      COMP->jmp(end);

      COMP->bind(gt32);
      COMP->mov(tmp, asmjit::Imm(4));
      COMP->or_(crValue.r8(), tmp.r8());
      COMP->jmp(end);

      COMP->bind(lt32);
      COMP->mov(tmp, asmjit::Imm(8));
      COMP->or_(crValue.r8(), tmp.r8());
      COMP->jmp(end);
    }

    // 64-bit mode
    COMP->bind(sfBitMode);
    {
      Label gt64 = newLabel();
      Label lt64 = newLabel();

      COMP->cmp(lhs.r64(), rhs.r64());
      if (isSigned) { COMP->jg(gt64); COMP->jl(lt64); }
      else          { COMP->ja(gt64); COMP->jb(lt64); }
      COMP->mov(tmp, asmjit::Imm(2));
      COMP->or_(crValue.r8(), tmp.r8());
      COMP->jmp(end);

      COMP->bind(gt64);
      COMP->mov(tmp, asmjit::Imm(4));
      COMP->or_(crValue.r8(), tmp.r8());
      COMP->jmp(end);

      COMP->bind(lt64);
      COMP->mov(tmp, asmjit::Imm(8));
      COMP->or_(crValue.r8(), tmp.r8());
    }
  } else {
    // Explicit compare path (cmp/cmpl): compare at input's native size
    Label gt = newLabel();
    Label lt = newLabel();

    COMP->cmp(lhs, rhs);
    if (isSigned) { COMP->jg(gt); COMP->jl(lt); }
    else          { COMP->ja(gt); COMP->jb(lt); }
    COMP->mov(tmp, asmjit::Imm(2));
    COMP->or_(crValue.r8(), tmp.r8());
    COMP->jmp(end);

    COMP->bind(gt);
    COMP->mov(tmp, asmjit::Imm(4));
    COMP->or_(crValue.r8(), tmp.r8());
    COMP->jmp(end);

    COMP->bind(lt);
    COMP->mov(tmp, asmjit::Imm(8));
    COMP->or_(crValue.r8(), tmp.r8());
  }

  COMP->bind(end);

  // SO bit from XER
#ifdef __LITTLE_ENDIAN__
  COMP->mov(tmp.r32(), SPRPtr(XER));
  COMP->shr(tmp.r32(), asmjit::Imm(31));
#else
  COMP->mov(tmp.r32(), SPRPtr(XER));
  COMP->and_(tmp.r32(), asmjit::Imm(1));
#endif
  COMP->shl(tmp, asmjit::Imm(3 - CR_BIT_SO));
  COMP->or_(crValue.r8(), tmp.r8());

  TagStoreReg(instr->dest, crValue);
}

// Performs a signed comparison between values
// CR field bits: LT=8, GT=4, EQ=2, SO=from XER
// flags=1: check MSR[SF] for width (arithmetic Rc=1)
// flags=0: compare at native input size (cmp/cmpi)
REGISTER_EMITTER(OPCODE_BUILD_CR_SIGNED, Emit_BUILD_CR_SIGNED)
static void Emit_BUILD_CR_SIGNED(x86CodeGenBackend *b, const HIR::Instr *instr) {
  EmitBuildCR(b, instr, true);
}

// Performs an unsigned comparison between values
// flags=1: check MSR[SF] for width (arithmetic Rc=1)
// flags=0: compare at native input size (cmpl/cmpli)
REGISTER_EMITTER(OPCODE_BUILD_CR_UNSIGNED, Emit_BUILD_CR_UNSIGNED)
static void Emit_BUILD_CR_UNSIGNED(x86CodeGenBackend *b, const HIR::Instr *instr) {
  EmitBuildCR(b, instr, false);
}

// Sets the specified CR field to the specified value.
// sig: X_O_V  (no dest, src1 = CR field index, src2 = 4-bit field value)
REGISTER_EMITTER(OPCODE_SET_CR_FIELD, Emit_SET_CR_FIELD)
static void Emit_SET_CR_FIELD(x86CodeGenBackend *b, const HIR::Instr *instr) {
  const u32 index = static_cast<u32>(instr->src1.offset);
  const u32 sh = (7 - index) * 4;
  const u32 clearMask = ~(0xFu << sh);

  x86::Gp field = LoadValueGp(b, instr->src2.value);

  x86::Gp field32 = newGP32();
  COMP->movzx(field32, field.r8());

  x86::Gp tempCR = newGP32();
  COMP->mov(tempCR, CRValPtr());
  COMP->and_(tempCR, asmjit::Imm(clearMask));
  COMP->shl(field32, asmjit::Imm(sh));
  COMP->or_(tempCR, field32);
  COMP->mov(CRValPtr(), tempCR);
}

//
// Assign
//

REGISTER_EMITTER(OPCODE_ASSIGN, Emit_ASSIGN)
static void Emit_ASSIGN(x86CodeGenBackend *b, const HIR::Instr *instr) {
  switch (instr->dest->type) {
  case HIR::INT8_TYPE:  { x86::Gp s = LoadValueGp(b, instr->src1.value);  x86::Gp d = newGP8();  COMP->mov(d, s.r8());  TagStoreReg(instr->dest, d); } break;
  case HIR::INT16_TYPE: { x86::Gp s = LoadValueGp(b, instr->src1.value);  x86::Gp d = newGP16(); COMP->mov(d, s.r16()); TagStoreReg(instr->dest, d); } break;
  case HIR::INT32_TYPE: { x86::Gp s = LoadValueGp(b, instr->src1.value);  x86::Gp d = newGP32(); COMP->mov(d, s.r32()); TagStoreReg(instr->dest, d); } break;
  case HIR::INT64_TYPE: { x86::Gp s = LoadValueGp(b, instr->src1.value);  x86::Gp d = newGP64(); COMP->mov(d, s.r64()); TagStoreReg(instr->dest, d); } break;
  case HIR::FLOAT32_TYPE: { x86::Vec s = LoadValueXmm(b, instr->src1.value); x86::Vec d = newXMM(); COMP->vmovaps(d, s); TagStoreReg(instr->dest, d); } break;
  case HIR::FLOAT64_TYPE: { x86::Vec s = LoadValueXmm(b, instr->src1.value); x86::Vec d = newXMM(); COMP->vmovaps(d, s); TagStoreReg(instr->dest, d); } break;
  case HIR::VEC128_TYPE:  { x86::Vec s = LoadValueXmm(b, instr->src1.value); x86::Vec d = newXMM(); COMP->vmovaps(d, s); TagStoreReg(instr->dest, d); } break;
  default: UNREACHABLE_MSG("Unimplemented ASSIGN type."); return;
  }
}

//
// Cast
//

REGISTER_EMITTER(OPCODE_CAST, Emit_CAST)
  static void Emit_CAST(x86CodeGenBackend *b, const HIR::Instr *instr) {
  switch (instr->dest->type) {
  case HIR::INT32_TYPE: {
    x86::Gp dst = newGP32();
    x86::Vec src = LoadValueXmm(b, instr->src1.value);
    COMP->vmovd(dst, src);
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT64_TYPE: {
    x86::Gp dst = newGP64();
    x86::Vec src = LoadValueXmm(b, instr->src1.value);
    COMP->vmovq(dst, src);
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::FLOAT32_TYPE: {
    x86::Gp src = LoadValueGp(b, instr->src1.value);
    x86::Vec dst = newXMM();
    COMP->vmovd(dst, src);
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::FLOAT64_TYPE: {
    x86::Gp src = LoadValueGp(b, instr->src1.value);
    x86::Vec dst = newXMM();
    COMP->vmovq(dst, src);
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented CAST type."); return;
  }
}

//
// Zero Extend
//

REGISTER_EMITTER(OPCODE_ZERO_EXTEND, Emit_ZERO_EXTEND)
  static void Emit_ZERO_EXTEND(x86CodeGenBackend *b, const HIR::Instr *instr) {
  x86::Gp src = LoadValueGp(b, instr->src1.value);
  x86::Gp dst;
  switch (instr->dest->type) {
  case HIR::INT16_TYPE:
    dst = newGP16();
    COMP->movzx(dst, src.r8());
    break;
  case HIR::INT32_TYPE:
    dst = newGP32();
    if (instr->src1.value->type == HIR::INT16_TYPE) {
      COMP->movzx(dst, src.r16());
    } else {
      COMP->movzx(dst, src.r8());
    }
    break;
  case HIR::INT64_TYPE:
    dst = newGP64();
    if (instr->src1.value->type == HIR::INT32_TYPE) {
      COMP->mov(dst.r32(), src.r32());
    } else if (instr->src1.value->type == HIR::INT16_TYPE) {
      COMP->movzx(dst.r32(), src.r16());
    } else {
      COMP->movzx(dst.r32(), src.r8());
    }
    break;
  default: UNREACHABLE_MSG("Unimplemented OPCODE_ZERO_EXTEND type."); return;
  }
  TagStoreReg(instr->dest, dst);
}

//
// Sign Extend
//

REGISTER_EMITTER(OPCODE_SIGN_EXTEND, Emit_SIGN_EXTEND)
  static void Emit_SIGN_EXTEND(x86CodeGenBackend *b, const HIR::Instr *instr) {
  x86::Gp src = LoadValueGp(b, instr->src1.value);
  x86::Gp dst;
  switch (instr->dest->type) {
  case HIR::INT16_TYPE:
    dst = newGP16();
    COMP->movsx(dst, src.r8());
    break;
  case HIR::INT32_TYPE:
    dst = newGP32();
    if (instr->src1.value->type == HIR::INT16_TYPE) {
      COMP->movsx(dst, src.r16());
    } else {
      COMP->movsx(dst, src.r8());
    }
    break;
  case HIR::INT64_TYPE:
    dst = newGP64();
    if (instr->src1.value->type == HIR::INT32_TYPE) {
      COMP->movsxd(dst, src.r32());
    } else if (instr->src1.value->type == HIR::INT16_TYPE) {
      COMP->movsx(dst, src.r16());
    } else {
      COMP->movsx(dst, src.r8());
    }
    break;
  default: UNREACHABLE_MSG("Unimplemented OPCODE_SIGN_EXTEND type."); return;
  }
  TagStoreReg(instr->dest, dst);
}

//
// Truncate
//

REGISTER_EMITTER(OPCODE_TRUNCATE, Emit_TRUNCATE)
  static void Emit_TRUNCATE(x86CodeGenBackend *b, const HIR::Instr *instr) {
  x86::Gp src = LoadValueGp(b, instr->src1.value);
  x86::Gp dst;
  switch (instr->dest->type) {
  case HIR::INT8_TYPE:
    dst = newGP8();
    if (instr->src1.value->type == HIR::INT16_TYPE ||
      instr->src1.value->type == HIR::INT32_TYPE ||
      instr->src1.value->type == HIR::INT64_TYPE) {
      COMP->movzx(dst.r32(), src.r8());
    }
    break;
  case HIR::INT16_TYPE:
    dst = newGP16();
    if (instr->src1.value->type == HIR::INT32_TYPE ||
      instr->src1.value->type == HIR::INT64_TYPE) {
      COMP->movzx(dst.r32(), src.r16());
    }
    break;
  case HIR::INT32_TYPE:
    dst = newGP32();
    COMP->mov(dst, src.r32());
    break;
  default: UNREACHABLE_MSG("Unimplemented OPCODE_TRUNCATE type."); return;
  }
  TagStoreReg(instr->dest, dst);
}

//
// Convert
//

REGISTER_EMITTER(OPCODE_CONVERT, Emit_CONVERT)
  static void Emit_CONVERT(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Get source type
  auto srcType = instr->src1.value->type;
  // Get destination type
  auto dstType = instr->dest->type;

  switch (dstType) {
  case HIR::INT32_TYPE: {
    x86::Gp dst = newGP32();
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    // Source = F32
    if (srcType == HIR::FLOAT32_TYPE) {
      if (instr->flags == HIR::ROUND_TO_ZERO) {
        COMP->vcvttss2si(dst, src1);
      } else {
        COMP->vcvtss2si(dst, src1);
      }
    } else if (srcType == HIR::FLOAT64_TYPE) {
      x86::Vec tmp = newXMM();
      COMP->vminsd(tmp, src1, LoadXmmConst(b, XMMIntMaxPD));
      if (instr->flags == HIR::ROUND_TO_ZERO) {
        COMP->vcvttsd2si(dst, tmp);
      } else {
        COMP->vcvtsd2si(dst, tmp);
      }
    }

    // Store value back
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT64_TYPE: {
    x86::Gp dst = newGP64();
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    if (srcType == HIR::FLOAT64_TYPE) {
      x86::Gp tmp = newGP64();
      x86::Gp tmp1 = newGP64();
      x86::Gp sat = newGP64();

      COMP->movq(tmp, src1);

      if (instr->flags == HIR::ROUND_TO_ZERO) {
        COMP->vcvttsd2si(dst, src1);
      } else {
        COMP->vcvtsd2si(dst, src1);
      }

      // Saturate positive overflow

      // Check if result = 0x8000000000000000
      COMP->mov(tmp1, imm(0x1));
      COMP->shl(tmp1, imm(63));
      COMP->cmp(tmp1, dst);
      COMP->sete(sat);
      COMP->movzx(tmp1, sat.r8());
      COMP->shr(tmp, imm(63));
      COMP->xor_(tmp, imm(1));
      COMP->and_(tmp1, tmp);
      COMP->sub(dst, tmp1);
    }
    // Store value back
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::FLOAT32_TYPE: {
    x86::Vec dst = newXMM();

    if (srcType == HIR::INT32_TYPE) {
      x86::Gp src1 = LoadValueGp(b, instr->src1.value);
      COMP->vcvtsi2ss(dst, dst, src1);
    } else if (srcType == HIR::FLOAT64_TYPE) {
      x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
      COMP->vcvtsd2ss(dst, dst, src1);
    }

    // Store value back
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::FLOAT64_TYPE: {
    x86::Vec dst = newXMM();

    if (srcType == HIR::INT64_TYPE) {
      x86::Gp src1 = LoadValueGp(b, instr->src1.value);
      COMP->vcvtsi2sd(dst, dst, src1);
    }
    else if (srcType == HIR::FLOAT32_TYPE) {
      x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
      COMP->vcvtss2sd(dst, dst, src1);
    }

    // Store value back
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented OPCODE_CONVERT type."); return;
  }
}

//
// Round
//

REGISTER_EMITTER(OPCODE_ROUND, Emit_ROUND)
  static void Emit_ROUND(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Vec dst = newXMM();
  // Get source
  x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
  // Rounding Mode
  u8 roundMode = 0;

  switch (instr->flags) {
  case HIR::ROUND_TO_ZERO: roundMode = 0b00000011; break;
  case HIR::ROUND_TO_NEAREST: roundMode = 0b00000000; break;
  case HIR::ROUND_TO_MINUS_INFINITY: roundMode = 0b00000001; break;
  case HIR::ROUND_TO_POSITIVE_INFINITY: roundMode = 0b00000010; break;
  }

  switch (instr->dest->type) {
  case HIR::FLOAT32_TYPE: {
    COMP->vroundss(dst, dst, src1, imm(roundMode));
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::FLOAT64_TYPE: {
    COMP->vroundsd(dst, dst, src1, imm(roundMode));
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::VEC128_TYPE: {
    COMP->vroundps(dst, src1, imm(roundMode));
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented OPCODE_ROUND type."); return;
  }
}

//
// Max
//

REGISTER_EMITTER(OPCODE_MAX, Emit_MAX)
  static void Emit_MAX(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Vec dst = newXMM();
  // Get sources
  x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
  x86::Vec src2 = LoadValueXmm(b, instr->src2.value);

  switch (instr->dest->type) {
  case HIR::FLOAT32_TYPE:
    COMP->vmaxss(dst, src1, src2);
    break;
  case HIR::FLOAT64_TYPE:
    COMP->vmaxsd(dst, src1, src2);
    break;
  case HIR::VEC128_TYPE:
    COMP->vmaxps(dst, src1, src2);
    break;
  default: UNREACHABLE_MSG("Unimplemented OPCODE_MAX type."); return;
  }

  // Store result
  TagStoreReg(instr->dest, dst);
}

//
// Min
//

REGISTER_EMITTER(OPCODE_MIN, Emit_MIN)
  static void Emit_MIN(x86CodeGenBackend *b, const HIR::Instr *instr) {
  switch (instr->dest->type) {
  case HIR::INT8_TYPE: {
    // Destination
    x86::Gp dst = newGP8();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // CMOV doesn't support 8-bit, widen to 32-bit
    x86::Gp src1_32 = newGP32();
    x86::Gp src2_32 = newGP32();
    COMP->movsx(src1_32, src1.r8());
    COMP->movsx(src2_32, src2.r8());
    COMP->cmp(src1_32, src2_32);
    COMP->cmovg(src1_32, src2_32);
    COMP->mov(dst, src1_32.r8());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::FLOAT64_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    // Operation
    COMP->vminsd(dst, src1, src2);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::VEC128_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    x86::Vec xmm2 = newXMM();
    x86::Vec xmm3 = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    // Operation
    COMP->vminps(xmm2, src1, src2);
    COMP->vminps(xmm3, src2, src1);
    COMP->vorps(dst, xmm2, xmm3);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented OPCODE_MIN type."); return;
  }
}

//
// Select
//

REGISTER_EMITTER(OPCODE_SELECT, Emit_SELECT)
  static void Emit_SELECT(x86CodeGenBackend *b, const HIR::Instr *instr) {
  switch (instr->dest->type) {
  case HIR::INT8_TYPE: {
    // Destination
    x86::Gp dst = newGP8();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    x86::Gp src3 = LoadValueGp(b, instr->src3.value);
    // Operation
    COMP->test(src1, src1);
    COMP->cmovnz(dst.r32(), src2.r32());
    COMP->cmovz(dst.r32(), src3.r32());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT16_TYPE: {
    // Destination
    x86::Gp dst = newGP16();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    x86::Gp src3 = LoadValueGp(b, instr->src3.value);
    // Operation
    COMP->test(src1, src1);
    COMP->cmovnz(dst.r32(), src2.r32());
    COMP->cmovz(dst.r32(), src3.r32());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT32_TYPE: {
    // Destination
    x86::Gp dst = newGP32();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    x86::Gp src3 = LoadValueGp(b, instr->src3.value);
    // Operation
    COMP->test(src1, src1);
    COMP->cmovnz(dst, src2);
    COMP->cmovz(dst, src3);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT64_TYPE: {
    // Destination
    x86::Gp dst = newGP64();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    x86::Gp src3 = LoadValueGp(b, instr->src3.value);
    // Operation
    COMP->test(src1, src1);
    COMP->cmovnz(dst, src2);
    COMP->cmovz(dst, src3);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::FLOAT32_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    x86::Vec src3 = LoadValueXmm(b, instr->src3.value);
    // Operation
    x86::Gp tmp = newGP32();
    x86::Vec tmpXmm1 = newXMM();
    x86::Vec tmpXmm0 = newXMM();
    COMP->movzx(tmp, src1);
    COMP->vmovd(tmpXmm1, tmp);
    COMP->vxorps(tmpXmm0, tmpXmm0, tmpXmm0);
    COMP->vpcmpeqd(tmpXmm0, tmpXmm0, tmpXmm1);
    COMP->vpandn(tmpXmm1, tmpXmm0, src2);
    COMP->vpand(dst, tmpXmm0, src3);
    COMP->vpor(dst, dst, tmpXmm1);

    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::FLOAT64_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    x86::Vec src3 = LoadValueXmm(b, instr->src3.value);
    // Operation
    x86::Gp tmp = newGP32();
    x86::Vec tmpXmm1 = newXMM();
    x86::Vec tmpXmm0 = newXMM();
    COMP->movzx(tmp, src1);
    COMP->vmovd(tmpXmm1, tmp);
    COMP->vpxor(tmpXmm0, tmpXmm0, tmpXmm0);
    COMP->vpcmpeqq(tmpXmm0, tmpXmm0, tmpXmm1);
    COMP->vpandn(tmpXmm1, tmpXmm0, src2);
    COMP->vpand(dst, tmpXmm0, src3);
    COMP->vpor(dst, dst, tmpXmm1);

    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::VEC128_TYPE: {
    // Destination
    x86::Vec dst = newXMM();

    if (instr->src1.value->type == HIR::INT8_TYPE) {
      // Get sources
      x86::Gp src1 = LoadValueGp(b, instr->src1.value);
      x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
      x86::Vec src3 = LoadValueXmm(b, instr->src3.value);
      // Operation
      x86::Gp tmp = newGP32();
      x86::Vec tmpXmm1 = newXMM();
      x86::Vec tmpXmm0 = newXMM();
      COMP->movzx(tmp, src1);
      COMP->vmovd(tmpXmm1, tmp);
      COMP->vpbroadcastd(tmpXmm1, tmpXmm1);
      COMP->vxorps(tmpXmm0, tmpXmm0, tmpXmm0);
      COMP->vpcmpeqd(tmpXmm0, tmpXmm0, tmpXmm1);
      COMP->vpandn(tmpXmm1, tmpXmm0, src2);
      COMP->vpand(dst, tmpXmm0, src3);
      COMP->vpor(dst, dst, tmpXmm1);
    } else if (instr->src1.value->type == HIR::VEC128_TYPE) {
      // Get sources
      x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
      x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
      x86::Vec src3 = LoadValueXmm(b, instr->src3.value);
      // Operation
      x86::Vec tmpXmm0 = newXMM();
      COMP->vpandn(tmpXmm0, src1, src2);
      COMP->vpand(dst, src1, src3);
      COMP->vpor(dst, dst, tmpXmm0);
    }

    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented OPCODE_SELECT type."); return;
  }
}

//
// Is True
//

REGISTER_EMITTER(OPCODE_IS_TRUE, Emit_IS_TRUE)
  static void Emit_IS_TRUE(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Gp dst = newGP8();

  switch (instr->src1.value->type) {
  case HIR::INT8_TYPE: {
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    COMP->test(src1, src1);
  } break;
  case HIR::INT16_TYPE: {
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    COMP->test(src1, src1);
  } break;
  case HIR::INT32_TYPE: {
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    COMP->test(src1, src1);
  } break;
  case HIR::INT64_TYPE: {
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    COMP->test(src1, src1);
  } break;
  case HIR::FLOAT32_TYPE: {
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    COMP->vptest(src1, src1);
  } break;
  case HIR::FLOAT64_TYPE: {
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    COMP->vptest(src1, src1);
  } break;
  case HIR::VEC128_TYPE: {
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    COMP->vptest(src1, src1);
  } break;
  default: UNREACHABLE_MSG("Unimplemented IS_TRUE type."); return;
  }

  // Set depending on test result
  COMP->setnz(dst);
  // Store result
  TagStoreReg(instr->dest, dst);
}

//
// Is False
//

REGISTER_EMITTER(OPCODE_IS_FALSE, Emit_IS_FALSE)
  static void Emit_IS_FALSE(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Gp dst = newGP8();

  switch (instr->src1.value->type) {
  case HIR::INT8_TYPE: {
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    COMP->test(src1, src1);
  } break;
  case HIR::INT16_TYPE: {
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    COMP->test(src1, src1);
  } break;
  case HIR::INT32_TYPE: {
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    COMP->test(src1, src1);
  } break;
  case HIR::INT64_TYPE: {
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    COMP->test(src1, src1);
  } break;
  case HIR::FLOAT32_TYPE: {
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    COMP->vptest(src1, src1);
  } break;
  case HIR::FLOAT64_TYPE: {
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    COMP->vptest(src1, src1);
  } break;
  case HIR::VEC128_TYPE: {
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    COMP->vptest(src1, src1);
  } break;
  default: UNREACHABLE_MSG("Unimplemented IS_FALSE type."); return;
  }

  // Set depending on test result
  COMP->setz(dst);
  // Store result
  TagStoreReg(instr->dest, dst);
}

//
// Is NaN
//

REGISTER_EMITTER(OPCODE_IS_NAN, Emit_IS_NAN)
  static void Emit_IS_NAN(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Gp dst = newGP8();
  // Get sources
  x86::Vec src1 = LoadValueXmm(b, instr->src1.value);

  switch (instr->src1.value->type) {
  case HIR::FLOAT32_TYPE: {
    COMP->vucomiss(src1, src1);
  } break;
  case HIR::FLOAT64_TYPE: {
    COMP->vucomisd(src1, src1);
  } break;
  default: UNREACHABLE_MSG("Unimplemented IS_NAN type."); return;
  }

  // Set depending on test result
  COMP->setp(dst);
  // Store result
  TagStoreReg(instr->dest, dst);
}

//
// Compare EQ
//

REGISTER_EMITTER(OPCODE_COMPARE_EQ, Emit_COMPARE_EQ)
  static void Emit_COMPARE_EQ(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Gp dst = newGP8();

  switch (instr->src1.value->type) {
  case HIR::INT8_TYPE: {
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    COMP->cmp(src1, src2);
  } break;
  case HIR::INT16_TYPE: {
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    COMP->cmp(src1, src2);
  } break;
  case HIR::INT32_TYPE: {
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    COMP->cmp(src1, src2);
  } break;
  case HIR::INT64_TYPE: {
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    COMP->cmp(src1, src2);
  } break;
  case HIR::FLOAT32_TYPE: {
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    COMP->vcomiss(src1, src2);
  } break;
  case HIR::FLOAT64_TYPE: {
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    COMP->vcomisd(src1, src2);
  } break;
  default: UNREACHABLE_MSG("Unimplemented COMPARE_EQ type."); return;
  }

  // Set depending on test result
  COMP->sete(dst);
  // Store result
  TagStoreReg(instr->dest, dst);
}

//
// Compare NE
//

REGISTER_EMITTER(OPCODE_COMPARE_NE, Emit_COMPARE_NE)
  static void Emit_COMPARE_NE(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Gp dst = newGP8();

  switch (instr->src1.value->type) {
  case HIR::INT8_TYPE: {
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    COMP->cmp(src1, src2);
  } break;
  case HIR::INT16_TYPE: {
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    COMP->cmp(src1, src2);
  } break;
  case HIR::INT32_TYPE: {
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    COMP->cmp(src1, src2);
  } break;
  case HIR::INT64_TYPE: {
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    COMP->cmp(src1, src2);
  } break;
  case HIR::FLOAT32_TYPE: {
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    COMP->vcomiss(src1, src2);
  } break;
  case HIR::FLOAT64_TYPE: {
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    COMP->vcomisd(src1, src2);
  } break;
  default: UNREACHABLE_MSG("Unimplemented COMPARE_NE type."); return;
  }

  // Set depending on test result
  COMP->setne(dst);
  // Store result
  TagStoreReg(instr->dest, dst);
}

//
// Compare XX (Signed)
//
// Note: For floating-point comparisons, vcomisd/vcomiss set flags differently:
//   - CF=1 if src1 < src2 OR unordered (NaN)
//   - ZF=1 if src1 == src2
//   - PF=1 if unordered (NaN)
// So we use: setb (CF=1) for LT, seta (CF=0 && ZF=0) for GT, etc.
// For integer comparisons, we use the standard signed condition codes.

// COMPARE_SLT: src1 < src2 (signed)
REGISTER_EMITTER(OPCODE_COMPARE_SLT, Emit_COMPARE_SLT)
static void Emit_COMPARE_SLT(x86CodeGenBackend *b, const HIR::Instr *instr) {
  x86::Gp dst = newGP8();

  switch (instr->src1.value->type) {
  case HIR::INT8_TYPE:
  case HIR::INT16_TYPE:
  case HIR::INT32_TYPE:
  case HIR::INT64_TYPE: {
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    switch (instr->src1.value->type) {
    case HIR::INT8_TYPE:  COMP->cmp(src1.r8(),  src2.r8());  break;
    case HIR::INT16_TYPE: COMP->cmp(src1.r16(), src2.r16()); break;
    case HIR::INT32_TYPE: COMP->cmp(src1.r32(), src2.r32()); break;
    case HIR::INT64_TYPE: COMP->cmp(src1.r64(), src2.r64()); break;
    default: break;
    }
    COMP->setl(dst);  // SF != OF
  } break;
  case HIR::FLOAT32_TYPE: {
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    COMP->vcomiss(src1, src2);
    COMP->setb(dst);  // CF=1 (src1 < src2, but also set for unordered - caller handles NaN separately)
  } break;
  case HIR::FLOAT64_TYPE: {
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    COMP->vcomisd(src1, src2);
    COMP->setb(dst);  // CF=1
  } break;
  default: UNREACHABLE_MSG("Unimplemented COMPARE_SLT type."); return;
  }
  TagStoreReg(instr->dest, dst);
}

// COMPARE_SLE: src1 <= src2 (signed)
REGISTER_EMITTER(OPCODE_COMPARE_SLE, Emit_COMPARE_SLE)
static void Emit_COMPARE_SLE(x86CodeGenBackend *b, const HIR::Instr *instr) {
  x86::Gp dst = newGP8();

  switch (instr->src1.value->type) {
  case HIR::INT8_TYPE:
  case HIR::INT16_TYPE:
  case HIR::INT32_TYPE:
  case HIR::INT64_TYPE: {
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    switch (instr->src1.value->type) {
    case HIR::INT8_TYPE:  COMP->cmp(src1.r8(),  src2.r8());  break;
    case HIR::INT16_TYPE: COMP->cmp(src1.r16(), src2.r16()); break;
    case HIR::INT32_TYPE: COMP->cmp(src1.r32(), src2.r32()); break;
    case HIR::INT64_TYPE: COMP->cmp(src1.r64(), src2.r64()); break;
    default: break;
    }
    COMP->setle(dst);  // ZF=1 OR SF != OF
  } break;
  case HIR::FLOAT32_TYPE: {
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    COMP->vcomiss(src1, src2);
    COMP->setbe(dst);  // CF=1 OR ZF=1
  } break;
  case HIR::FLOAT64_TYPE: {
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    COMP->vcomisd(src1, src2);
    COMP->setbe(dst);  // CF=1 OR ZF=1
  } break;
  default: UNREACHABLE_MSG("Unimplemented COMPARE_SLE type."); return;
  }
  TagStoreReg(instr->dest, dst);
}

// COMPARE_SGT: src1 > src2 (signed)
REGISTER_EMITTER(OPCODE_COMPARE_SGT, Emit_COMPARE_SGT)
static void Emit_COMPARE_SGT(x86CodeGenBackend *b, const HIR::Instr *instr) {
  x86::Gp dst = newGP8();

  switch (instr->src1.value->type) {
  case HIR::INT8_TYPE:
  case HIR::INT16_TYPE:
  case HIR::INT32_TYPE:
  case HIR::INT64_TYPE: {
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    switch (instr->src1.value->type) {
    case HIR::INT8_TYPE:  COMP->cmp(src1.r8(),  src2.r8());  break;
    case HIR::INT16_TYPE: COMP->cmp(src1.r16(), src2.r16()); break;
    case HIR::INT32_TYPE: COMP->cmp(src1.r32(), src2.r32()); break;
    case HIR::INT64_TYPE: COMP->cmp(src1.r64(), src2.r64()); break;
    default: break;
    }
    COMP->setg(dst);  // ZF=0 AND SF == OF
  } break;
  case HIR::FLOAT32_TYPE: {
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    COMP->vcomiss(src1, src2);
    COMP->seta(dst);  // CF=0 AND ZF=0
  } break;
  case HIR::FLOAT64_TYPE: {
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    COMP->vcomisd(src1, src2);
    COMP->seta(dst);  // CF=0 AND ZF=0
  } break;
  default: UNREACHABLE_MSG("Unimplemented COMPARE_SGT type."); return;
  }
  TagStoreReg(instr->dest, dst);
}

// COMPARE_SGE: src1 >= src2 (signed)
REGISTER_EMITTER(OPCODE_COMPARE_SGE, Emit_COMPARE_SGE)
static void Emit_COMPARE_SGE(x86CodeGenBackend *b, const HIR::Instr *instr) {
  x86::Gp dst = newGP8();

  switch (instr->src1.value->type) {
  case HIR::INT8_TYPE:
  case HIR::INT16_TYPE:
  case HIR::INT32_TYPE:
  case HIR::INT64_TYPE: {
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    switch (instr->src1.value->type) {
    case HIR::INT8_TYPE:  COMP->cmp(src1.r8(),  src2.r8());  break;
    case HIR::INT16_TYPE: COMP->cmp(src1.r16(), src2.r16()); break;
    case HIR::INT32_TYPE: COMP->cmp(src1.r32(), src2.r32()); break;
    case HIR::INT64_TYPE: COMP->cmp(src1.r64(), src2.r64()); break;
    default: break;
    }
    COMP->setge(dst);  // SF == OF
  } break;
  case HIR::FLOAT32_TYPE: {
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    COMP->vcomiss(src1, src2);
    COMP->setae(dst);  // CF=0
  } break;
  case HIR::FLOAT64_TYPE: {
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    COMP->vcomisd(src1, src2);
    COMP->setae(dst);  // CF=0
  } break;
  default: UNREACHABLE_MSG("Unimplemented COMPARE_SGE type."); return;
  }
  TagStoreReg(instr->dest, dst);
}

// Integer unsigned comparisons (no FP support needed)
#define EMIT_COMPARE_UNSIGNED_INT(opname, setcc_fn)                               \
  REGISTER_EMITTER(OPCODE_##opname, Emit_##opname)                                \
  static void Emit_##opname(x86CodeGenBackend *b, const HIR::Instr *instr) {      \
    x86::Gp dst = newGP8();                                                       \
                                                                                  \
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);                             \
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);                             \
                                                                                  \
    switch (instr->src1.value->type) {                                            \
    case HIR::INT8_TYPE:  COMP->cmp(src1.r8(),  src2.r8());  break;               \
    case HIR::INT16_TYPE: COMP->cmp(src1.r16(), src2.r16()); break;               \
    case HIR::INT32_TYPE: COMP->cmp(src1.r32(), src2.r32()); break;               \
    case HIR::INT64_TYPE: COMP->cmp(src1.r64(), src2.r64()); break;               \
    default: UNREACHABLE_MSG("Unimplemented COMPARE_XX type."); return;           \
    }                                                                             \
    COMP->setcc_fn(dst);                                                          \
    TagStoreReg(instr->dest, dst);                                                \
  }

 EMIT_COMPARE_UNSIGNED_INT(COMPARE_ULT, setb)
 EMIT_COMPARE_UNSIGNED_INT(COMPARE_ULE, setbe)
 EMIT_COMPARE_UNSIGNED_INT(COMPARE_UGT, seta)
 EMIT_COMPARE_UNSIGNED_INT(COMPARE_UGE, setae)

#undef EMIT_COMPARE_UNSIGNED_INT


//
// Did Saturate
//
REGISTER_EMITTER(OPCODE_DID_SATURATE, Emit_DID_SATURATE)
static void Emit_DID_SATURATE(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // TODO(bitsh1ft3r): Implement actual saturation check!
  // Destination
  x86::Gp dst = newGP8();
  // Operation (Clear for now, no saturation)
  COMP->xor_(dst.r32(), dst.r32());
  // Store result
  TagStoreReg(instr->dest, dst);
}


//
// Add
//

REGISTER_EMITTER(OPCODE_ADD, Emit_ADD)
  static void Emit_ADD(x86CodeGenBackend *b, const HIR::Instr *instr) {
  switch (instr->src1.value->type) {
  case HIR::INT8_TYPE: {
    // Destination
    x86::Gp dst = newGP8();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r8());
    COMP->add(dst, src2.r8());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT16_TYPE: {
    // Destination
    x86::Gp dst = newGP16();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r16());
    COMP->add(dst, src2.r16());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT32_TYPE: {
    // Destination
    x86::Gp dst = newGP32();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r32());
    COMP->add(dst, src2.r32());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT64_TYPE: {
    // Destination
    x86::Gp dst = newGP64();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r64());
    COMP->add(dst, src2.r64());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::FLOAT32_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    COMP->vaddss(dst, src1, src2);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::FLOAT64_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    COMP->vaddsd(dst, src1, src2);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::VEC128_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    COMP->vaddps(dst, src1, src2);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented ADD type."); return;
  }
}

//
// Add with Carry
//

REGISTER_EMITTER(OPCODE_ADD_CARRY, Emit_ADD_CARRY)
static void Emit_ADD_CARRY(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Gp dst;
  // Get sources
  x86::Gp src1 = LoadValueGp(b, instr->src1.value);
  x86::Gp src2 = LoadValueGp(b, instr->src2.value);
  x86::Gp carry = LoadValueGp(b, instr->src3.value);


  switch (instr->dest->type) {
  case HIR::INT8_TYPE:
    dst = newGP8();
    COMP->mov(dst, src1.r8());
    COMP->add(dst, src2.r8());
    COMP->add(dst, carry.r8());
    break;
  case HIR::INT16_TYPE: {
    dst = newGP16();
    x86::Gp cExt = newGP16();
    COMP->mov(dst, src1.r16());
    COMP->add(dst, src2.r16());
    COMP->movzx(cExt, carry.r8());
    COMP->add(dst, cExt);
  } break;
  case HIR::INT32_TYPE: {
    dst = newGP32();
    x86::Gp cExt = newGP32();
    COMP->mov(dst, src1.r32());
    COMP->add(dst, src2.r32());
    COMP->movzx(cExt, carry.r8());
    COMP->add(dst, cExt);
  } break;
  case HIR::INT64_TYPE: {
    dst = newGP64();
    x86::Gp cExt = newGP64();
    COMP->movzx(cExt.r32(), carry.r8());
    COMP->mov(dst, src1.r64());
    COMP->add(dst, src2.r64());
    COMP->add(dst, cExt);
  } break;
  default: UNREACHABLE_MSG("Unimplemented ADD_CARRY type."); return;
  }
  // Store result
  TagStoreReg(instr->dest, dst);
}

//
// Subtract
//

REGISTER_EMITTER(OPCODE_SUB, Emit_SUB)
static void Emit_SUB(x86CodeGenBackend *b, const HIR::Instr *instr) {
  switch (instr->src1.value->type) {
  case HIR::INT8_TYPE: {
    // Destination
    x86::Gp dst = newGP8();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r8());
    COMP->sub(dst, src2.r8());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT16_TYPE: {
    // Destination
    x86::Gp dst = newGP16();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r16());
    COMP->sub(dst, src2.r16());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT32_TYPE: {
    // Destination
    x86::Gp dst = newGP32();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r32());
    COMP->sub(dst, src2.r32());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT64_TYPE: {
    // Destination
    x86::Gp dst = newGP64();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r64());
    COMP->sub(dst, src2.r64());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::FLOAT32_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    COMP->vsubss(dst, src1, src2);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::FLOAT64_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    COMP->vsubsd(dst, src1, src2);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::VEC128_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    COMP->vsubps(dst, src1, src2);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented SUB type."); return;
  }
}

//
// Multiply
//

REGISTER_EMITTER(OPCODE_MUL, Emit_MUL)
static void Emit_MUL(x86CodeGenBackend *b, const HIR::Instr *instr) {
  switch (instr->src1.value->type) {
  case HIR::INT8_TYPE: {
    // Destination
    x86::Gp dst = newGP16();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r8());
    COMP->imul(dst, src2.r8());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT16_TYPE: {
    // Destination
    x86::Gp dst = newGP16();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r16());
    COMP->imul(dst, src2.r16());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT32_TYPE: {
    // Destination
    x86::Gp dst = newGP32();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r32());
    COMP->imul(dst, src2.r32());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT64_TYPE: {
    // Destination
    x86::Gp dst = newGP64();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r64());
    COMP->imul(dst, src2.r64());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::FLOAT32_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    COMP->vmulss(dst, src1, src2);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::FLOAT64_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    COMP->vmulsd(dst, src1, src2);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::VEC128_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    COMP->vmulps(dst, src1, src2);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented MUL type."); return;
  }
}

//
// Multiply High
//

REGISTER_EMITTER(OPCODE_MUL_HI, Emit_MUL_HI)
static void Emit_MUL_HI(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Get sources
  x86::Gp src1 = LoadValueGp(b, instr->src1.value);
  x86::Gp src2 = LoadValueGp(b, instr->src2.value);

  switch (instr->dest->type) {
  case HIR::INT32_TYPE: {
    // Widen 32->64, multiply, shift right 32 to get high half.
    x86::Gp lhs64 = newGP64();
    x86::Gp rhs64 = newGP64();
    if (instr->flags & HIR::ARITHMETIC_UNSIGNED) {
      COMP->mov(lhs64.r32(), src1.r32()); // zero-extend
      COMP->mov(rhs64.r32(), src2.r32());
    } else {
      COMP->movsxd(lhs64, src1.r32()); // sign-extend
      COMP->movsxd(rhs64, src2.r32());
    }
    COMP->imul(lhs64, rhs64);
    COMP->shr(lhs64, asmjit::Imm(32));
    x86::Gp dst = newGP32();
    COMP->mov(dst, lhs64.r32());
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT64_TYPE: {
    // 64-bit mul_hi requires 1-operand mul/imul with implicit rdx:rax.
    x86::Gp lo = newGP64();
    x86::Gp hi = newGP64();
    COMP->mov(lo, src1.r64());
    if (instr->flags & HIR::ARITHMETIC_UNSIGNED)
      COMP->mul(hi, lo, src2.r64());
    else
      COMP->imul(hi, lo, src2.r64());
    x86::Gp dst = newGP64();
    COMP->mov(dst, hi);
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented MUL_HI type."); return;
  }
}

//
// Divide
//

REGISTER_EMITTER(OPCODE_DIV, Emit_DIV)
static void Emit_DIV(x86CodeGenBackend *b, const HIR::Instr *instr) {
  switch (instr->dest->type) {
  case HIR::INT32_TYPE: {
    // Destination
    x86::Gp dst = newGP32();

    // Zero out dest
    COMP->xor_(dst, dst);

    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);

    // Skip label
    Label skip = newLabel();

    x86::Gp eax = newGP32();
    x86::Gp edx = newGP32();

    COMP->mov(eax, src1.r32());
    // Test src2, skip if it's zero.
    COMP->test(src2.r32(), src2.r32());
    COMP->jz(skip);

    if (instr->flags & HIR::ARITHMETIC_UNSIGNED) {
      COMP->xor_(edx, edx);
      COMP->div(edx, eax, src2.r32());
    } else {
      COMP->cdq(edx, eax);
      COMP->idiv(edx, eax, src2.r32());
    }

    COMP->mov(dst, eax);

    COMP->bind(skip);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT64_TYPE: {
    // Destination
    x86::Gp dst = newGP64();

    // Zero out dest
    COMP->xor_(dst, dst);

    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);

    // Skip label
    Label skip = newLabel();

    // Test src2, skip if it's zero.
    COMP->test(src2, src2);
    COMP->jz(skip);

    x86::Gp rax = newGP64();
    x86::Gp rdx = newGP64();

    COMP->mov(rax, src1.r64());

    if (instr->flags & HIR::ARITHMETIC_UNSIGNED) {
      COMP->xor_(rdx, rdx);
      COMP->div(rdx, rax, src2.r64());
    } else {
      COMP->cqo(rdx, rax);
      COMP->idiv(rdx, rax, src2.r64());
    }

    COMP->mov(dst, rax);

    COMP->bind(skip);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::FLOAT32_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);

    COMP->vdivss(dst, src1, src2);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::FLOAT64_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);

    COMP->vdivsd(dst, src1, src2);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::VEC128_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);

    COMP->vdivps(dst, src1, src2);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented DIV type."); return;
  }
}

//
// Multiply Add
//

REGISTER_EMITTER(OPCODE_MUL_ADD, Emit_MUL_ADD)
  static void Emit_MUL_ADD(x86CodeGenBackend *b, const HIR::Instr *instr) {
  switch (instr->dest->type) {
  case HIR::FLOAT32_TYPE: {
    // Get sources
    x86::Vec s1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec s2 = LoadValueXmm(b, instr->src2.value);
    x86::Vec s3 = LoadValueXmm(b, instr->src3.value);
    // Operation
    COMP->vfmadd213ss(s1, s2, s3);
    // Store result
    TagStoreReg(instr->dest, s1);
  } break;
  case HIR::FLOAT64_TYPE: {
    // Get sources
    x86::Vec s1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec s2 = LoadValueXmm(b, instr->src2.value);
    x86::Vec s3 = LoadValueXmm(b, instr->src3.value);
    // Operation
    COMP->vfmadd213sd(s1, s2, s3);
    // Store result
    TagStoreReg(instr->dest, s1);
  } break;
  case HIR::VEC128_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    x86::Vec src3 = LoadValueXmm(b, instr->src3.value);
    // Operation
    COMP->vmulps(dst, src1, src2);
    COMP->vaddps(dst, dst, src3);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented MUL_ADD type."); return;
  }
}

//
// Multiply Subtract
//

REGISTER_EMITTER(OPCODE_MUL_SUB, Emit_MUL_SUB)
  static void Emit_MUL_SUB(x86CodeGenBackend *b, const HIR::Instr *instr) {
  switch (instr->dest->type) {
  case HIR::FLOAT32_TYPE: {
    // Get sources
    x86::Vec s1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec s2 = LoadValueXmm(b, instr->src2.value);
    x86::Vec s3 = LoadValueXmm(b, instr->src3.value);
    // Operation
    COMP->vfmsub213ss(s1, s2, s3);
    // Store result
    TagStoreReg(instr->dest, s1);
  } break;
  case HIR::FLOAT64_TYPE: {
    // Get sources
    x86::Vec s1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec s2 = LoadValueXmm(b, instr->src2.value);
    x86::Vec s3 = LoadValueXmm(b, instr->src3.value);
    // Operation
    COMP->vfmsub213sd(s1, s2, s3);
    // Store result
    TagStoreReg(instr->dest, s1);
  } break;
  case HIR::VEC128_TYPE: {
    // Get sources
    x86::Vec s1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec s2 = LoadValueXmm(b, instr->src2.value);
    x86::Vec s3 = LoadValueXmm(b, instr->src3.value);
    // Operation
    COMP->vfmsub213ps(s1, s2, s3);
    // Store result
    TagStoreReg(instr->dest, s1);
  } break;
  default: UNREACHABLE_MSG("Unimplemented MUL_SUB type."); return;
  }
}

//
// Negation
//

REGISTER_EMITTER(OPCODE_NEG, Emit_NEG)
static void Emit_NEG(x86CodeGenBackend *b, const HIR::Instr *instr) {
  switch (instr->dest->type) {
  case HIR::INT8_TYPE: {
    // Destination
    x86::Gp dst = newGP8();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    // Operation
    COMP->mov(dst, src1.r8());
    COMP->neg(dst);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT16_TYPE: {
    // Destination
    x86::Gp dst = newGP16();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    // Operation
    COMP->mov(dst, src1.r16());
    COMP->neg(dst);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT32_TYPE: {
    // Destination
    x86::Gp dst = newGP32();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    // Operation
    COMP->mov(dst, src1.r32());
    COMP->neg(dst);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT64_TYPE: {
    // Destination
    x86::Gp dst = newGP64();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    // Operation
    COMP->mov(dst, src1.r64());
    COMP->neg(dst);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::FLOAT32_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    // Operation
    COMP->vxorps(dst, src1, LoadXmmConst(b, XMMSignMaskPS));
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::FLOAT64_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    // Operation
    COMP->vxorpd(dst, src1, LoadXmmConst(b, XMMSignMaskPD));
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::VEC128_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    // Operation
    COMP->vxorps(dst, src1, LoadXmmConst(b, XMMSignMaskPS));
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented NEG type."); return;
  }
}

//
// Absolute
//

REGISTER_EMITTER(OPCODE_ABS, Emit_ABS)
  static void Emit_ABS(x86CodeGenBackend *b, const HIR::Instr *instr) {
  switch (instr->dest->type) {
  case HIR::FLOAT32_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    // Operation
    COMP->vpand(dst, src1, LoadXmmConst(b, XMMAbsMaskPS));
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::FLOAT64_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    // Operation
    COMP->vpand(dst, src1, LoadXmmConst(b, XMMAbsMaskPD));
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::VEC128_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    // Operation
    COMP->vpand(dst, src1, LoadXmmConst(b, XMMAbsMaskPS));
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented ABS type."); return;
  }
}

//
// Square Root
//

REGISTER_EMITTER(OPCODE_SQRT, Emit_SQRT)
  static void Emit_SQRT(x86CodeGenBackend *b, const HIR::Instr *instr) {
  switch (instr->dest->type) {
  case HIR::FLOAT32_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    // Operation
    COMP->vsqrtss(dst, dst, src1);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::FLOAT64_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    // Operation
    COMP->vsqrtsd(dst, dst, src1);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::VEC128_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    // Operation
    COMP->vsqrtps(dst, src1);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented SQRT type."); return;
  }
}

//
// Reciprocal Square Root
//

// Note:
// Altivec guarantees an error of < 1/4096 for vrsqrtefp while AVX only gives
// < 1.5*2^-12 ≈ 1/2730 for vrsqrtps.

REGISTER_EMITTER(OPCODE_RSQRT, Emit_RSQRT)
  static void Emit_RSQRT(x86CodeGenBackend *b, const HIR::Instr *instr) {
  switch (instr->dest->type) {
  case HIR::FLOAT32_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Temps
    x86::Vec xmm0 = newXMM();
    x86::Vec xmm1 = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    // Operation
    COMP->vmovaps(xmm0, LoadXmmConst(b, XMMOne));
    COMP->vsqrtss(xmm1, src1, src1);
    COMP->vdivss(dst, xmm0, xmm1);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::FLOAT64_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Temps
    x86::Vec xmm0 = newXMM();
    x86::Vec xmm1 = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    // Operation
    COMP->vmovapd(xmm0, LoadXmmConst(b, XMMOnePD));
    COMP->vsqrtsd(xmm1, src1, src1);
    COMP->vdivsd(dst, xmm0, xmm1);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::VEC128_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Temps
    x86::Vec xmm0 = newXMM();
    x86::Vec xmm1 = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    // Operation
    COMP->vmovaps(xmm0, LoadXmmConst(b, XMMOne));
    COMP->vsqrtps(xmm1, src1);
    COMP->vdivps(dst, xmm0, xmm1);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented RSQRT type."); return;
  }
}

//
// Reciprocal
//

// Note:
// Altivec guarantees an error of < 1/4096 for vrefp while AVX only gives
// < 1.5*2^-12 ≈ 1/2730 for rcpps. This breaks camp, horse and random event
// spawning, breaks cactus collision as well as flickering grass in 5454082B

REGISTER_EMITTER(OPCODE_RECIP, Emit_RECIP)
  static void Emit_RECIP(x86CodeGenBackend *b, const HIR::Instr *instr) {
  switch (instr->dest->type) {
  case HIR::FLOAT32_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Temps
    x86::Vec xmm0 = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    // Operation
    COMP->vmovaps(xmm0, LoadXmmConst(b, XMMOne));
    COMP->vdivss(dst, xmm0, src1);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::FLOAT64_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Temps
    x86::Vec xmm0 = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    // Operation
    COMP->vmovapd(xmm0, LoadXmmConst(b, XMMOnePD));
    COMP->vdivsd(dst, xmm0, src1);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::VEC128_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Temps
    x86::Vec xmm0 = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    // Operation
    COMP->vmovaps(xmm0, LoadXmmConst(b, XMMOne));
    COMP->vdivps(dst, xmm0, src1);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented RECIP type."); return;
  }
}

//
// Pow2
//

REGISTER_EMITTER(OPCODE_POW2, Emit_POW2)
  static void Emit_POW2(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Build the signature manually: __m128i(void*, __m128i, __m128i)
  // asmjit has no TypeIdOfT for __m128i, so use explicit TypeId values.
  asmjit::FuncSignature sig(
    asmjit::CallConvId::kCDecl,
    asmjit::FuncSignature::kNoVarArgs,
    asmjit::TypeId::kInt32x4,   // return: __m128i
    asmjit::TypeId::kUIntPtr,   // arg0:   void*
    asmjit::TypeId::kInt32x4);   // arg1:   __m128i

  switch (instr->dest->type) {
  case HIR::FLOAT32_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    // Operation
    InvokeNode *invokeNode = nullptr;
    Xe::JITCompat::Invoke(COMP, invokeNode, asmjit::Imm(reinterpret_cast<uintptr_t>(EmulatePow2Float)), sig);
    Xe::JITCompat::SetArg(invokeNode, 0, Imm(0)); // void* ctx = nullptr
    Xe::JITCompat::SetArg(invokeNode, 1, src1);
    Xe::JITCompat::SetRet(invokeNode, 0, dst);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::FLOAT64_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    // Operation
    InvokeNode *invokeNode = nullptr;
    Xe::JITCompat::Invoke(COMP, invokeNode, asmjit::Imm(reinterpret_cast<uintptr_t>(EmulatePow2Double)), sig);
    Xe::JITCompat::SetArg(invokeNode, 0, Imm(0)); // void* ctx = nullptr
    Xe::JITCompat::SetArg(invokeNode, 1, src1);
    Xe::JITCompat::SetRet(invokeNode, 0, dst);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::VEC128_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    // Operation
    InvokeNode *invokeNode = nullptr;
    Xe::JITCompat::Invoke(COMP, invokeNode, asmjit::Imm(reinterpret_cast<uintptr_t>(EmulatePow2Vec)), sig);
    Xe::JITCompat::SetArg(invokeNode, 0, Imm(0)); // void* ctx = nullptr
    Xe::JITCompat::SetArg(invokeNode, 1, src1);
    Xe::JITCompat::SetRet(invokeNode, 0, dst);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented RECIP type."); return;
  }
}

//
// Log2
//

REGISTER_EMITTER(OPCODE_LOG2, Emit_LOG2)
  static void Emit_LOG2(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Build the signature manually: __m128i(void*, __m128i, __m128i)
  // asmjit has no TypeIdOfT for __m128i, so use explicit TypeId values.
  asmjit::FuncSignature sig(
    asmjit::CallConvId::kCDecl,
    asmjit::FuncSignature::kNoVarArgs,
    asmjit::TypeId::kInt32x4,   // return: __m128i
    asmjit::TypeId::kUIntPtr,   // arg0:   void*
    asmjit::TypeId::kInt32x4);   // arg1:   __m128i

  switch (instr->dest->type) {
  case HIR::FLOAT32_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    // Operation
    InvokeNode *invokeNode = nullptr;
    Xe::JITCompat::Invoke(COMP, invokeNode, asmjit::Imm(reinterpret_cast<uintptr_t>(EmulateLog2Float)), sig);
    Xe::JITCompat::SetArg(invokeNode, 0, Imm(0)); // void* ctx = nullptr
    Xe::JITCompat::SetArg(invokeNode, 1, src1);
    Xe::JITCompat::SetRet(invokeNode, 0, dst);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::FLOAT64_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    // Operation
    InvokeNode *invokeNode = nullptr;
    Xe::JITCompat::Invoke(COMP, invokeNode, asmjit::Imm(reinterpret_cast<uintptr_t>(EmulateLog2Double)), sig);
    Xe::JITCompat::SetArg(invokeNode, 0, Imm(0)); // void* ctx = nullptr
    Xe::JITCompat::SetArg(invokeNode, 1, src1);
    Xe::JITCompat::SetRet(invokeNode, 0, dst);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::VEC128_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    // Operation
    InvokeNode *invokeNode = nullptr;
    Xe::JITCompat::Invoke(COMP, invokeNode, asmjit::Imm(reinterpret_cast<uintptr_t>(EmulateLog2Vec)), sig);
    Xe::JITCompat::SetArg(invokeNode, 0, Imm(0)); // void* ctx = nullptr
    Xe::JITCompat::SetArg(invokeNode, 1, src1);
    Xe::JITCompat::SetRet(invokeNode, 0, dst);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented RECIP type."); return;
  }
}

//
// And
//

REGISTER_EMITTER(OPCODE_AND, Emit_AND)
static void Emit_AND(x86CodeGenBackend *b, const HIR::Instr *instr) {
  switch (instr->dest->type) {
  case HIR::INT8_TYPE: {
    // Destination
    x86::Gp dst = newGP8();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r8());
    COMP->and_(dst, src2.r8());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT16_TYPE: {
    // Destination
    x86::Gp dst = newGP16();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r16());
    COMP->and_(dst, src2.r16());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT32_TYPE: {
    // Destination
    x86::Gp dst = newGP32();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r32());
    COMP->and_(dst, src2.r32());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT64_TYPE: {
    // Destination
    x86::Gp dst = newGP64();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r64());
    COMP->and_(dst, src2.r64());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::VEC128_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    // Operation
    COMP->vpand(dst, src1, src2);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented AND type."); return;
  }
}

//
// And Not
//

REGISTER_EMITTER(OPCODE_AND_NOT, Emit_AND_NOT)
  static void Emit_AND_NOT(x86CodeGenBackend *b, const HIR::Instr *instr) {
  switch (instr->dest->type) {
  case HIR::INT8_TYPE: {
    // Destination
    x86::Gp dst = newGP8();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src2.r8());
    COMP->not_(dst);
    COMP->and_(dst, src1.r8());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT16_TYPE: {
    // Destination
    x86::Gp dst = newGP16();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src2.r16());
    COMP->not_(dst);
    COMP->and_(dst, src1.r16());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT32_TYPE: {
    // Destination
    x86::Gp dst = newGP32();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src2.r32());
    COMP->not_(dst);
    COMP->and_(dst, src1.r32());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT64_TYPE: {
    // Destination
    x86::Gp dst = newGP64();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src2.r64());
    COMP->not_(dst);
    COMP->and_(dst, src1.r64());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::VEC128_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    // Operation
    COMP->vpandn(dst, src2, src1);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented AND_NOT type."); return;
  }
}

//
// OR
//

REGISTER_EMITTER(OPCODE_OR, Emit_OR)
static void Emit_OR(x86CodeGenBackend *b, const HIR::Instr *instr) {
  switch (instr->dest->type) {
  case HIR::INT8_TYPE: {
    // Destination
    x86::Gp dst = newGP8();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r8());
    COMP->or_(dst, src2.r8());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT16_TYPE: {
    // Destination
    x86::Gp dst = newGP16();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r16());
    COMP->or_(dst, src2.r16());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT32_TYPE: {
    // Destination
    x86::Gp dst = newGP32();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r32());
    COMP->or_(dst, src2.r32());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT64_TYPE: {
    // Destination
    x86::Gp dst = newGP64();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r64());
    COMP->or_(dst, src2.r64());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::VEC128_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    // Operation
    COMP->vpor(dst, src1, src2);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented OR type."); return;
  }
}

//
// XOR
//

REGISTER_EMITTER(OPCODE_XOR, Emit_XOR)
static void Emit_XOR(x86CodeGenBackend *b, const HIR::Instr *instr) {
  switch (instr->dest->type) {
  case HIR::INT8_TYPE: {
    // Destination
    x86::Gp dst = newGP8();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r8());
    COMP->xor_(dst, src2.r8());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT16_TYPE: {
    // Destination
    x86::Gp dst = newGP16();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r16());
    COMP->xor_(dst, src2.r16());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT32_TYPE: {
    // Destination
    x86::Gp dst = newGP32();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r32());
    COMP->xor_(dst, src2.r32());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT64_TYPE: {
    // Destination
    x86::Gp dst = newGP64();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r64());
    COMP->xor_(dst, src2.r64());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::VEC128_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    // Operation
    COMP->vpxor(dst, src1, src2);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented XOR type."); return;
  }
}

//
// NOT
//

REGISTER_EMITTER(OPCODE_NOT, Emit_NOT)
static void Emit_NOT(x86CodeGenBackend *b, const HIR::Instr *instr) {
  switch (instr->dest->type) {
  case HIR::INT8_TYPE: {
    // Destination
    x86::Gp dst = newGP8();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    // Operation
    COMP->mov(dst, src1.r8());
    COMP->not_(dst);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT16_TYPE: {
    // Destination
    x86::Gp dst = newGP16();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    // Operation
    COMP->mov(dst, src1.r16());
    COMP->not_(dst);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT32_TYPE: {
    // Destination
    x86::Gp dst = newGP32();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    // Operation
    COMP->mov(dst, src1.r32());
    COMP->not_(dst);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT64_TYPE: {
    // Destination
    x86::Gp dst = newGP64();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    // Operation
    COMP->mov(dst, src1.r64());
    COMP->not_(dst);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::VEC128_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    // Operation
    COMP->vpxor(dst, src1, LoadXmmConst(b, XMMFFFF));
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented NOT type."); return;
  }
}

//
// Shift Left
//

REGISTER_EMITTER(OPCODE_SHL, Emit_SHL)
static void Emit_SHL(x86CodeGenBackend *b, const HIR::Instr *instr) {
  switch (instr->dest->type) {
  case HIR::INT8_TYPE: {
    // Destination
    x86::Gp dst = newGP8();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r8());
    COMP->shl(dst, src2.r8());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT16_TYPE: {
    // Destination
    x86::Gp dst = newGP16();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r16());
    COMP->shl(dst, src2.r8());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT32_TYPE: {
    // Destination
    x86::Gp dst = newGP32();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r32());
    COMP->shl(dst, src2.r8());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT64_TYPE: {
    // Destination
    x86::Gp dst = newGP64();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r64());
    COMP->shl(dst, src2.r8());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::VEC128_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    // Operation
    // Build the signature manually: __m128i(void*, __m128i, __m128i)
    // asmjit has no TypeIdOfT for __m128i, so use explicit TypeId values.
    asmjit::FuncSignature sig(
      asmjit::CallConvId::kCDecl,
      asmjit::FuncSignature::kNoVarArgs,
      asmjit::TypeId::kInt32x4,   // return: __m128i
      asmjit::TypeId::kUIntPtr,   // arg0:   void*
      asmjit::TypeId::kInt32x4,   // arg1:   __m128i
      asmjit::TypeId::kUInt8);    // arg2:   unsigned char

    asmjit::InvokeNode *invokeNode = nullptr;
    Xe::JITCompat::Invoke(COMP, invokeNode, asmjit::Imm(reinterpret_cast<uintptr_t>(EmulateShlV128)), sig);
    Xe::JITCompat::SetArg(invokeNode, 0, asmjit::Imm(0)); // void* ctx = nullptr
    Xe::JITCompat::SetArg(invokeNode, 1, src1);
    Xe::JITCompat::SetArg(invokeNode, 2, src2);
    Xe::JITCompat::SetRet(invokeNode, 0, dst);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented SHL type."); return;
  }
}

//
// Shift Right
//

REGISTER_EMITTER(OPCODE_SHR, Emit_SHR)
static void Emit_SHR(x86CodeGenBackend *b, const HIR::Instr *instr) {
  switch (instr->dest->type) {
  case HIR::INT8_TYPE: {
    // Destination
    x86::Gp dst = newGP8();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r8());
    COMP->shr(dst, src2.r8());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT16_TYPE: {
    // Destination
    x86::Gp dst = newGP16();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r16());
    COMP->shr(dst, src2.r8());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT32_TYPE: {
    // Destination
    x86::Gp dst = newGP32();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r32());
    COMP->shr(dst, src2.r8());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT64_TYPE: {
    // Destination
    x86::Gp dst = newGP64();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r64());
    COMP->shr(dst, src2.r8());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::VEC128_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
    // Operation
    // Build the signature manually: __m128i(void*, __m128i, __m128i)
    // asmjit has no TypeIdOfT for __m128i, so use explicit TypeId values.
    asmjit::FuncSignature sig(
      asmjit::CallConvId::kCDecl,
      asmjit::FuncSignature::kNoVarArgs,
      asmjit::TypeId::kInt32x4,   // return: __m128i
      asmjit::TypeId::kUIntPtr,   // arg0:   void*
      asmjit::TypeId::kInt32x4,   // arg1:   __m128i
      asmjit::TypeId::kUInt8);    // arg2:   uint8_t

    asmjit::InvokeNode *invokeNode = nullptr;
    Xe::JITCompat::Invoke(COMP, invokeNode, asmjit::Imm(reinterpret_cast<uintptr_t>(EmulateShrV128)), sig);
    Xe::JITCompat::SetArg(invokeNode, 0, asmjit::Imm(0)); // void* ctx = nullptr
    Xe::JITCompat::SetArg(invokeNode, 1, src1);
    Xe::JITCompat::SetArg(invokeNode, 2, src2);
    Xe::JITCompat::SetRet(invokeNode, 0, dst);
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented SHR type."); return;
  }
}

//
// Shift Right Arithmetic
//

REGISTER_EMITTER(OPCODE_SHA, Emit_SHA)
static void Emit_SHA(x86CodeGenBackend *b, const HIR::Instr *instr) {
  switch (instr->dest->type) {
  case HIR::INT8_TYPE: {
    // Destination
    x86::Gp dst = newGP8();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r8());
    COMP->sar(dst, src2.r8());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT16_TYPE: {
    // Destination
    x86::Gp dst = newGP16();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r16());
    COMP->sar(dst, src2.r8());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT32_TYPE: {
    // Destination
    x86::Gp dst = newGP32();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r32());
    COMP->sar(dst, src2.r8());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT64_TYPE: {
    // Destination
    x86::Gp dst = newGP64();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r64());
    COMP->sar(dst, src2.r8());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented SHR type."); return;
  }
}

//
// Rotate Left
//

REGISTER_EMITTER(OPCODE_ROTATE_LEFT, Emit_ROTATE_LEFT)
static void Emit_ROTATE_LEFT(x86CodeGenBackend *b, const HIR::Instr *instr) {
  switch (instr->dest->type) {
  case HIR::INT8_TYPE: {
    // Destination
    x86::Gp dst = newGP8();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r8());
    COMP->rol(dst, src2.r8());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT16_TYPE: {
    // Destination
    x86::Gp dst = newGP16();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r16());
    COMP->rol(dst, src2.r8());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT32_TYPE: {
    // Destination
    x86::Gp dst = newGP32();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r32());
    COMP->rol(dst, src2.r8());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT64_TYPE: {
    // Destination
    x86::Gp dst = newGP64();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    x86::Gp src2 = LoadValueGp(b, instr->src2.value);
    // Operation
    COMP->mov(dst, src1.r64());
    COMP->rol(dst, src2.r8());
    // Store result
    TagStoreReg(instr->dest, dst);
  } break;
  default: UNREACHABLE_MSG("Unimplemented ROTATE_LEFT type."); return;
  }
}

//
// Type Conversions
//

// BYTE_SWAP: dest = bswap(src1)
REGISTER_EMITTER(OPCODE_BYTE_SWAP, Emit_BYTE_SWAP)
static void Emit_BYTE_SWAP(x86CodeGenBackend *b, const HIR::Instr *instr) {
  switch (instr->dest->type) {
  case HIR::INT16_TYPE: {
    // Destination
    x86::Gp dst = newGP16();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    COMP->mov(dst, src1.r16());
    COMP->rol(dst, asmjit::Imm(8)); // bswap16 = rol16 by 8
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT32_TYPE: {
    // Destination
    x86::Gp dst = newGP32();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    COMP->mov(dst, src1.r32());
    COMP->bswap(dst);
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT64_TYPE: {
    // Destination
    x86::Gp dst = newGP64();
    // Get sources
    x86::Gp src1 = LoadValueGp(b, instr->src1.value);
    COMP->mov(dst, src1.r64());
    COMP->bswap(dst);
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::VEC128_TYPE: {
    // Destination
    x86::Vec dst = newXMM();
    // Get sources
    x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
    COMP->vpshufb(dst, src1, LoadXmmConst(b, XMMByteSwapMask));
    TagStoreReg(instr->dest, dst);
    return;
  }
  default: UNREACHABLE_MSG("Unimplemented BYTE_SWAP type."); return;
  }
}

//
// Count Leading Zeroes
//

REGISTER_EMITTER(OPCODE_CNTLZ, Emit_CNTLZ)
static void Emit_CNTLZ(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Sources
  x86::Gp src = LoadValueGp(b, instr->src1.value);
  // Destination
  x86::Gp dst = newGP8();
  x86::Gp tmp = newGP32();

  switch (instr->src1.value->type) {
  case HIR::INT32_TYPE:
    COMP->lzcnt(tmp.r32(), src.r32());
    break;
  case HIR::INT64_TYPE:
    COMP->lzcnt(tmp.r64(), src.r64());
    break;
  default: UNREACHABLE_MSG("Unimplemented LZCNT type."); return;
  }
  COMP->mov(dst, tmp.r8());
  TagStoreReg(instr->dest, dst);
}

//
// Set Rounding Mode
//

REGISTER_EMITTER(OPCODE_SET_ROUNDING_MODE, Emit_SET_ROUNDING_MODE)
static void Emit_SET_ROUNDING_MODE(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // MXCST Table holding correct values that map to PPC's FPSCR[RM]
  static const u32 mxcsrTable[] = { 0x1F80, 0x7F80, 0x5F80, 0x3F80, 0x9F80, 0xFF80, 0xDF80, 0xBF80, };
  // New Mode
  x86::Gp mode = LoadValueGp(b, instr->src1.value);
  // Table address ptr
  x86::Gp tableAddr = newGP64();
  // Get table address
  COMP->mov(tableAddr, asmjit::Imm(reinterpret_cast<uintptr_t>(mxcsrTable)));
  // Get correct mapping based on index and set mxcsr
  x86::Gp tmp = newGP64();
  COMP->mov(tmp, mode);
  COMP->and_(tmp, Imm(0x7));
  COMP->imul(tmp, Imm(0x4));
  COMP->add(tableAddr, tmp);
  COMP->vldmxcsr(x86::ptr(tableAddr));
}



//
// Convert Vector elements from Int to Float
//

REGISTER_EMITTER(OPCODE_VECTOR_CONVERT_I2F, Emit_VECTOR_CONVERT_I2F)
static void Emit_VECTOR_CONVERT_I2F(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Vec dst = newXMM();
  // Get sources
  x86::Vec src1 = LoadValueXmm(b, instr->src1.value);

  // Unsigned?
  if (instr->flags & HIR::ARITHMETIC_UNSIGNED) {
    // Unsigned int32 to float with correct rounding for large values.
    // See Xenia comments about double rounding for values >= 0x80000000.
    x86::Vec xmm0 = newXMM();
    x86::Vec xmm1 = newXMM();

    // Round manually to (1.stored mantissa bits * 2^31) or to 2^32 to the
    // nearest even (the only rounding mode used on AltiVec) if the number is
    // 0x80000000 or greater, instead of converting src & 0x7FFFFFFF and then
    // adding 2147483648.0f, which results in double rounding that can give a
    // result larger than needed - see OPCODE_VECTOR_CONVERT_I2F notes.

    // [0x80000000, 0xFFFFFFFF] case:

    // Round to the nearest even, from (0x80000000 | 31 stored mantissa bits)
    // to ((-1 << 23) | 23 stored mantissa bits), or to 0 if the result should
    // be 4294967296.0f.
    COMP->vpaddd(xmm1, src1, LoadXmmConst(b, XMMInt127));
    COMP->vpslld(xmm0, src1, Imm(31 - 8));
    COMP->vpsrld(xmm0, xmm0, Imm(31));
    COMP->vpaddd(xmm0, xmm0, xmm1);
    // xmm0 = (0xFF800000 | 23 explicit mantissa bits), or 0 if overflowed
    COMP->vpsrad(xmm0, xmm0, Imm(8));
    // Calculate the result for the [0x80000000, 0xFFFFFFFF] case - take the
    // rounded mantissa, and add -1 or 0 to the exponent of 32, depending on
    // whether the number should be (1.stored mantissa bits * 2^31) or 2^32.
    // xmm0 = [0x80000000, 0xFFFFFFFF] case result
    COMP->vpaddd(xmm0, xmm0, LoadXmmConst(b, XMM2To32));

    // [0x00000000, 0x7FFFFFFF] case
    // (during vblendvps reg -> vpaddd reg -> vpaddd mem dependency):

    // Convert from signed integer to float.
    // xmm1 = [0x00000000, 0x7FFFFFFF] case result
    COMP->vcvtdq2ps(xmm1, src1);

    // Merge the two ways depending on whether the number is >= 0x80000000 (has high bit set).
    COMP->vblendvps(dst, xmm1, xmm0, src1);
  } else {
    COMP->vcvtdq2ps(dst, src1);
  }

  // Store result
  TagStoreReg(instr->dest, dst);
}

//
// Convert Vector elements from Float to Int
//

REGISTER_EMITTER(OPCODE_VECTOR_CONVERT_F2I, Emit_VECTOR_CONVERT_F2I)
static void Emit_VECTOR_CONVERT_F2I(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Vec dst = newXMM();
  // Get sources
  x86::Vec src1 = LoadValueXmm(b, instr->src1.value);

  // Unsigned?
  if (instr->flags & HIR::ARITHMETIC_UNSIGNED) {
    x86::Vec xmm0 = newXMM();
    x86::Vec xmm1 = newXMM();
    x86::Vec xmm2 = newXMM();

    // Clamp to min 0
    COMP->vmaxps(xmm0, src1, LoadXmmConst(b, XMMZero));

    // Mask of values >= 2^31.
    COMP->vcmpps(xmm1, xmm0, LoadXmmConst(b, XMMPosIntMinPS), Imm(0x0D)); // _CMP_GE_OS

    // Scale values >= 2^31 back to [0, ...]
    COMP->vsubps(xmm2, xmm0, LoadXmmConst(b, XMMPosIntMinPS));
    COMP->vblendvps(xmm0, xmm0, xmm2, xmm1);

    // Convert [0, INT_MAX].
    COMP->vcvttps2dq(dst, xmm0);

    // Detect saturation (values that overflowed to 0x80000000).
    COMP->vpcmpeqd(xmm0, dst, LoadXmmConst(b, XMMIntMin));

    // Add INT_MIN back for originally-high values.
    COMP->vpand(xmm1, xmm1, LoadXmmConst(b, XMMIntMin));
    COMP->vpaddd(dst, dst, xmm1);

    // Saturate overflows to 0xFFFFFFFF.
    COMP->vpor(dst, dst, xmm0);
  } else {
    x86::Vec xmm0 = newXMM();
    x86::Vec xmm1 = newXMM();
    x86::Vec xmm2 = newXMM();

    // NaN mask.
    COMP->vcmpps(xmm2, src1, src1, Imm(0x03)); // _CMP_UNORD_Q

    // Convert.
    COMP->vcvttps2dq(xmm0, src1);

    // Detect indeterminate (overflow to 0x80000000) where src >= 0.
    COMP->vpcmpeqd(xmm1, xmm0, LoadXmmConst(b, XMMIntMin));
    COMP->vpandn(xmm1, src1, xmm1);

    // Saturate positive overflows to INT_MAX.
    COMP->vblendvps(dst, xmm0, LoadXmmConst(b, XMMIntMax), xmm1);

    // Zero out NaN results.
    COMP->vpandn(dst, xmm2, dst);
  }

  // Store result
  TagStoreReg(instr->dest, dst);
}

//
// Load Vector Shift Left
//

REGISTER_EMITTER(OPCODE_LOAD_VECTOR_SHL, Emit_LOAD_VECTOR_SHL)
  static void Emit_LOAD_VECTOR_SHL(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Vec dst = newXMM();
  // Get sources
  x86::Gp src1 = LoadValueGp(b, instr->src1.value);
  // Table address (lvsl table)
  x86::Gp tableAddr = newGP64();
  // Table index
  x86::Gp idx = newGP64();

  COMP->movzx(idx.r32(), src1);
  COMP->and_(idx, 0xF);
  COMP->shl(idx, Imm(4));
  // Get final address: tableAddr = &table + idx
  COMP->mov(tableAddr, Imm(reinterpret_cast<uptr>(&loadVectorShiftLeftTable)));
  COMP->add(tableAddr, idx);
  COMP->vmovaps(dst, x86::ptr(tableAddr));
  // Store result
  TagStoreReg(instr->dest, dst);
}

//
// Load Vector Shift Right
//

REGISTER_EMITTER(OPCODE_LOAD_VECTOR_SHR, Emit_LOAD_VECTOR_SHR)
  static void Emit_LOAD_VECTOR_SHR(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Vec dst = newXMM();
  // Get sources
  x86::Gp src1 = LoadValueGp(b, instr->src1.value);
  // Table address (lvsl table)
  x86::Gp tableAddr = newGP64();
  // Table index
  x86::Gp idx = newGP64();

  COMP->movzx(idx.r32(), src1);
  COMP->and_(idx, 0xF);
  COMP->shl(idx, Imm(4));
  // Get final address: tableAddr = &table + idx
  COMP->mov(tableAddr, Imm(reinterpret_cast<uptr>(&loadVectorShiftRightTable)));
  COMP->add(tableAddr, idx);
  COMP->vmovaps(dst, x86::ptr(tableAddr));
  // Store result
  TagStoreReg(instr->dest, dst);
}

//
// Vector Max
//

REGISTER_EMITTER(OPCODE_VECTOR_MAX, Emit_VECTOR_MAX)
  static void Emit_VECTOR_MAX(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Vec dst = newXMM();
  // Get sources
  x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
  x86::Vec src2 = LoadValueXmm(b, instr->src2.value);

  // Get part type
  HIR::TypeName partType = static_cast<HIR::TypeName>(instr->flags >> 8);

  // Unsigned?
  if (instr->flags & HIR::ARITHMETIC_UNSIGNED) {
    switch (partType) {
    case HIR::INT8_TYPE:
      COMP->vpmaxub(dst, src1, src2);
      break;
    case HIR::INT16_TYPE:
      COMP->vpmaxuw(dst, src1, src2);
      break;
    case HIR::INT32_TYPE:
      COMP->vpmaxud(dst, src1, src2);
      break;
    default: UNREACHABLE_MSG("Unimplemented VECTOR_MAX Unsigned type."); return;
    }
  } else {
    switch (partType) {
    case HIR::INT8_TYPE:
      COMP->vpmaxsb(dst, src1, src2);
      break;
    case HIR::INT16_TYPE:
      COMP->vpmaxsw(dst, src1, src2);
      break;
    case HIR::INT32_TYPE:
      COMP->vpmaxsd(dst, src1, src2);
      break;
    default: UNREACHABLE_MSG("Unimplemented VECTOR_MAX signed type."); return;
    }
  }
  // Store result
  TagStoreReg(instr->dest, dst);
}

//
// Vector Min
//

REGISTER_EMITTER(OPCODE_VECTOR_MIN, Emit_VECTOR_MIN)
  static void Emit_VECTOR_MIN(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Vec dst = newXMM();
  // Get sources
  x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
  x86::Vec src2 = LoadValueXmm(b, instr->src2.value);

  // Get part type
  HIR::TypeName partType = static_cast<HIR::TypeName>(instr->flags >> 8);

  // Unsigned?
  if (instr->flags & HIR::ARITHMETIC_UNSIGNED) {
    switch (partType) {
    case HIR::INT8_TYPE:
      COMP->vpminub(dst, src1, src2);
      break;
    case HIR::INT16_TYPE:
      COMP->vpminuw(dst, src1, src2);
      break;
    case HIR::INT32_TYPE:
      COMP->vpminud(dst, src1, src2);
      break;
    default: UNREACHABLE_MSG("Unimplemented VECTOR_MIX Unsigned type."); return;
    }
  } else {
    switch (partType) {
    case HIR::INT8_TYPE:
      COMP->vpminsb(dst, src1, src2);
      break;
    case HIR::INT16_TYPE:
      COMP->vpminsw(dst, src1, src2);
      break;
    case HIR::INT32_TYPE:
      COMP->vpminsd(dst, src1, src2);
      break;
    default: UNREACHABLE_MSG("Unimplemented VECTOR_MIX signed type."); return;
    }
  }
  // Store result
  TagStoreReg(instr->dest, dst);
}


//
// Vector Compare Equal
//

REGISTER_EMITTER(OPCODE_VECTOR_COMPARE_EQ, Emit_VECTOR_COMPARE_EQ)
static void Emit_VECTOR_COMPARE_EQ(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Vec dst = newXMM();
  // Get sources
  x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
  x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
  // Get part type
  HIR::TypeName partType = static_cast<HIR::TypeName>(instr->flags);

  switch (partType) {
  case HIR::INT8_TYPE:
    COMP->vpcmpeqb(dst, src1, src2);
    break;
  case HIR::INT16_TYPE:
    COMP->vpcmpeqw(dst, src1, src2);
    break;
  case HIR::INT32_TYPE:
    COMP->vpcmpeqd(dst, src1, src2);
    break;
  case HIR::FLOAT32_TYPE:
    COMP->vcmpps(dst, src1, src2, Imm(0x00)); // _CMP_EQ_OQ
    break;
  default: UNREACHABLE_MSG("Unimplemented VECTOR_COMPARE_EQ type."); return;
  }
  // Store result
  TagStoreReg(instr->dest, dst);
}

//
// Vector Compare Signed Greater Than
//

REGISTER_EMITTER(OPCODE_VECTOR_COMPARE_SGT, Emit_VECTOR_COMPARE_SGT)
static void Emit_VECTOR_COMPARE_SGT(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Vec dst = newXMM();
  // Get sources
  x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
  x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
  // Get part type
  HIR::TypeName partType = static_cast<HIR::TypeName>(instr->flags);

  switch (partType) {
  case HIR::INT8_TYPE:
    COMP->vpcmpgtb(dst, src1, src2);
    break;
  case HIR::INT16_TYPE:
    COMP->vpcmpgtw(dst, src1, src2);
    break;
  case HIR::INT32_TYPE:
    COMP->vpcmpgtd(dst, src1, src2);
    break;
  case HIR::FLOAT32_TYPE:
    COMP->vcmpps(dst, src1, src2, Imm(0x0E)); // _CMP_GT_OS
    break;
  default: UNREACHABLE_MSG("Unimplemented VECTOR_COMPARE_SGT type."); return;
  }
  // Store result
  TagStoreReg(instr->dest, dst);
}

//
// Vector Compare Signed Greater Equal
//

REGISTER_EMITTER(OPCODE_VECTOR_COMPARE_SGE, Emit_VECTOR_COMPARE_SGE)
static void Emit_VECTOR_COMPARE_SGE(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Vec dst = newXMM();
  // Get sources
  x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
  x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
  // Get part type
  HIR::TypeName partType = static_cast<HIR::TypeName>(instr->flags);
  // Temp
  x86::Vec xmm0 = newXMM();

  switch (partType) {
  case HIR::INT8_TYPE:{
    COMP->vpcmpeqb(xmm0, src1, src2);
    COMP->vpcmpgtb(dst, src1, src2);
    COMP->vpor(dst, dst, xmm0);
    break;
  }
  case HIR::INT16_TYPE: {
    COMP->vpcmpeqw(xmm0, src1, src2);
    COMP->vpcmpgtw(dst, src1, src2);
    COMP->vpor(dst, dst, xmm0);
    break;
  }
  case HIR::INT32_TYPE: {
    COMP->vpcmpeqd(xmm0, src1, src2);
    COMP->vpcmpgtd(dst, src1, src2);
    COMP->vpor(dst, dst, xmm0);
    break;
  }
  case HIR::FLOAT32_TYPE:
    COMP->vcmpps(dst, src1, src2, Imm(0x0D)); // _CMP_GE_OS
    break;
  default: UNREACHABLE_MSG("Unimplemented VECTOR_COMPARE_SGE type."); return;
  }
  // Store result
  TagStoreReg(instr->dest, dst);
}

//
// Vector Compare Unsigned Greater Than
//

REGISTER_EMITTER(OPCODE_VECTOR_COMPARE_UGT, Emit_VECTOR_COMPARE_UGT)
static void Emit_VECTOR_COMPARE_UGT(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Vec dst = newXMM();
  // Get sources
  x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
  x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
  // Temps
  x86::Vec xmm0 = newXMM();
  x86::Vec xmm1 = newXMM();

  HIR::TypeName partType = static_cast<HIR::TypeName>(instr->flags);
  switch (partType) {
  case HIR::INT8_TYPE: {
    x86::Vec mask = LoadXmmConst(b, XMMSignMaskI8);
    COMP->vpxor(xmm0, src1, mask);
    COMP->vpxor(xmm1, src2, mask);
    COMP->vpcmpgtb(dst, xmm0, xmm1);
    break;
  }
  case HIR::INT16_TYPE: {
    x86::Vec mask = LoadXmmConst(b, XMMSignMaskI16);
    COMP->vpxor(xmm0, src1, mask);
    COMP->vpxor(xmm1, src2, mask);
    COMP->vpcmpgtw(dst, xmm0, xmm1);
    break;
  }
  case HIR::INT32_TYPE: {
    x86::Vec mask = LoadXmmConst(b, XMMSignMaskI32);
    COMP->vpxor(xmm0, src1, mask);
    COMP->vpxor(xmm1, src2, mask);
    COMP->vpcmpgtd(dst, xmm0, xmm1);
    break;
  }
  case HIR::FLOAT32_TYPE: {
    x86::Vec mask = LoadXmmConst(b, XMMSignMaskF32);
    COMP->vpxor(xmm0, src1, mask);
    COMP->vpxor(xmm1, src2, mask);
    COMP->vcmpps(dst, xmm0, xmm1, Imm(0x0D)); // _CMP_GE_OS
    break;
  }
  default: UNREACHABLE_MSG("Unimplemented VECTOR_COMPARE_UGT type."); return;
  }
  // Store result
  TagStoreReg(instr->dest, dst);
}

//
// Vector Compare Unsigned Greater Equal
//

REGISTER_EMITTER(OPCODE_VECTOR_COMPARE_UGE, Emit_VECTOR_COMPARE_UGE)
static void Emit_VECTOR_COMPARE_UGE(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Vec dst = newXMM();
  // Get sources
  x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
  x86::Vec src2 = LoadValueXmm(b, instr->src2.value);

  // Temps
  x86::Vec xmm0 = newXMM();
  x86::Vec xmm1 = newXMM();
  x86::Vec xmm2 = newXMM();

  // Get part type
  HIR::TypeName partType = static_cast<HIR::TypeName>(instr->flags);

  switch (partType) {
  case HIR::INT8_TYPE: {
    x86::Vec mask = LoadXmmConst(b, XMMSignMaskI8);
    COMP->vpxor(xmm0, src1, mask);
    COMP->vpxor(xmm1, src2, mask);
    COMP->vpcmpeqb(xmm2, xmm0, xmm1);
    COMP->vpcmpgtb(dst, xmm0, xmm1);
    COMP->vpor(dst, dst, xmm2);
  } break;
  case HIR::INT16_TYPE: {
    x86::Vec mask = LoadXmmConst(b, XMMSignMaskI16);
    COMP->vpxor(xmm0, src1, mask);
    COMP->vpxor(xmm1, src2, mask);
    COMP->vpcmpeqw(xmm2, xmm0, xmm1);
    COMP->vpcmpgtw(dst, xmm0, xmm1);
    COMP->vpor(dst, dst, xmm2);
  } break;
  case HIR::INT32_TYPE: {
    x86::Vec mask = LoadXmmConst(b, XMMSignMaskI32);
    COMP->vpxor(xmm0, src1, mask);
    COMP->vpxor(xmm1, src2, mask);
    COMP->vpcmpeqd(xmm2, xmm0, xmm1);
    COMP->vpcmpgtd(dst, xmm0, xmm1);
    COMP->vpor(dst, dst, xmm2);
  } break;
  case HIR::FLOAT32_TYPE: {
    x86::Vec mask = LoadXmmConst(b, XMMSignMaskF32);
    COMP->vpxor(xmm0, src1, mask);
    COMP->vpxor(xmm1, src2, mask);
    COMP->vcmpps(dst, src1, src2, Imm(0x0D)); // _CMP_GE_OS
  } break;
  default: UNREACHABLE_MSG("Unimplemented VECTOR_COMPARE_UGE type."); return;
  }
  // Store result
  TagStoreReg(instr->dest, dst);
}

//
// Vector Add
//

REGISTER_EMITTER(OPCODE_VECTOR_ADD, Emit_VECTOR_ADD)
static void Emit_VECTOR_ADD(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Vec dst = newXMM();
  // Get sources
  x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
  x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
  // Get part type
  HIR::TypeName partType = static_cast<HIR::TypeName>(instr->flags & 0xFF);
 // Get flags
  u32 flags = instr->flags >> 8;
  // Get unsignned and saturate flags
  bool isUnsigned = !!(flags & HIR::ARITHMETIC_UNSIGNED);
  bool saturate = !!(flags & HIR::ARITHMETIC_SATURATE);

  switch (partType) {
  case HIR::INT8_TYPE:
    if (saturate) {
      if (isUnsigned)
        COMP->vpaddusb(dst, src1, src2);
      else
        COMP->vpaddsb(dst, src1, src2);
    } else {
      COMP->vpaddb(dst, src1, src2);
    }
    break;
  case HIR::INT16_TYPE:
    if (saturate) {
      if (isUnsigned)
        COMP->vpaddusw(dst, src1, src2);
      else
        COMP->vpaddsw(dst, src1, src2);
    } else {
      COMP->vpaddw(dst, src1, src2);
    }
    break;
  case HIR::INT32_TYPE:
    if (saturate) {
      if (isUnsigned) {
        x86::Vec xmm0 = newXMM();
        x86::Vec xmm1 = newXMM();
        x86::Vec xmm2 = newXMM();

        // Unsigned saturate: add, then max with original (overflow wraps).
        COMP->vpaddd(xmm1, src1, src2);

        // If result is smaller than either of the inputs, we've
        // overflowed (only need to check one input)
        // if (src1 > res) then overflowed
        // http://locklessinc.com/articles/sat_arithmetic/

        COMP->vpxor(xmm2, src1, LoadXmmConst(b, XMMSignMaskI32));
        COMP->vpxor(xmm0, xmm1, LoadXmmConst(b, XMMSignMaskI32));
        COMP->vpcmpgtd(xmm0, xmm2, xmm0);
        COMP->vpor(dst, xmm1, xmm0);
      } else {
        x86::Vec xmm1 = newXMM();
        x86::Vec xmm2 = newXMM();
        x86::Vec xmm3 = newXMM();

        COMP->vpaddd(xmm1, src1, src2);

        // Overflow results if two inputs are the same sign and the
        // result isn't the same sign. if ((s32b)(~(src1 ^ src2) &
        // (src1 ^ res)) < 0) then overflowed
        // http://locklessinc.com/articles/sat_arithmetic/
        COMP->vpxor(xmm2, src1, src2);
        COMP->vpxor(xmm3, src1, xmm1);
        COMP->vpandn(xmm2, xmm2, xmm3);

        // Set any negative overflowed elements of src1 to INT_MIN
        COMP->vpand(xmm3, src1, xmm2);
        COMP->vblendvps(xmm1, xmm1, LoadXmmConst(b, XMMSignMaskI32), xmm3);
        // Set any positive overflowed elements of src1 to INT_MAX
        COMP->vpandn(xmm3, src1, xmm2);
        COMP->vblendvps(dst, xmm1, LoadXmmConst(b, XMMAbsMaskPS), xmm3);
      }
    } else {
      COMP->vpaddd(dst, src1, src2);
    }
    break;
  case HIR::FLOAT32_TYPE:
    if (isUnsigned || saturate) { UNREACHABLE_MSG("Invalid flags in VECTOR_ADD"); }
    COMP->vaddps(dst, src1, src2);
    break;
  default: UNREACHABLE_MSG("Unimplemented VECTOR_ADD type."); return;
  }
  // Store result
  TagStoreReg(instr->dest, dst);
}

//
// Vector Subtract
//

REGISTER_EMITTER(OPCODE_VECTOR_SUB, Emit_VECTOR_SUB)
static void Emit_VECTOR_SUB(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Vec dst = newXMM();
  // Get sources
  x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
  x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
  // Get part type
  HIR::TypeName partType = static_cast<HIR::TypeName>(instr->flags & 0xFF);
  // Get flags
  u32 flags = instr->flags >> 8;
  // Get unsignned and saturate flags
  bool isUnsigned = !!(flags & HIR::ARITHMETIC_UNSIGNED);
  bool saturate = !!(flags & HIR::ARITHMETIC_SATURATE);

  switch (partType) {
  case HIR::INT8_TYPE:
    if (saturate) {
      if (isUnsigned)
        COMP->vpsubusb(dst, src1, src2);
      else
        COMP->vpsubsb(dst, src1, src2);
    } else {
      COMP->vpsubb(dst, src1, src2);
    }
    break;
  case HIR::INT16_TYPE:
    if (saturate) {
      if (isUnsigned)
        COMP->vpsubusw(dst, src1, src2);
      else
        COMP->vpsubsw(dst, src1, src2);
    } else {
      COMP->vpsubw(dst, src1, src2);
    }
    break;
  case HIR::INT32_TYPE:
    if (saturate) {
      if (isUnsigned) {
        x86::Vec xmm0 = newXMM();
        x86::Vec xmm1 = newXMM();
        x86::Vec xmm2 = newXMM();

        // Unsigned saturate: add, then max with original (overflow wraps).
        COMP->vpsubd(xmm1, src1, src2);

        // If result is greater than either of the inputs, we've
        // underflowed (only need to check one input)
        // if (res > src1) then underflowed
        // http://locklessinc.com/articles/sat_arithmetic/

        COMP->vpxor(xmm2, src1, LoadXmmConst(b, XMMSignMaskI32));
        COMP->vpxor(xmm0, xmm1, LoadXmmConst(b, XMMSignMaskI32));
        COMP->vpcmpgtd(xmm0, xmm0, xmm2);
        COMP->vpandn(dst, xmm0, xmm1);
      } else {
        x86::Vec xmm1 = newXMM();
        x86::Vec xmm2 = newXMM();
        x86::Vec xmm3 = newXMM();

        COMP->vpsubd(xmm1, src1, src2);

        // We can only overflow if the signs of the operands are
        // opposite. If signs are opposite and result sign isn't the
        // same as src1's sign, we've overflowed. if ((s32b)((src1 ^
        // src2) & (src1 ^ res)) < 0) then overflowed
        // http://locklessinc.com/articles/sat_arithmetic/
        COMP->vpxor(xmm2, src1, src2);
        COMP->vpxor(xmm3, src1, xmm1);
        COMP->vpand(xmm2, xmm2, xmm3);

        // Set any negative overflowed elements of src1 to INT_MIN
        COMP->vpand(xmm3, src1, xmm2);
        COMP->vblendvps(xmm1, xmm1, LoadXmmConst(b, XMMSignMaskI32), xmm3);
        // Set any positive overflowed elements of src1 to INT_MAX
        COMP->vpandn(xmm3, src1, xmm2);
        COMP->vblendvps(dst, xmm1, LoadXmmConst(b, XMMAbsMaskPS), xmm3);
      }
    } else {
      COMP->vpsubd(dst, src1, src2);
    }
    break;
  case HIR::FLOAT32_TYPE:
    if (isUnsigned || saturate) { UNREACHABLE_MSG("Invalid flags in VECTOR_ADD"); }
    COMP->vsubps(dst, src1, src2);
    break;
  default: UNREACHABLE_MSG("Unimplemented VECTOR_ADD type."); return;
  }
  // Store result
  TagStoreReg(instr->dest, dst);
}

// ========================================================================
// OPCODE_VECTOR_SHL
// ========================================================================
REGISTER_EMITTER(OPCODE_VECTOR_SHL, Emit_VECTOR_SHL)
static void Emit_VECTOR_SHL(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Vec dst = newXMM();
  // Get sources
  x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
  x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
  // Get part type
  HIR::TypeName partType = static_cast<HIR::TypeName>(instr->flags);

  using ShiftFn = __m128i(*)(void *, __m128i, __m128i);

  // Build the signature manually: __m128i(void*, __m128i, __m128i)
  // asmjit has no TypeIdOfT for __m128i, so use explicit TypeId values.
  asmjit::FuncSignature sig(
    asmjit::CallConvId::kCDecl,
    asmjit::FuncSignature::kNoVarArgs,
    asmjit::TypeId::kInt32x4,   // return: __m128i
    asmjit::TypeId::kUIntPtr,   // arg0:   void*
    asmjit::TypeId::kInt32x4,   // arg1:   __m128i
    asmjit::TypeId::kInt32x4);  // arg2:   __m128i

  switch (partType) {
  case HIR::INT8_TYPE: {
    ShiftFn fn = static_cast<ShiftFn>(EmulateVectorShl<uint8_t>);
    InvokeNode *invokeNode = nullptr;
    Xe::JITCompat::Invoke(COMP, invokeNode, asmjit::Imm(reinterpret_cast<uptr>(fn)), sig);
    Xe::JITCompat::SetArg(invokeNode, 0, Imm(0)); // void* ctx = nullptr
    Xe::JITCompat::SetArg(invokeNode, 1, src1);
    Xe::JITCompat::SetArg(invokeNode, 2, src2);
    Xe::JITCompat::SetRet(invokeNode, 0, dst);
  } break;
  case HIR::INT16_TYPE: {
    ShiftFn fn = static_cast<ShiftFn>(EmulateVectorShl<uint16_t>);
    InvokeNode *invokeNode = nullptr;
    Xe::JITCompat::Invoke(COMP, invokeNode, asmjit::Imm(reinterpret_cast<uptr>(fn)), sig);
    Xe::JITCompat::SetArg(invokeNode, 0, Imm(0)); // void* ctx = nullptr
    Xe::JITCompat::SetArg(invokeNode, 1, src1);
    Xe::JITCompat::SetArg(invokeNode, 2, src2);
    Xe::JITCompat::SetRet(invokeNode, 0, dst);
  } break;
  case HIR::INT32_TYPE: {
    x86::Vec xmm0 = newXMM();
    COMP->vandps(xmm0, src2, LoadXmmConst(b, XMMShiftMaskPS));
    COMP->vpsllvd(dst, src1, xmm0);
  } break;
  default: UNREACHABLE_MSG("Unimplemented VECTOR_SHL type."); return;
  }
  // Store result
  TagStoreReg(instr->dest, dst);
}

// ========================================================================
// OPCODE_VECTOR_SHR
// ========================================================================
REGISTER_EMITTER(OPCODE_VECTOR_SHR, Emit_VECTOR_SHR)
static void Emit_VECTOR_SHR(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Vec dst = newXMM();
  // Get sources
  x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
  x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
  // Get part type
  HIR::TypeName partType = static_cast<HIR::TypeName>(instr->flags);

  using ShiftFn = __m128i(*)(void *, __m128i, __m128i);

  // Build the signature manually: __m128i(void*, __m128i, __m128i)
  // asmjit has no TypeIdOfT for __m128i, so use explicit TypeId values.
  asmjit::FuncSignature sig(
    asmjit::CallConvId::kCDecl,
    asmjit::FuncSignature::kNoVarArgs,
    asmjit::TypeId::kInt32x4,   // return: __m128i
    asmjit::TypeId::kUIntPtr,   // arg0:   void*
    asmjit::TypeId::kInt32x4,   // arg1:   __m128i
    asmjit::TypeId::kInt32x4);  // arg2:   __m128i

  switch (partType) {
  case HIR::INT8_TYPE: {
    ShiftFn fn = static_cast<ShiftFn>(EmulateVectorShr<uint8_t>);
    InvokeNode *invokeNode = nullptr;
    Xe::JITCompat::Invoke(COMP, invokeNode, asmjit::Imm(reinterpret_cast<uptr>(fn)), sig);
    Xe::JITCompat::SetArg(invokeNode, 0, Imm(0)); // void* ctx = nullptr
    Xe::JITCompat::SetArg(invokeNode, 1, src1);
    Xe::JITCompat::SetArg(invokeNode, 2, src2);
    Xe::JITCompat::SetRet(invokeNode, 0, dst);
  } break;
  case HIR::INT16_TYPE: {
    ShiftFn fn = static_cast<ShiftFn>(EmulateVectorShr<uint16_t>);
    InvokeNode *invokeNode = nullptr;
    Xe::JITCompat::Invoke(COMP, invokeNode, asmjit::Imm(reinterpret_cast<uptr>(fn)), sig);
    Xe::JITCompat::SetArg(invokeNode, 0, Imm(0)); // void* ctx = nullptr
    Xe::JITCompat::SetArg(invokeNode, 1, src1);
    Xe::JITCompat::SetArg(invokeNode, 2, src2);
    Xe::JITCompat::SetRet(invokeNode, 0, dst);
  } break;
  case HIR::INT32_TYPE: {
    x86::Vec xmm0 = newXMM();
    COMP->vandps(xmm0, src2, LoadXmmConst(b, XMMShiftMaskPS));
    COMP->vpsrlvd(dst, src1, xmm0);
  } break;
  default: UNREACHABLE_MSG("Unimplemented VECTOR_SHR type."); return;
  }
  // Store result
  TagStoreReg(instr->dest, dst);
}

//
// Vector Shift Arithmetic
//

REGISTER_EMITTER(OPCODE_VECTOR_SHA, Emit_VECTOR_SHA)
static void Emit_VECTOR_SHA(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Vec dst = newXMM();
  // Get sources
  x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
  x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
  // Get part type
  HIR::TypeName partType = static_cast<HIR::TypeName>(instr->flags);

  using ShiftFn = __m128i(*)(void *, __m128i, __m128i);

  // Build the signature manually: __m128i(void*, __m128i, __m128i)
  // asmjit has no TypeIdOfT for __m128i, so use explicit TypeId values.
  asmjit::FuncSignature sig(
    asmjit::CallConvId::kCDecl,
    asmjit::FuncSignature::kNoVarArgs,
    asmjit::TypeId::kInt32x4,   // return: __m128i
    asmjit::TypeId::kUIntPtr,   // arg0:   void*
    asmjit::TypeId::kInt32x4,   // arg1:   __m128i
    asmjit::TypeId::kInt32x4);  // arg2:   __m128i

  switch (partType) {
  case HIR::INT8_TYPE: {
    ShiftFn fn = static_cast<ShiftFn>(EmulateVectorShr<int8_t>);
    InvokeNode *invokeNode = nullptr;
    Xe::JITCompat::Invoke(COMP, invokeNode, asmjit::Imm(reinterpret_cast<uptr>(fn)), sig);
    Xe::JITCompat::SetArg(invokeNode, 0, Imm(0)); // void* ctx = nullptr
    Xe::JITCompat::SetArg(invokeNode, 1, src1);
    Xe::JITCompat::SetArg(invokeNode, 2, src2);
    Xe::JITCompat::SetRet(invokeNode, 0, dst);
  } break;
  case HIR::INT16_TYPE: {
    ShiftFn fn = static_cast<ShiftFn>(EmulateVectorShr<int16_t>);
    InvokeNode *invokeNode = nullptr;
    Xe::JITCompat::Invoke(COMP, invokeNode, asmjit::Imm(reinterpret_cast<uptr>(fn)), sig);
    Xe::JITCompat::SetArg(invokeNode, 0, Imm(0)); // void* ctx = nullptr
    Xe::JITCompat::SetArg(invokeNode, 1, src1);
    Xe::JITCompat::SetArg(invokeNode, 2, src2);
    Xe::JITCompat::SetRet(invokeNode, 0, dst);
  } break;
  case HIR::INT32_TYPE: {
    x86::Vec xmm0 = newXMM();
    COMP->vandps(xmm0, src2, LoadXmmConst(b, XMMShiftMaskPS));
    COMP->vpsravd(dst, src1, xmm0);
  } break;
  default: UNREACHABLE_MSG("Unimplemented VECTOR_SHA type."); return;
  }
  // Store result
  TagStoreReg(instr->dest, dst);
}

//
// Vector Rotate Left
//

REGISTER_EMITTER(OPCODE_VECTOR_ROTATE_LEFT, Emit_VECTOR_ROTATE_LEFT)
static void Emit_VECTOR_ROTATE_LEFT(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Vec dst = newXMM();
  // Get sources
  x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
  x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
  // Get part type
  HIR::TypeName partType = static_cast<HIR::TypeName>(instr->flags);

  using ShiftFn = __m128i(*)(void *, __m128i, __m128i);

  // Build the signature manually: __m128i(void*, __m128i, __m128i)
  // asmjit has no TypeIdOfT for __m128i, so use explicit TypeId values.
  asmjit::FuncSignature sig(
    asmjit::CallConvId::kCDecl,
    asmjit::FuncSignature::kNoVarArgs,
    asmjit::TypeId::kInt32x4,   // return: __m128i
    asmjit::TypeId::kUIntPtr,   // arg0:   void*
    asmjit::TypeId::kInt32x4,   // arg1:   __m128i
    asmjit::TypeId::kInt32x4);  // arg2:   __m128i

  switch (partType) {
  case HIR::INT8_TYPE: {
    ShiftFn fn = static_cast<ShiftFn>(EmulateVectorRotateLeft<uint8_t>);
    InvokeNode *invokeNode = nullptr;
    Xe::JITCompat::Invoke(COMP, invokeNode, asmjit::Imm(reinterpret_cast<uptr>(fn)), sig);
    Xe::JITCompat::SetArg(invokeNode, 0, Imm(0)); // void* ctx = nullptr
    Xe::JITCompat::SetArg(invokeNode, 1, src1);
    Xe::JITCompat::SetArg(invokeNode, 2, src2);
    Xe::JITCompat::SetRet(invokeNode, 0, dst);
  } break;
  case HIR::INT16_TYPE: {
    ShiftFn fn = static_cast<ShiftFn>(EmulateVectorRotateLeft<uint16_t>);
    InvokeNode *invokeNode = nullptr;
    Xe::JITCompat::Invoke(COMP, invokeNode, asmjit::Imm(reinterpret_cast<uptr>(fn)), sig);
    Xe::JITCompat::SetArg(invokeNode, 0, Imm(0)); // void* ctx = nullptr
    Xe::JITCompat::SetArg(invokeNode, 1, src1);
    Xe::JITCompat::SetArg(invokeNode, 2, src2);
    Xe::JITCompat::SetRet(invokeNode, 0, dst);
  } break;
  case HIR::INT32_TYPE: {
    x86::Vec xmm0 = newXMM();
    x86::Vec xmm1 = newXMM();
    x86::Vec tmp = newXMM();
    // Shift left (to get high bits):
    COMP->vpand(xmm0, src2, LoadXmmConst(b, XMMShiftMaskPS));
    COMP->vpsllvd(xmm1, src1, xmm0);
    // Shift right (to get low bits):
    COMP->vmovaps(tmp, LoadXmmConst(b, XMMPI32));
    COMP->vpsubd(tmp, tmp, xmm0);
    COMP->vpsrlvd(dst, src1, tmp);
    // Merge:
    COMP->vpor(dst, dst, xmm1);
  } break;
  default: UNREACHABLE_MSG("Unimplemented VECTOR_ROTATE_LEFT type."); return;
  }
  // Store result
  TagStoreReg(instr->dest, dst);
}

//
// Vector Average
//

REGISTER_EMITTER(OPCODE_VECTOR_AVERAGE, Emit_VECTOR_AVERAGE)
static void Emit_VECTOR_AVERAGE(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Vec dst = newXMM();
  // Get sources
  x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
  x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
  // Get part type
  HIR::TypeName partType = static_cast<HIR::TypeName>(instr->flags & 0xFF);
  // Get flags
  u8 flags = instr->flags >> 8;
  // Unsigned?
  bool isUnsigned = !!(flags & HIR::ARITHMETIC_UNSIGNED);

  switch (partType) {
  case HIR::INT8_TYPE:
    if (isUnsigned) {
      COMP->vpavgb(dst, src1, src2);
    } else {
      UNREACHABLE_MSG("VECTOR_AVERAGE: Signed INT8 not supported.");
    }
    break;
  case HIR::INT16_TYPE:
    if (isUnsigned) {
      COMP->vpavgw(dst, src1, src2);
    } else {
      UNREACHABLE_MSG("VECTOR_AVERAGE: Signed INT16 not supported.");
    }
    break;
  case HIR::INT32_TYPE: {
    // Sadly there's no 32 bit averages in AVX/2.
    // Fall back to scalar emulation via EmulateVectorAverage.
    using AvgFn = __m128i(*)(void *, __m128i, __m128i);
    AvgFn fn = isUnsigned
      ? static_cast<AvgFn>(EmulateVectorAverage<uint32_t>)
      : static_cast<AvgFn>(EmulateVectorAverage<int32_t>);

    // Build the signature manually: __m128i(void*, __m128i, __m128i)
    // asmjit has no TypeIdOfT for __m128i, so use explicit TypeId values.
    asmjit::FuncSignature sig(
      asmjit::CallConvId::kCDecl,
      asmjit::FuncSignature::kNoVarArgs,
      asmjit::TypeId::kInt32x4,   // return: __m128i
      asmjit::TypeId::kUIntPtr,   // arg0:   void*
      asmjit::TypeId::kInt32x4,   // arg1:   __m128i
      asmjit::TypeId::kInt32x4);  // arg2:   __m128i

    asmjit::InvokeNode *invokeNode = nullptr;
    Xe::JITCompat::Invoke(COMP, invokeNode, asmjit::Imm(reinterpret_cast<uptr>(fn)), sig);
    Xe::JITCompat::SetArg(invokeNode, 0, asmjit::Imm(0)); // void* ctx = nullptr
    Xe::JITCompat::SetArg(invokeNode, 1, src1);
    Xe::JITCompat::SetArg(invokeNode, 2, src2);
    Xe::JITCompat::SetRet(invokeNode, 0, dst);
    break;
  }
  default: UNREACHABLE_MSG("Unimplemented VECTOR_AVERAGE type."); return;
  }
  // Store result
  TagStoreReg(instr->dest, dst);
}


//
// Insert
//

// Stuck here!

REGISTER_EMITTER(OPCODE_INSERT, Emit_INSERT)
static void Emit_INSERT(x86CodeGenBackend *b, const HIR::Instr *instr) {
  x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
  x86::Vec dst = newXMM();
  COMP->vmovaps(dst, src1);

  u32 index = static_cast<u32>(instr->src3.offset);
  HIR::TypeName partType = static_cast<HIR::TypeName>(instr->flags);

  switch (partType) {
  case HIR::INT8_TYPE: {
    x86::Gp val = LoadValueGp(b, instr->src2.value);
    // XOR index with 3 for big-endian byte order within dword.
    COMP->vpinsrb(dst, dst, val.r32(), asmjit::Imm(index ^ 0x3));
    break;
  }
  case HIR::INT16_TYPE: {
    x86::Gp val = LoadValueGp(b, instr->src2.value);
    COMP->vpinsrw(dst, dst, val.r32(), asmjit::Imm(index ^ 0x1));
    break;
  }
  case HIR::INT32_TYPE: {
    x86::Gp val = LoadValueGp(b, instr->src2.value);
    COMP->vpinsrd(dst, dst, val.r32(), asmjit::Imm(index));
    break;
  }
  default: UNREACHABLE_MSG("Unimplemented INSERT type."); return;
  }
  // Store result
  TagStoreReg(instr->dest, dst);
}

// ========================================================================
// OPCODE_EXTRACT
// ========================================================================

REGISTER_EMITTER(OPCODE_EXTRACT, Emit_EXTRACT)
  static void Emit_EXTRACT(x86CodeGenBackend *b, const HIR::Instr *instr) {
  x86::Vec src1 = LoadValueXmm(b, instr->src1.value);

  HIR::TypeName partType = static_cast<HIR::TypeName>(instr->dest->type);

  switch (partType) {
  case HIR::INT8_TYPE: {
    x86::Gp dst = newGP32();
    if (instr->src2.value->IsConstant()) {
      // Constant path: direct vpextrb with big-endian byte XOR.
      COMP->vpextrb(dst, src1, asmjit::Imm(instr->src2.value->constant.u8 ^ 0x3));
      COMP->and_(dst, asmjit::Imm(0xFF));
    } else {
      // Non-constant path (Xenia-style):
      // Build a vpshufb selector that routes byte (idx ^ 3) into lane 0.
      x86::Gp index = LoadValueGp(b, instr->src2.value);
      x86::Gp eax = newGP32();
      // eax = 0x00000003 ^ (index & 0x1F), gives the rotated byte position.
      COMP->mov(eax, asmjit::Imm(0x00000003));
      COMP->xor_(eax.r8(), index.r8());
      COMP->and_(eax.r8(), asmjit::Imm(0x1F));
      // Broadcast selector into xmm0, then vpshufb routes src1[eax] -> lane 0.
      x86::Vec shuf = newXMM();
      COMP->vmovd(shuf, eax);
      COMP->vpshufb(shuf, src1, shuf);
      COMP->vmovd(dst, shuf);
      COMP->and_(dst, asmjit::Imm(0xFF));
    }
    TagStoreReg(instr->dest, dst);
    break;
  }
  case HIR::INT16_TYPE: {
    x86::Gp dst = newGP32();
    if (instr->src2.value->IsConstant()) {
      // Constant path: direct vpextrw with big-endian word XOR.
      COMP->vpextrw(dst, src1, asmjit::Imm(instr->src2.value->constant.u8 ^ 0x1));
      COMP->and_(dst, asmjit::Imm(0xFFFF));
    } else {
      // Non-constant path (Xenia-style):
      // Build a 2-byte vpshufb selector in eax[7:0] (lo byte) and eax[15:8] (hi byte).
      x86::Gp index = LoadValueGp(b, instr->src2.value);
      x86::Gp eax = newGP32();
      // al = (index ^ 0x01) << 1  => byte offset of the word's low byte.
      COMP->mov(eax.r8(), index.r8());
      COMP->xor_(eax.r8(), asmjit::Imm(0x01));
      COMP->shl(eax.r8(), asmjit::Imm(1));
      // ah = al + 1  => byte offset of the word's high byte.
      x86::Gp ah = newGP8();
      COMP->mov(ah, eax.r8());
      COMP->add(ah, asmjit::Imm(1));
      // Pack al/ah into eax low word: eax = (ah << 8) | al.
      COMP->movzx(eax, eax.r8());
      x86::Gp ahExt = newGP32();
      COMP->movzx(ahExt, ah);
      COMP->shl(ahExt, asmjit::Imm(8));
      COMP->or_(eax, ahExt);
      // vpshufb: routes src1[al] -> byte 0, src1[ah] -> byte 1 of lane 0.
      x86::Vec shuf = newXMM();
      COMP->vmovd(shuf, eax);
      COMP->vpshufb(shuf, src1, shuf);
      COMP->vmovd(eax, shuf);
      COMP->and_(eax, asmjit::Imm(0xFFFF));
      COMP->mov(dst, eax);
    }
    TagStoreReg(instr->dest, dst);
    break;
  }
  case HIR::INT32_TYPE: {
    x86::Gp dst = newGP32();
    if (instr->src2.value->IsConstant()) {
      // Constant path: direct vpextrd (no XOR needed for dwords).
      COMP->vpextrd(dst, src1, asmjit::Imm(instr->src2.value->constant.u8));
    } else {
      // Non-constant path (Xenia-style):
      // Build a 4-byte vpshufb selector routing dword[index] into lane 0.
      x86::Gp index = LoadValueGp(b, instr->src2.value);
      x86::Gp eax = newGP32();
      // base = index << 2 => byte offset of dword start.
      COMP->movzx(eax, index.r8());
      COMP->shl(eax, asmjit::Imm(2));
      // Build selector word: bytes 0..3 = base, base+1, base+2, base+3.
      x86::Gp tmp = newGP32();
      COMP->mov(tmp, eax);
      COMP->add(tmp, asmjit::Imm(1));
      COMP->shl(tmp, asmjit::Imm(8));
      COMP->or_(eax, tmp);
      COMP->mov(tmp, eax);
      // eax now has [base+1, base] in low 16 bits; build full 32-bit selector.
      x86::Gp hi = newGP32();
      COMP->movzx(hi, index.r8());
      COMP->shl(hi, asmjit::Imm(2));
      COMP->add(hi, asmjit::Imm(2));
      x86::Gp hi2 = newGP32();
      COMP->mov(hi2, hi);
      COMP->add(hi2, asmjit::Imm(1));
      COMP->shl(hi2, asmjit::Imm(8));
      COMP->or_(hi, hi2);
      COMP->shl(hi, asmjit::Imm(16));
      COMP->or_(eax, hi);
      // vpshufb routes dword bytes into lane 0.
      x86::Vec shuf = newXMM();
      COMP->vmovd(shuf, eax);
      COMP->vpshufb(shuf, src1, shuf);
      COMP->vmovd(dst, shuf);
    }
    TagStoreReg(instr->dest, dst);
    break;
  }
  default: UNREACHABLE_MSG("Unimplemented EXTRACT type."); return;
  }
}

// ========================================================================
// OPCODE_SPLAT
// ========================================================================
REGISTER_EMITTER(OPCODE_SPLAT, Emit_SPLAT)
static void Emit_SPLAT(x86CodeGenBackend *b, const HIR::Instr *instr) {
  x86::Vec dst = newXMM();

  switch (instr->src1.value->type) {
  case HIR::INT8_TYPE: {
    x86::Gp val = LoadValueGp(b, instr->src1.value);
    x86::Gp val32 = newGP32();
    COMP->movzx(val32, val.r8());
    COMP->vmovd(dst, val32);
    COMP->vpbroadcastb(dst, dst);
    break;
  }
  case HIR::INT16_TYPE: {
    x86::Gp val = LoadValueGp(b, instr->src1.value);
    x86::Gp val32 = newGP32();
    COMP->movzx(val32, val.r16());
    COMP->vmovd(dst, val32);
    COMP->vpbroadcastw(dst, dst);
    break;
  }
  case HIR::INT32_TYPE: {
    x86::Gp val = LoadValueGp(b, instr->src1.value);
    COMP->vmovd(dst, val.r32());
    COMP->vpbroadcastd(dst, dst);
    break;
  }
  case HIR::FLOAT32_TYPE: {
    x86::Vec val = LoadValueXmm(b, instr->src1.value);
    COMP->vbroadcastss(dst, val);
    break;
  }
  default: UNREACHABLE_MSG("Unimplemented SPLAT type."); return;
  }
  TagStoreReg(instr->dest, dst);
}

// Permute across two source vectors using a control mask.
// partType (in instr->flags) determines the granularity:
//   INT8_TYPE:  byte-level (vperm). Control = VEC128 with byte indices 0..31.
//   INT16_TYPE: halfword-level. Control = VEC128 with halfword indices 0..15.
//   INT32_TYPE: dword-level. Control = constant u32 from MakePermuteMask().
REGISTER_EMITTER(OPCODE_PERMUTE, Emit_PERMUTE)
  static void Emit_PERMUTE(x86CodeGenBackend *b, const HIR::Instr *instr) {

  // Switch Dest Type
  switch (instr->dest->type) {
  case HIR::VEC128_TYPE: {

    // Get Part Type
    HIR::TypeName partType = static_cast<HIR::TypeName>(instr->flags);

    // Switch Part Type
    switch (partType) {
    case HIR::INT32_TYPE: {
      if (instr->src1.value->IsConstant()) {
        u32 control = instr->src1.value->constant.u32;
        // Shuffle things into the right places in dest & tmp,
        // then we blend them together.
        u32 src_control = (((control >> 24) & 0x3) << 6) | (((control >> 16) & 0x3) << 4) |
          (((control >> 8) & 0x3) << 2) | (((control >> 0) & 0x3) << 0);

        u32 blend_control =
          (((control >> 26) & 0x1) << 3) | (((control >> 18) & 0x1) << 2) |
          (((control >> 10) & 0x1) << 1) | (((control >> 2) & 0x1) << 0);

        x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
        x86::Vec src3 = LoadValueXmm(b, instr->src3.value);
        x86::Vec dst = newXMM();
        x86::Vec tmp = newXMM();

        COMP->vpshufd(dst, src2, asmjit::Imm(src_control));
        COMP->vpshufd(tmp, src3, asmjit::Imm(src_control));
        COMP->vpblendd(dst, dst, tmp, asmjit::Imm(blend_control));
        TagStoreReg(instr->dest, dst);
      } else {
        // Permute by non-constant.
        UNREACHABLE_MSG("PERMUTE Part INT32 by non constant");
      }

      break;
    }
    case HIR::INT16_TYPE: {
      // Destination
      x86::Vec dst = newXMM();
      // Get sources
      x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
      x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
      x86::Vec src3 = LoadValueXmm(b, instr->src3.value);
      // Temps
      x86::Vec xmm0 = newXMM();
      x86::Vec xmm1 = newXMM();
      x86::Vec xmm2 = newXMM();

      Base::Vector128 perm = (instr->src1.value->constant.v128 & Base::Vector128s(0xF)) ^ Base::Vector128s(0x1);
      Base::Vector128 perm_ctrl = Base::Vector128b(0);
      for (int i = 0; i < 8; i++) {
        perm_ctrl.sword[i] = perm.sword[i] > 7 ? -1 : 0;

        auto v = u8(perm.word[i]);
        perm.bytes[i * 2] = v * 2;
        perm.bytes[i * 2 + 1] = v * 2 + 1;
      }


      // Store into the HIR value's constant storage so the pointer is stable.
      // TODO: Correctly implement dynamic constant system.
      instr->src1.value->constant.v128 = perm;

      COMP->vmovdqu(xmm0, LoadXmmConst(b, instr->src1.value->constant.v128));

      COMP->vmovdqa(xmm1, src2);
      COMP->vmovdqa(xmm2, src3);
      COMP->vpshufb(xmm1, xmm1, xmm0);
      COMP->vpshufb(xmm2, xmm2, xmm0);

      u8 mask = 0;
      for (int i = 0; i < 8; i++) {
        if (perm_ctrl.sword[i] == 0) {
          mask |= 1 << (7 - i);
        }
      }

      COMP->vpblendw(dst, xmm1, xmm2, Imm(mask));
      // Store result
      TagStoreReg(instr->dest, dst);
      break;
    }
    case HIR::INT8_TYPE: {
      // General permute.
      // Control mask needs to be shuffled.

      // Destination
      x86::Vec dst = newXMM();
      // Get sources
      x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
      x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
      x86::Vec src3 = LoadValueXmm(b, instr->src3.value);
      // Temps
      x86::Vec xmm2 = newXMM();
      x86::Vec src2Shuffled = newXMM();
      x86::Vec src3Shuffled = newXMM();

      COMP->vxorps(xmm2, src1, LoadXmmConst(b, XMMSwapWordMask));
      COMP->vpand(xmm2, xmm2, LoadXmmConst(b, XMMPermuteByteMask));

      COMP->vpshufb(src2Shuffled, src2, xmm2);
      COMP->vpshufb(src3Shuffled, src3, xmm2);
      COMP->vpcmpgtb(dst, xmm2, LoadXmmConst(b, XMMPermuteControl15));
      COMP->vpblendvb(dst, src2Shuffled, src3Shuffled, dst);
      // Store result
      TagStoreReg(instr->dest, dst);
    }
      break;
    default: UNREACHABLE_MSG("Unimplemented PERMUTE part type."); return;
    }
    break;
  }
  default: UNREACHABLE_MSG("Unimplemented PERMUTE type."); return;
  }
}

// ========================================================================
// OPCODE_SWIZZLE
// ========================================================================
REGISTER_EMITTER(OPCODE_SWIZZLE, Emit_SWIZZLE)
static void Emit_SWIZZLE(x86CodeGenBackend *b, const HIR::Instr *instr) {
  u32 elementType = instr->flags;

  if (elementType == HIR::INT32_TYPE || elementType == HIR::FLOAT32_TYPE) {
    x86::Vec src = LoadValueXmm(b, instr->src1.value);
    u8 swizzleMask = static_cast<u8>(instr->src2.offset);
    x86::Vec dst = newXMM();
    COMP->vpshufd(dst, src, swizzleMask);
    TagStoreReg(instr->dest, dst);
  } else {
    UNREACHABLE_MSG("OPCODE_SWIZZLE currently only supports 32-bit element types (DWORD/FLOAT)");
  }
}

// ========================================================================
// OPCODE_DOT_PRODUCT_3
// ========================================================================
REGISTER_EMITTER(OPCODE_DOT_PRODUCT_3, Emit_DOT_PRODUCT_3)
static void Emit_DOT_PRODUCT_3(x86CodeGenBackend *b, const HIR::Instr *instr) {
  x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
  x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
  x86::Vec dst = newXMM();
  // dpps with mask 0x71: multiply X,Y,Z (bits 4,5,6), store to X (bit 0).
  // But PPC wants result in all lanes, so use 0x7F (store to all).
  COMP->vdpps(dst, src1, src2, asmjit::Imm(0x7F));
  TagStoreReg(instr->dest, dst);
}

// ========================================================================
// OPCODE_DOT_PRODUCT_4
// ========================================================================
REGISTER_EMITTER(OPCODE_DOT_PRODUCT_4, Emit_DOT_PRODUCT_4)
static void Emit_DOT_PRODUCT_4(x86CodeGenBackend *b, const HIR::Instr *instr) {
  x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
  x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
  x86::Vec dst = newXMM();
  // dpps with mask 0xFF: multiply all 4 lanes, store to all.
  COMP->vdpps(dst, src1, src2, asmjit::Imm(0xFF));
  TagStoreReg(instr->dest, dst);
}

//
// Vector Pack
//

REGISTER_EMITTER(OPCODE_PACK, Emit_PACK)
static void Emit_PACK(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Vec dst = newXMM();
  // Get sources
  x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
  x86::Vec src2 = LoadValueXmm(b, instr->src2.value);
  // Get flags
  u32 flags = instr->flags;
  // Pack mode
  u32 mode = flags & HIR::PACK_TYPE_MODE;

  // Build the signature manually: __m128i(void*, __m128i, __m128i)
  // asmjit has no TypeIdOfT for __m128i, so use explicit TypeId values.
  asmjit::FuncSignature sig(
    asmjit::CallConvId::kCDecl,
    asmjit::FuncSignature::kNoVarArgs,
    asmjit::TypeId::kInt32x4,   // return: __m128i
    asmjit::TypeId::kUIntPtr,   // arg0:   void*
    asmjit::TypeId::kInt32x4,   // arg1:   __m128i
    asmjit::TypeId::kInt32x4);  // arg2:   __m128i

  switch (mode) {
  case HIR::PACK_TYPE_D3DCOLOR: {
    // Saturate to [3,3....] so that only values between 3...[00] and 3...[FF]
    // are valid - max before min to pack NaN as zero (5454082B is heavily
    // affected by the order - packs 0xFFFFFFFF in matrix code to get a 0
    // constant).
    COMP->vmaxps(dst, src1, LoadXmmConst(b, XMM3333));
    COMP->vminps(dst, dst, LoadXmmConst(b, XMMPackD3DCOLORSat));
    // Extract bytes.
    // RGBA (XYZW) -> ARGB (WXYZ)
    // w = ((src1.uw & 0xFF) << 24) | ((src1.ux & 0xFF) << 16) |
    //     ((src1.uy & 0xFF) << 8) | (src1.uz & 0xFF)
    COMP->vpshufb(dst, dst, LoadXmmConst(b, XMMPackD3DCOLOR));
  } break;
  case HIR::PACK_TYPE_FLOAT16_2: {
    // 0|0|0|0|W|Z|Y|X
    COMP->vcvtps2ph(dst, src1, 0b00000011);
    // Shuffle to X|Y|0|0|0|0|0|0
    COMP->vpshufb(dst, dst, LoadXmmConst(b, XMMPackFLOAT16_2));
  } break;
  case HIR::PACK_TYPE_FLOAT16_4: {
    // 0|0|0|0|W|Z|Y|X
    COMP->vcvtps2ph(dst, src1, 0b00000011);
    // Shuffle to Z|W|X|Y|0|0|0|0
    COMP->vpshufb(dst, dst, LoadXmmConst(b, XMMPackFLOAT16_4));
  } break;
  case HIR::PACK_TYPE_SHORT_2: {
    // Saturate
    COMP->vmaxps(dst, src1, LoadXmmConst(b, XMMPackSHORT_Min));
    COMP->vminps(dst, dst, LoadXmmConst(b, XMMPackSHORT_Max));
    COMP->vcvttps2dq(dst, dst);
    // Pack
    COMP->vpshufb(dst, dst, LoadXmmConst(b, XMMPackSHORT_2));
  } break;
  case HIR::PACK_TYPE_SHORT_4: {
    // Saturate
    COMP->vmaxps(dst, src1, LoadXmmConst(b, XMMPackSHORT_Min));
    COMP->vminps(dst, dst, LoadXmmConst(b, XMMPackSHORT_Max));
    // Pack
    COMP->vpshufb(dst, dst, LoadXmmConst(b, XMMPackSHORT_4));
  } break;
  case HIR::PACK_TYPE_UINT_2101010: {
    // Temps
    x86::Vec xmm0 = newXMM();
    // Saturate.
    COMP->vmaxps(dst, src1, LoadXmmConst(b, XMMPackUINT_2101010_MinUnpacked));
    COMP->vminps(dst, dst, LoadXmmConst(b, XMMPackUINT_2101010_MaxUnpacked));
    // Remove the unneeded bits of the floats.
    COMP->vpand(dst, dst, LoadXmmConst(b, XMMPackUINT_2101010_MaskUnpacked));
    // Shift the components up.
    COMP->vpsllvd(dst, dst, LoadXmmConst(b, XMMPackUINT_2101010_Shift));
    // Combine the components.
    COMP->vshufps(xmm0, dst, dst, _MM_SHUFFLE(2, 3, 0, 1));
    COMP->vorps(dst,dst, xmm0);
    COMP->vshufps(xmm0, dst, dst, _MM_SHUFFLE(1, 0, 3, 2));
    COMP->vorps(dst, dst, xmm0);
  } break;
  case HIR::PACK_TYPE_ULONG_4202020: {
    // Temps
    x86::Vec xmm0 = newXMM();
    // Saturate.
    COMP->vmaxps(dst, src1, LoadXmmConst(b, XMMPackULONG_4202020_MinUnpacked));
    COMP->vminps(dst, dst, LoadXmmConst(b, XMMPackULONG_4202020_MaxUnpacked));
    // Remove the unneeded bits of the floats.
    COMP->vpand(dst, dst, LoadXmmConst(b, XMMPackULONG_4202020_MaskUnpacked));
    // Store Y and W shifted left by 4 so vpshufb can be used with them.
    COMP->vpslld(xmm0, dst, 4);
    // Place XZ where they're supposed to be.
    COMP->vpshufb(dst, dst, LoadXmmConst(b, XMMPackULONG_4202020_PermuteXZ));
    // Place YW.
    COMP->vpshufb(xmm0, xmm0, LoadXmmConst(b, XMMPackULONG_4202020_PermuteYW));
    // Merge
    COMP->vorps(dst, dst, xmm0);
  } break;
  case HIR::PACK_TYPE_8_IN_16: {
    if (HIR::IsPackInUnsigned(flags)) {
      if (HIR::IsPackOutUnsigned(flags)) {
        if (HIR::IsPackOutSaturate(flags)) {
          // unsigned -> unsigned + saturate
          asmjit::InvokeNode *invokeNode = nullptr;
          Xe::JITCompat::Invoke(COMP, invokeNode, asmjit::Imm(reinterpret_cast<uptr>(EmulatePack8_IN_16_UN_UN_SAT)), sig);
          Xe::JITCompat::SetArg(invokeNode, 0, asmjit::Imm(0)); // void* ctx = nullptr
          Xe::JITCompat::SetArg(invokeNode, 1, src1);
          Xe::JITCompat::SetArg(invokeNode, 2, src2);
          Xe::JITCompat::SetRet(invokeNode, 0, dst);

          COMP->vpshufb(dst, dst, LoadXmmConst(b, XMMByteOrderMask));
        } else {
          // unsigned -> unsigned
          asmjit::InvokeNode *invokeNode = nullptr;
          Xe::JITCompat::Invoke(COMP, invokeNode, asmjit::Imm(reinterpret_cast<uptr>(EmulatePack8_IN_16_UN_UN)), sig);
          Xe::JITCompat::SetArg(invokeNode, 0, asmjit::Imm(0)); // void* ctx = nullptr
          Xe::JITCompat::SetArg(invokeNode, 1, src1);
          Xe::JITCompat::SetArg(invokeNode, 2, src2);
          Xe::JITCompat::SetRet(invokeNode, 0, dst);

          COMP->vpshufb(dst, dst, LoadXmmConst(b, XMMByteOrderMask));
        }
      } else {
        if (HIR::IsPackOutSaturate(flags)) {
          // unsigned -> signed + saturate
          UNREACHABLE_MSG("Unimplemented PACK_TYPE_8_IN_16 type: unsigned -> signed + saturate.");
        } else {
          // unsigned -> signed
          UNREACHABLE_MSG("Unimplemented PACK_TYPE_8_IN_16 type: unsigned -> signed.");
        }
      }
    } else {
      if (HIR::IsPackOutUnsigned(flags)) {
        if (HIR::IsPackOutSaturate(flags)) {
          // signed -> unsigned + saturate
          // PACKUSWB / SaturateSignedWordToUnsignedByte
          COMP->vpackuswb(dst, src1, src2);
          COMP->vpshufb(dst, dst, LoadXmmConst(b, XMMByteOrderMask));
        } else {
          // signed -> unsigned
          UNREACHABLE_MSG("Unimplemented PACK_TYPE_8_IN_16 type: signed -> unsigned.");
        }
      } else {
        if (HIR::IsPackOutSaturate(flags)) {
          // signed -> signed + saturate
          // PACKSSWB / SaturateSignedWordToSignedByte
          COMP->vpacksswb(dst, src1, src2);
          COMP->vpshufb(dst, dst, LoadXmmConst(b, XMMByteOrderMask));
        } else {
          // signed -> signed
          UNREACHABLE_MSG("Unimplemented PACK_TYPE_8_IN_16 type: signed -> signed.");
        }
      }
    }
  } break;
  case HIR::PACK_TYPE_16_IN_32: {
    // Temps
    x86::Gp tmp = newGP32();
    x86::Vec xmm0 = newXMM();
    x86::Vec xmm1 = newXMM();

    if (HIR::IsPackInUnsigned(flags)) {
      if (HIR::IsPackOutUnsigned(flags)) {
        if (HIR::IsPackOutSaturate(flags)) {
          // unsigned -> unsigned + saturate
          // Construct a saturation max value
          COMP->mov(tmp, 0xFFFFu);
          COMP->vmovd(xmm0, tmp);
          COMP->vpshufd(xmm0, xmm0, 0b00000000);

          if (!instr->src1.value->IsConstant()) {
            COMP->vpminud(xmm1, src1, xmm0);  // Saturate src1
            COMP->vpshuflw(xmm1, xmm1, 0b00100010);
            COMP->vpshufhw(xmm1, xmm1, 0b00100010);
            COMP->vpshufd(xmm1, xmm1, 0b00001000);
          } else {
            // TODO(DrChat): Non-zero constants
            ASSERT(instr->src1.value->constant.v128.qword[0] == 0 && instr->src1.value->constant.v128.qword[1] == 0);
            COMP->vpxor(xmm1, xmm1, xmm1);
          }

          if (!instr->src2.value->IsConstant()) {
            COMP->vpminud(dst, src2, xmm0);  // Saturate src2
            COMP->vpshuflw(dst, dst, 0b00100010);
            COMP->vpshufhw(dst, dst, 0b00100010);
            COMP->vpshufd(dst, dst, 0b10000000);
          } else {
            // TODO(DrChat): Non-zero constants
            ASSERT(instr->src1.value->constant.v128.qword[0] == 0 && instr->src1.value->constant.v128.qword[1] == 0);
            COMP->vpxor(dst, dst, dst);
          }

          COMP->vpblendw(dst, dst, xmm1, 0b00001111);
        } else {
          // unsigned -> unsigned
          COMP->vmovaps(xmm0, src1);
          COMP->vpshuflw(xmm0, xmm0, 0b00100010);
          COMP->vpshufhw(xmm0, xmm0, 0b00100010);
          COMP->vpshufd(xmm0, xmm0, 0b00001000);

          COMP->vmovaps(dst, src2);
          COMP->vpshuflw(dst, dst, 0b00100010);
          COMP->vpshufhw(dst, dst, 0b00100010);
          COMP->vpshufd(dst, dst, 0b10000000);

          COMP->vpblendw(dst, dst, xmm0, 0b00001111);
        }
      } else {
        if (HIR::IsPackOutSaturate(flags)) {
          // unsigned -> signed + saturate
          UNREACHABLE_MSG("Unimplemented PACK_TYPE_16_IN_32 type: unsigned -> signed + saturate.");
        } else {
          // unsigned -> signed
          UNREACHABLE_MSG("Unimplemented PACK_TYPE_16_IN_32 type: unsigned -> signed.");
        }
      }
    } else {
      if (HIR::IsPackOutUnsigned(flags)) {
        if (HIR::IsPackOutSaturate(flags)) {
          // signed -> unsigned + saturate
          // PACKUSDW
          // TMP[15:0] <- (DEST[31:0] < 0) ? 0 : DEST[15:0];
          // DEST[15:0] <- (DEST[31:0] > FFFFH) ? FFFFH : TMP[15:0];
          COMP->vpackusdw(dst, src1, src2);
          COMP->vpshuflw(dst, dst, 0b10110001);
          COMP->vpshufhw(dst, dst, 0b10110001);
        } else {
          // signed -> unsigned
          UNREACHABLE_MSG("Unimplemented PACK_TYPE_16_IN_32 type: signed -> unsigned.");
        }
      } else {
        if (HIR::IsPackOutSaturate(flags)) {
          // signed -> signed + saturate
          // PACKSSDW / SaturateSignedDwordToSignedWord
          COMP->vpackssdw(dst, src1, src2);
          COMP->vpshuflw(dst, dst, 0b10110001);
          COMP->vpshufhw(dst, dst, 0b10110001);
        } else {
          // signed -> signed
          UNREACHABLE_MSG("Unimplemented PACK_TYPE_16_IN_32 type: signed -> signed.");
        }
      }
    }
  } break;
  default: UNREACHABLE_MSG("Unimplemented PACK type."); return;
  }
  // Store result
  TagStoreReg(instr->dest, dst);
}

//
// Vector Unpack
//

REGISTER_EMITTER(OPCODE_UNPACK, Emit_UNPACK)
static void Emit_UNPACK(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Destination
  x86::Vec dst = newXMM();
  // Get sources
  x86::Vec src1 = LoadValueXmm(b, instr->src1.value);
  // Get flags
  u32 flags = instr->flags;
  // Pack mode
  u32 mode = flags & HIR::PACK_TYPE_MODE;

  switch (mode) {
  case HIR::PACK_TYPE_D3DCOLOR: {
    // ARGB (WXYZ) -> RGBA (XYZW)
    // src = ZZYYXXWW
    // Unpack to 000000ZZ,000000YY,000000XX,000000WW
    COMP->vpshufb(dst, src1, LoadXmmConst(b, XMMUnpackD3DCOLOR));
    // Add 1.0f to each.
    COMP->vpor(dst, dst, LoadXmmConst(b, XMMOne));
    // To convert to 0 to 1, games multiply by 0x47008081 and add 0xC7008081.
  } break;
  case HIR::PACK_TYPE_FLOAT16_2: {
    // 1 bit sign, 5 bit exponent, 10 bit mantissa
    // D3D10 half float format
    // TODO(benvanik):
    // http://blogs.msdn.com/b/chuckw/archive/2012/09/11/directxmath-f16c-and-fma.aspx
    // Use _mm_cvtph_ps -- requires very modern processors (SSE5+)
    // Unpacking half floats:
    // http://fgiesen.wordpress.com/2012/03/28/half-to-float-done-quic/
    // Packing half floats: https://gist.github.com/rygorous/2156668
    // Load source, move from tight pack of X16Y16.... to X16...Y16...
    // Also zero out the high end.
    // TODO(benvanik): special case constant unpacks that just get 0/1/etc.

    // sx = src.iw >> 16;
    // sy = src.iw & 0xFFFF;
    // dest = { XMConvertHalfToFloat(sx),
    //          XMConvertHalfToFloat(sy),
    //          0.0,
    //          1.0 };
    // Shuffle to 0|0|0|0|0|0|Y|X

    COMP->vpshufb(dst, src1, LoadXmmConst(b, XMMUnpackFLOAT16_2));
    COMP->vcvtph2ps(dst, dst);
    COMP->vpshufd(dst, dst, 0b10100100);
    COMP->vpor(dst, dst, LoadXmmConst(b, XMM0001));
  } break;
  case HIR::PACK_TYPE_FLOAT16_4: {
    // src = [(dest.x | dest.y), (dest.z | dest.w), 0, 0]
    // Shuffle to 0|0|0|0|W|Z|Y|X
    COMP->vpshufb(dst, src1, LoadXmmConst(b, XMMUnpackFLOAT16_4));
    COMP->vcvtph2ps(dst, dst);
  } break;
  case HIR::PACK_TYPE_SHORT_2: {
    // Temps
    x86::Vec xmm0 = newXMM();

    // (VD.x) = 3.0 + (VB.x>>16)*2^-22
    // (VD.y) = 3.0 + (VB.x)*2^-22
    // (VD.z) = 0.0
    // (VD.w) = 1.0 (games splat W after unpacking to get vectors of 1.0f)
    // src is (xx,xx,xx,VALUE)

    // Shuffle bytes.
    COMP->vpshufb(dst, src1, LoadXmmConst(b, XMMUnpackSHORT_2));
    // If negative, make smaller than 3 - sign extend before adding.
    COMP->vpslld(dst, dst, 16);
    COMP->vpsrad(dst,dst, 16);
    // Add 3,3,0,1.
    COMP->vpaddd(dst, dst, LoadXmmConst(b, XMM3301));
    // Return quiet NaNs in case of negative overflow.
    COMP->vcmpps(xmm0, dst, LoadXmmConst(b, XMMUnpackSHORT_Overflow), Imm(0x00)); // _CMP_EQ_OS
    COMP->vblendvps(dst, dst, LoadXmmConst(b, XMMQNaN), xmm0);
  } break;
  case HIR::PACK_TYPE_SHORT_4: {
    // Temps
    x86::Vec xmm0 = newXMM();

    // (VD.x) = 3.0 + (VB.x>>16)*2^-22
    // (VD.y) = 3.0 + (VB.x)*2^-22
    // (VD.z) = 3.0 + (VB.y>>16)*2^-22
    // (VD.w) = 3.0 + (VB.y)*2^-22
    // src is (xx,xx,VALUE,VALUE)

    // Shuffle bytes.
    COMP->vpshufb(dst, src1, LoadXmmConst(b, XMMUnpackSHORT_4));
    // If negative, make smaller than 3 - sign extend before adding.
    COMP->vpslld(dst, dst, 16);
    COMP->vpsrad(dst, dst, 16);
    // Add 3,3,3,3.
    COMP->vpaddd(dst, dst, LoadXmmConst(b, XMM3333));
    // Return quiet NaNs in case of negative overflow.
    COMP->vcmpps(xmm0, dst, LoadXmmConst(b, XMMUnpackSHORT_Overflow), Imm(0x00)); // _CMP_EQ_OS
    COMP->vblendvps(dst, dst, LoadXmmConst(b, XMMQNaN), xmm0);
  } break;
  case HIR::PACK_TYPE_UINT_2101010: {
    // Temps
    x86::Vec xmm0 = newXMM();

    // Splat W.
    COMP->vshufps(dst, src1, src1, _MM_SHUFFLE(3, 3, 3, 3));
    // Keep only the needed components.
    // Red in 0-9 now, green in 10-19, blue in 20-29, alpha in 30-31.
    COMP->vpand(dst, dst, LoadXmmConst(b, XMMPackUINT_2101010_MaskPacked));
    // Shift the components down.
    COMP->vpsrlvd(dst, dst, LoadXmmConst(b, XMMPackUINT_2101010_Shift));
    // If XYZ are negative, make smaller than 3 - sign extend XYZ before adding.
    // W is unsigned.
    COMP->vpslld(dst, dst, 22);
    COMP->vpsrad(dst, dst, 22);
    // Add 3,3,3,1.
    COMP->vpaddd(dst, dst, LoadXmmConst(b, XMM3331));
    // Return quiet NaNs in case of negative overflow.
    COMP->vcmpps(xmm0, dst, LoadXmmConst(b, XMMUnpackUINT_2101010_Overflow), Imm(0x00)); // _CMP_EQ_OS
    COMP->vblendvps(dst, dst, LoadXmmConst(b, XMMQNaN), xmm0);
    // To convert XYZ to -1 to 1, games multiply by 0x46004020 & sub 0x46C06030.
    // For W to 0 to 1, they multiply by and subtract 0x4A2AAAAB.
  } break;
  case HIR::PACK_TYPE_ULONG_4202020: {
    // Temps
    x86::Vec xmm0 = newXMM();

    // Extract pairs of nibbles to XZYW. XZ will have excess 4 upper bits, YW
    // will have excess 4 lower bits.
    COMP->vpshufb(dst, src1, LoadXmmConst(b, XMMUnpackULONG_4202020_Permute));
    // Drop the excess nibble of YW.
    COMP->vpsrld(xmm0, dst, 4);
    // Merge XZ and YW now both starting at offset 0.
    COMP->vshufps(dst, dst, xmm0, _MM_SHUFFLE(3, 2, 1, 0));
    // Reorder as XYZW.
    COMP->vshufps(dst, dst, dst, _MM_SHUFFLE(3, 1, 2, 0));
    // Drop the excess upper nibble in XZ and sign-extend XYZ.
    COMP->vpslld(dst, dst, 12);
    COMP->vpsrad(dst, dst, 12);
    // Add 3,3,3,1.
    COMP->vpaddd(dst, dst, LoadXmmConst(b, XMM3331));
    // Return quiet NaNs in case of negative overflow.
    COMP->vcmpps(xmm0, dst, LoadXmmConst(b, XMMUnpackULONG_4202020_Overflow), Imm(0x00)); // _CMP_EQ_OS
    COMP->vblendvps(dst, dst, LoadXmmConst(b, XMMQNaN), xmm0);
  } break;
  case HIR::PACK_TYPE_8_IN_16: {
    if (HIR::IsPackToLo(flags)) {
      // Unpack to LO.
      if (HIR::IsPackInUnsigned(flags)) {
        if (HIR::IsPackOutUnsigned(flags)) {
          // unsigned -> unsigned
          UNREACHABLE_MSG("Unimplemented UNPACK_TYPE_8_IN_16 type: unsigned -> unsigned.");
        } else {
          // unsigned -> signed
          UNREACHABLE_MSG("Unimplemented UNPACK_TYPE_8_IN_16 type: unsigned -> signed.");
        }
      } else {
        if (HIR::IsPackOutUnsigned(flags)) {
          // signed -> unsigned
          UNREACHABLE_MSG("Unimplemented UNPACK_TYPE_8_IN_16 type: signed -> unsigned.");
        } else {
          // signed -> signed
          COMP->vpshufb(dst, src1, LoadXmmConst(b, XMMByteOrderMask));
          COMP->vpunpckhbw(dst, dst, dst);
          COMP->vpsraw(dst, dst, 8);
        }
      }
    } else {
      // Unpack to HI.
      if (HIR::IsPackInUnsigned(flags)) {
        if (HIR::IsPackOutUnsigned(flags)) {
          // unsigned -> unsigned
          UNREACHABLE_MSG("Unimplemented UNPACK_TYPE_8_IN_16 type: unsigned -> unsigned.");
        } else {
          // unsigned -> signed
          UNREACHABLE_MSG("Unimplemented UNPACK_TYPE_8_IN_16 type: unsigned -> signed.");
        }
      } else {
        if (HIR::IsPackOutUnsigned(flags)) {
          // signed -> unsigned
          UNREACHABLE_MSG("Unimplemented UNPACK_TYPE_8_IN_16 type: signed -> unsigned.");
        } else {
          // signed -> signed
          COMP->vpshufb(dst, src1, LoadXmmConst(b, XMMByteOrderMask));
          COMP->vpunpcklbw(dst, dst, dst);
          COMP->vpsraw(dst, dst, 8);
        }
      }
    }
  } break;
  case HIR::PACK_TYPE_16_IN_32: {
    if (HIR::IsPackToLo(flags)) {
      // Unpack to LO.
      if (HIR::IsPackInUnsigned(flags)) {
        if (HIR::IsPackOutUnsigned(flags)) {
          // unsigned -> unsigned
          UNREACHABLE_MSG("Unimplemented UNPACK_TYPE_16_IN_32 type: unsigned -> unsigned.");
        } else {
          // unsigned -> signed
          UNREACHABLE_MSG("Unimplemented UNPACK_TYPE_16_IN_32 type: unsigned -> signed.");
        }
      } else {
        if (HIR::IsPackOutUnsigned(flags)) {
          // signed -> unsigned
          UNREACHABLE_MSG("Unimplemented UNPACK_TYPE_16_IN_32 type: signed -> unsigned.");
        } else {
          // signed -> signed
          COMP->vpunpckhwd(dst, src1, src1);
          COMP->vpsrad(dst, dst, 16);
        }
      }
    } else {
      // Unpack to HI.
      if (HIR::IsPackInUnsigned(flags)) {
        if (HIR::IsPackOutUnsigned(flags)) {
          // unsigned -> unsigned
          UNREACHABLE_MSG("Unimplemented UNPACK_TYPE_16_IN_32 type: unsigned -> unsigned.");
        } else {
          // unsigned -> signed
          UNREACHABLE_MSG("Unimplemented UNPACK_TYPE_16_IN_32 type: unsigned -> signed.");
        }
      } else {
        if (HIR::IsPackOutUnsigned(flags)) {
          // signed -> unsigned
          UNREACHABLE_MSG("Unimplemented UNPACK_TYPE_16_IN_32 type: signed -> unsigned.");
        } else {
          // signed -> signed
          COMP->vpunpcklwd(dst, src1, src1);
          COMP->vpsrad(dst, dst, 16);
        }
      }
    }
    COMP->vpshufd(dst, dst, 0xB1);
  } break;
  default: UNREACHABLE_MSG("Unimplemented UNPACK type."); return;
  }
  // Store result
  TagStoreReg(instr->dest, dst);
}


void DumpHIR(HIR::HIRBlock *inBlock) {
  using namespace HIR;

  // Get current block
  HIRBlock *block = inBlock;

  if (!block) {
    LOG_INFO(Xenon, "[HIR Dump]: No HIR block generated");
    return;
  }

  // Formats a constant value as a decimal integer or float literal.
  auto fmtConst = [](const Value *v) -> std::string {
    switch (v->type) {
    case INT8_TYPE:    return FMT("{:#04x}", static_cast<u8>(v->constant.i8));
    case INT16_TYPE:   return FMT("{:#06x}", static_cast<u16>(v->constant.i16));
    case INT32_TYPE:   return FMT("{:#010x}", static_cast<u32>(v->constant.i32));
    case INT64_TYPE:   return FMT("{:#018x}", static_cast<u64>(v->constant.i64));
    case FLOAT32_TYPE: return FMT("{}", v->constant.f32);
    case FLOAT64_TYPE: return FMT("{}", v->constant.f64);
    case VEC128_TYPE:
      return FMT("[{:#010x}, {:#010x}, {:#010x}, {:#010x}]",
        v->constant.v128.dsword[0], v->constant.v128.dsword[1],
        v->constant.v128.dsword[2], v->constant.v128.dsword[3]);
    default: return "WTF?";
    }
  };

  // Formats a Value as "v<N>" for SSA values or the constant literal for constants.
  auto fmtVal = [&](const Value *v) -> std::string {
    if (!v) return "(null)";
    if (v->flags & VALUE_IS_CONSTANT) {
      return fmtConst(v);
    }
    return FMT("v{}", v->ordinal);
    };

  LOG_INFO(Xenon, "[HIR Dump]: === Begin HIR ===");

  const Instr *instr = block->instrHead;
  u32 instrCount = 0;
  while (instr) {
    if (!instr->opcode) {
      instr = instr->next;
      continue;
    }

    if (instr->opcode->num == OPCODE_COMMENT) {
      const char *text = reinterpret_cast<const char *>(instr->src1.offset);
      LOG_INFO(Xenon, "  // {}", text ? text : "");
    } else if (instr->opcode->num == OPCODE_NOP) {
      LOG_INFO(Xenon, "  nop");
    } else {
      const char *opName = instr->opcode->name ? instr->opcode->name : "???";

      u32 sig = instr->opcode->signature;
      u32 src1Type = (sig >> 3) & 0x7;
      u32 src2Type = (sig >> 6) & 0x7;
      u32 src3Type = (sig >> 9) & 0x7;

      // Build the operand list.
      std::string operands;
      auto appendOperand = [&](std::string_view op) {
        if (!operands.empty())
          operands += ", ";
        operands += op;
      };

      if (src1Type == OPCODE_SIG_TYPE_V && instr->src1.value) {
        appendOperand(fmtVal(instr->src1.value));
      } else if (src1Type == OPCODE_SIG_TYPE_O) {
        appendOperand(FMT("+{:#x}", instr->src1.offset));
      } else if (src1Type == OPCODE_SIG_TYPE_S && instr->src1.label) {
        appendOperand(FMT("label@{}", static_cast<const void *>(instr->src1.label)));
      } else if (src1Type == OPCODE_SIG_TYPE_L) {
        appendOperand(FMT("label@{}", static_cast<const void *>(instr->src1.label)));
      }

      if (src2Type == OPCODE_SIG_TYPE_V && instr->src2.value) {
        appendOperand(fmtVal(instr->src2.value));
      } else if (src2Type == OPCODE_SIG_TYPE_O) {
        appendOperand(FMT("+{:#x}", instr->src2.offset));
      } else if (src2Type == OPCODE_SIG_TYPE_L) {
        appendOperand(FMT("label@{}", static_cast<const void *>(instr->src2.label)));
      }

      if (src3Type == OPCODE_SIG_TYPE_V && instr->src3.value) {
        appendOperand(fmtVal(instr->src3.value));
      } else if (src3Type == OPCODE_SIG_TYPE_O) {
        appendOperand(FMT("+{:#x}", instr->src3.offset));
      }

      // Emit: "  [vN = ]opname [operands]"
      if (instr->dest) {
        if (!operands.empty()) {
          LOG_INFO(Xenon, "  v{} = {} {}", instr->dest->ordinal, opName, operands);
        } else {
          LOG_INFO(Xenon, "  v{} = {}", instr->dest->ordinal, opName);
        }
      } else {
        if (!operands.empty()) {
          LOG_INFO(Xenon, "  {} {}", opName, operands);
        } else {
          LOG_INFO(Xenon, "  {}", opName);
        }
      }
    }

    ++instrCount;
    instr = instr->next;
  }

  LOG_INFO(Xenon, "[HIR Dump]: === End HIR ({} instructions) ===", instrCount);
}

//
// Stub emitters for unused opcodes
//

REGISTER_EMITTER(OPCODE_COMMENT, Emit_COMMENT)
static void Emit_COMMENT(x86CodeGenBackend *, const HIR::Instr *) {
  // Comments are annotation-only, no code emitted.
}

REGISTER_EMITTER(OPCODE_NOP, Emit_NOP)
static void Emit_NOP(x86CodeGenBackend *, const HIR::Instr *) {
  // Intentional no-op.
}

REGISTER_EMITTER(OPCODE_SOURCE_OFFSET, Emit_SOURCE_OFFSET)
static void Emit_SOURCE_OFFSET(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Source offset annotation emitted by PPCTranslator.
  // flags bit 0 = needs full CIA/NIA/CI write (branch or faulting instr)
  // Otherwise only CI is written.
  u64 addr = instr->src1.offset;
  u32 rawWord = instr->currentInstrData.opcode;

  if (instr->flags & 1) {
    // Full prologue: CIA, NIA, CI
    asmjit::x86::Gp tmp = newGP64();
    COMP->mov(tmp, asmjit::Imm(static_cast<int64_t>(addr)));
    COMP->mov(CIAPtr(), tmp);
    COMP->mov(tmp, asmjit::Imm(static_cast<int64_t>(addr + 4)));
    COMP->mov(NIAPtr(), tmp);
    COMP->mov(tmp, asmjit::Imm(static_cast<int64_t>(rawWord)));
    COMP->mov(b->GetThreadContext()->scalar(&sPPUThread::CI).Ptr<u32>(), tmp);
  } else {
    // Minimal prologue: CI only
    asmjit::x86::Gp tmp = newGP64();
    COMP->mov(tmp, asmjit::Imm(static_cast<int64_t>(rawWord)));
    COMP->mov(b->GetThreadContext()->scalar(&sPPUThread::CI).Ptr<u32>(), tmp);
  }
}

REGISTER_EMITTER(OPCODE_CONTEXT_BARRIER, Emit_CONTEXT_BARRIER)
static void Emit_CONTEXT_BARRIER(x86CodeGenBackend *, const HIR::Instr *) {
  // Barrier for optimization passes, no code emitted.
}

REGISTER_EMITTER(OPCODE_SYSCALL, Emit_SYSCALL)
static void Emit_SYSCALL(x86CodeGenBackend *b, const HIR::Instr *instr) {
  x86::Gp exReg = newGP16();
  COMP->mov(exReg, EXPtr());
  COMP->or_(exReg, imm<u16>(ppuSystemCallEx));
  COMP->mov(EXPtr(), exReg);
  COMP->mov(b->GetThreadContext()->scalar(&sPPUThread::exHVSysCall).Ptr(), imm<bool>(instr->currentInstrData.lev & 1));
}

REGISTER_EMITTER(OPCODE_RFID, Emit_RFID)
static void Emit_RFID(x86CodeGenBackend *b, const HIR::Instr *instr) {
  x86::Gp srr1 = newGP64();
  x86::Gp msr = newGP64();

  Label skipHVMESet = newLabel();
  Label use64 = newLabel();

  // MSR[1-2,4-32,37-41,49-50,52-57,60-63] <- SRR1[1-2,4-32,37-41,49-50,52-57,60-63]
  COMP->mov(srr1, SPRPtr(SRR1));
  COMP->mov(msr, srr1);

  // MSR[0] <- SRR1[0] | SRR1[1]

  Label setMSRSF = newLabel();
  Label doneMSRSF = newLabel();

  COMP->bt(srr1, 63); // SRR1[0]
  COMP->jc(setMSRSF);
  COMP->bt(srr1, 62); // SRR1[1]
  COMP->jc(setMSRSF);
  // SRR1[0] || SRR1[1] == 0
  COMP->btr(msr, 63);
  COMP->jmp(doneMSRSF);
  COMP->bind(setMSRSF);
  COMP->bts(msr, 63);
  // Check was done and MSR was set.
  COMP->bind(doneMSRSF);

  // MSR[58] = SRR1[58] | SRR1[49]

  Label setMSRIR = newLabel();
  Label doneMSRIR = newLabel();

  COMP->bt(srr1, 5); // SRR1[58]
  COMP->jc(setMSRIR);
  COMP->bt(srr1, 14); // SRR1[49]
  COMP->jc(setMSRIR);
  // SRR1[58] || SRR1[49] == 0
  COMP->btr(msr, 5);
  COMP->jmp(doneMSRIR);
  COMP->bind(setMSRIR);
  COMP->bts(msr, 5);
  // Check was done and MSR was set.
  COMP->bind(doneMSRIR);

  // MSR[59] = SRR1[59] | SRR1[49]

  Label setMSRDR = newLabel();
  Label doneMSRDR = newLabel();

  COMP->bt(srr1, 4); // SRR1[59]
  COMP->jc(setMSRDR);
  COMP->bt(srr1, 14); // SRR1[49]
  COMP->jc(setMSRDR);
  // SRR1[58] || SRR1[49] == 0
  COMP->btr(msr, 4);
  COMP->jmp(doneMSRDR);
  COMP->bind(setMSRDR);
  COMP->bts(msr, 4);
  // Check was done and MSR was set.
  COMP->bind(doneMSRDR);

  // Check for HV bit in current MSR to skip setting HV and ME bits
  x86::Gp currentMSR = newGP64();
  COMP->mov(currentMSR, SPRPtr(MSR));
  COMP->bt(currentMSR, 60); // MSR.HV
  COMP->jnc(skipHVMESet);   // Jump if not in HV mode.

  Label skipMSRSF = newLabel();
  Label skipMSRME = newLabel();

  // Set MSR[HV]
  COMP->bt(srr1, 60); // SRR1[3]
  COMP->jnc(skipMSRSF);
  COMP->bts(msr, 60);
  COMP->bind(skipMSRSF);
  // Set MSR[ME]
  COMP->bt(srr1, 12); // SRR1[51]
  COMP->jnc(skipMSRME);
  COMP->bts(msr, 12);
  COMP->bind(skipMSRME);

  // Skip setting HV and ME bits if not in HV mode
  COMP->bind(skipHVMESet);
  // Store composed MSR
  COMP->mov(SPRPtr(MSR), msr);

  // NIA <- SRR0 & ~3
  x86::Gp srr0 = newGP64();
  x86::Gp nia = newGP64();
  COMP->mov(srr0, SPRPtr(SRR0));
  COMP->mov(nia, srr0);
  COMP->and_(nia, imm<u64>(~3ULL));
  COMP->mov(NIAPtr(), nia);

  // If MSR.SF == 0 (32-bit mode), truncate NIA to 32 bits
  COMP->bt(msr, 63);  // MSR.SF
  COMP->jc(use64);    // if set -> 64-bit mode, skip truncation
  COMP->and_(nia, imm<u32>(0xFFFFFFFF));
  COMP->mov(NIAPtr(), nia);

  COMP->bind(use64);
}

REGISTER_EMITTER(OPCODE_MTMSRD, Emit_MTMSRD)
static void Emit_MTMSRD(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Source
  x86::Gp rsValue = LoadValueGp(b, instr->src1.value);
  // Get MSR
  x86::Gp msrValue = newGP64();
  COMP->mov(msrValue, SPRPtr(MSR));

  if (instr->currentInstrData.l15) {
    /*
       Bits 48 and 62 of register RS are placed into the corresponding bits
       of the MSR. The remaining bits of the MSR are unchanged
    */

    Label skipMSREE = newLabel();
    Label skipMSRRI = newLabel();

    // Bit 48 = MSR[EE]

    // Clear MSR
    COMP->btr(msrValue, imm(63 - 48));  // 48
    // Check if rS has the bit set
    COMP->bt(rsValue, imm(63 - 48));    // 48
    // Jump otherwise
    COMP->jnc(skipMSREE);
    // Set the MSR bit
    COMP->bts(msrValue, imm(63 - 48));
    COMP->bind(skipMSREE);


    // Bit 62 = MSR[RI]
    COMP->btr(msrValue, imm(63 - 62));  // 62
    COMP->bt(rsValue, imm(63 - 62));    // 62
    COMP->jnc(skipMSRRI);
    COMP->bts(msrValue, imm(63 - 62));
    COMP->bind(skipMSRRI);

    // Store back MSR
    COMP->mov(SPRPtr(MSR), msrValue);
  } else {
    /*
       The result of ORing bits 00 and 01 of register RS is placed into MSR0
       The result of ORing bits 48 and 49 of register RS is placed into MSR48
       The result of ORing bits 58 and 49 of register RS is placed into MSR58
       The result of ORing bits 59 and 49 of register RS is placed into MSR59
       Bits 1:2, 4:47, 49:50, 52:57, and 60:63 of register RS are placed into
       the corresponding bits of the MSR
    */

    // MSR = rS
    COMP->mov(msrValue, rsValue);

    // MSR0 = (RS)0 | (RS)1

    Label setMSRSF = newLabel();
    Label doneMSRSF = newLabel();

    COMP->bt(rsValue, imm(63 - 0));
    COMP->jc(setMSRSF);
    COMP->bt(rsValue, imm(63 - 1));
    COMP->jc(setMSRSF);
    COMP->btr(msrValue, imm(63 - 0));
    COMP->jmp(doneMSRSF);
    COMP->bind(setMSRSF);
    COMP->bts(msrValue, imm(63 - 0));
    // Check was done and MSR was set.
    COMP->bind(doneMSRSF);

    // MSR48 = (RS)48 | (RS)49

    Label setMSREE = newLabel();
    Label doneMSREE = newLabel();

    COMP->bt(rsValue, imm(63 - 48));
    COMP->jc(setMSREE);
    COMP->bt(rsValue, imm(63 - 49));
    COMP->jc(setMSREE);
    COMP->btr(msrValue, imm(63 - 48));
    COMP->jmp(doneMSREE);
    COMP->bind(setMSREE);
    COMP->bts(msrValue, imm(63 - 48));
    // Check was done and MSR was set.
    COMP->bind(doneMSREE);

    // MSR58 = (RS)58 | (RS)49

    Label setMSRIR = newLabel();
    Label doneMSRIR = newLabel();

    COMP->bt(rsValue, imm(63 - 58));
    COMP->jc(setMSRIR);
    COMP->bt(rsValue, imm(63 - 49));
    COMP->jc(setMSRIR);
    COMP->btr(msrValue, imm(63 - 58));
    COMP->jmp(doneMSRIR);
    COMP->bind(setMSRIR);
    COMP->bts(msrValue, imm(63 - 58));
    // Check was done and MSR was set.
    COMP->bind(doneMSRIR);

    // MSR59 = (RS)59 | (RS)49

    Label setMSRDR = newLabel();
    Label doneMSRDR = newLabel();

    COMP->bt(rsValue, imm(63 - 59));
    COMP->jc(setMSRDR);
    COMP->bt(rsValue, imm(63 - 49));
    COMP->jc(setMSRDR);
    COMP->btr(msrValue, imm(63 - 59));
    COMP->jmp(doneMSRDR);
    COMP->bind(setMSRDR);
    COMP->bts(msrValue, imm(63 - 59));
    // Check was done and MSR was set.
    COMP->bind(doneMSRDR);

    // Store back MSR
    COMP->mov(SPRPtr(MSR), msrValue);
  }
}

REGISTER_EMITTER(OPCODE_SYNC_EXCEPTION_CHECK, Emit_SYNC_EXCEPTION_CHECK)
  static void Emit_SYNC_EXCEPTION_CHECK(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Test for present exceptions and return if any is found.
  Label skipRet = newLabel();
  x86::Gp exceptReg = newGP16();
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm(SyncExceptionMask));
  COMP->test(exceptReg, exceptReg);
  COMP->jz(skipRet);
  COMP->ret();
  COMP->bind(skipRet);
}

REGISTER_EMITTER(OPCODE_MEMORY_BARRIER, Emit_MEMORY_BARRIER)
  static void Emit_MEMORY_BARRIER(x86CodeGenBackend *b, const HIR::Instr *instr) {
  COMP->mfence();
}

REGISTER_EMITTER(OPCODE_MFSPR, Emit_MFSPR)
  static void Emit_MFSPR(x86CodeGenBackend *b, const HIR::Instr *instr) {
  u32 sprNum = instr->currentInstrData.spr;
  sprNum = ((sprNum & 0x1F) << 5) | ((sprNum >> 5) & 0x1F);

  x86::Gp rSValue = newGP64();

  switch (static_cast<eXenonSPR>(sprNum)) {
  case eXenonSPR::XER:
    COMP->mov(rSValue, SPRPtr(XER));
    break;
  case eXenonSPR::LR:
    COMP->mov(rSValue, SPRPtr(LR));
    break;
  case eXenonSPR::CTR:
    COMP->mov(rSValue, SPRPtr(CTR));
    break;
  case eXenonSPR::DSISR:
    COMP->mov(rSValue, SPRPtr(DSISR));
    break;
  case eXenonSPR::DAR:
    COMP->mov(rSValue, SPRPtr(DAR));
    break;
  case eXenonSPR::DEC:
    COMP->mov(rSValue, SPRPtr(DEC));
    break;
  case eXenonSPR::SDR1:
    COMP->mov(rSValue, SharedSPRPtr(SDR1));
    break;
  case eXenonSPR::SRR0:
    COMP->mov(rSValue, SPRPtr(SRR0));
    break;
  case eXenonSPR::SRR1:
    COMP->mov(rSValue, SPRPtr(SRR1));
    break;
  case eXenonSPR::CFAR:
    COMP->mov(rSValue, SPRPtr(CFAR));
    break;
  case eXenonSPR::CTRLRD:
    COMP->mov(rSValue, SharedSPRPtr(CTRL));
    break;
  case eXenonSPR::VRSAVE:
    COMP->mov(rSValue, SPRPtr(VRSAVE));
    break;
  case eXenonSPR::TBLRO:
    COMP->mov(rSValue, 0x00000000FFFFFFFF);
    COMP->and_(rSValue, SharedSPRPtr(TB));
    break;
  case eXenonSPR::TBURO:
    COMP->mov(rSValue, 0xFFFFFFFF00000000);
    COMP->and_(rSValue, SharedSPRPtr(TB));
    break;
  case eXenonSPR::SPRG0:
    COMP->mov(rSValue, SPRPtr(SPRG0));
    break;
  case eXenonSPR::SPRG1:
    COMP->mov(rSValue, SPRPtr(SPRG1));
    break;
  case eXenonSPR::SPRG2:
    COMP->mov(rSValue, SPRPtr(SPRG2));
    break;
  case eXenonSPR::SPRG3:
    COMP->mov(rSValue, SPRPtr(SPRG3));
    break;
  case eXenonSPR::PVR:
    COMP->mov(rSValue, SharedSPRPtr(PVR));
    break;
  case eXenonSPR::HSPRG0:
    COMP->mov(rSValue, SPRPtr(HSPRG0));
    break;
  case eXenonSPR::HSPRG1:
    COMP->mov(rSValue, SPRPtr(HSPRG1));
    break;
  case eXenonSPR::RMOR:
    COMP->mov(rSValue, SharedSPRPtr(RMOR));
    break;
  case eXenonSPR::HRMOR:
    COMP->mov(rSValue, SharedSPRPtr(HRMOR));
    break;
  case eXenonSPR::LPCR:
    COMP->mov(rSValue, SharedSPRPtr(LPCR));
    break;
  case eXenonSPR::TSCR:
    COMP->mov(rSValue, SharedSPRPtr(TSCR));
    break;
  case eXenonSPR::TTR:
    COMP->mov(rSValue, SharedSPRPtr(TTR));
    break;
  case eXenonSPR::PPE_TLB_Index_Hint:
    COMP->mov(rSValue, SPRPtr(PPE_TLB_Index_Hint));
    break;
  case eXenonSPR::HID0:
    COMP->mov(rSValue, SharedSPRPtr(HID0));
    break;
  case eXenonSPR::HID1:
    COMP->mov(rSValue, SharedSPRPtr(HID1));
    break;
  case eXenonSPR::HID4:
    COMP->mov(rSValue, SharedSPRPtr(HID4));
    break;
  case eXenonSPR::DABR:
    COMP->mov(rSValue, SPRPtr(DABR));
    break;
  case eXenonSPR::HID6:
    COMP->mov(rSValue, SharedSPRPtr(HID6));
    break;
  case eXenonSPR::PIR:
    COMP->mov(rSValue, SPRPtr(PIR));
    break;
  default:
    break;
  }


  TagStoreReg(instr->dest, rSValue);
}


void JITHelper_MTCTRLWR(sPPEState *ppeState) {
  uCTRL newCTRL;
  newCTRL.hexValue = static_cast<u32>(GPRi(rd));
  if (curThreadId == ePPUThread_Zero) {
    // Thread Zero
    if (ppeState->SPR.CTRL.TE1) {
      // TE1 is set, do not modify it.
      newCTRL.TE1 = 1;
    }
  } else {
    // Thread One
    if (ppeState->SPR.CTRL.TE0) {
      // TE0 is set, do not modify it.
      newCTRL.TE0 = 1;
    }
  }

  // TODO: Check this, reversing and docs suggests this is the correct behavior.
  // If a thread is being enabled, we must generate a reset interrupt on said thread.
  if (ppeState->SPR.CTRL.TE0 == 0 && newCTRL.TE0) { ppeState->ppuThread[0].exceptReg |= ppuSystemResetEx; }
  if (ppeState->SPR.CTRL.TE1 == 0 && newCTRL.TE1) { ppeState->ppuThread[1].exceptReg |= ppuSystemResetEx; }

  LOG_TRACE(Xenon, "{} (Thread{:#d}): Setting ctrl to {:#x}", ppeState->ppuName, (u8)curThreadId, newCTRL.hexValue);

  ppeState->SPR.CTRL = newCTRL;
}

REGISTER_EMITTER(OPCODE_MTSPR, Emit_MTSPR)
  static void Emit_MTSPR(x86CodeGenBackend *b, const HIR::Instr *instr) {
  u32 sprNum = instr->currentInstrData.spr;
  sprNum = ((sprNum & 0x1F) << 5) | ((sprNum >> 5) & 0x1F);

  x86::Gp rSValue = LoadValueGp(b, instr->src1.value);

  switch (static_cast<eXenonSPR>(sprNum)) {
  case eXenonSPR::XER:
    // Clear the unused bits in XER (35:56)
    COMP->and_(rSValue, imm(0xE000007F));
    COMP->mov(SPRPtr(XER), rSValue);
    break;
  case eXenonSPR::LR:
    COMP->mov(SPRPtr(LR), rSValue);
    break;
  case eXenonSPR::CTR:
    COMP->mov(SPRPtr(CTR), rSValue);
    break;
  case eXenonSPR::DSISR:
    COMP->mov(SPRPtr(DSISR), rSValue);
    break;
  case eXenonSPR::DAR:
    COMP->mov(SPRPtr(DAR), rSValue);
    break;
  case eXenonSPR::DEC:
    COMP->mov(SPRPtr(DEC), rSValue.r32());
    break;
  case eXenonSPR::SDR1:
    COMP->mov(SharedSPRPtr(SDR1), rSValue);
    break;
  case eXenonSPR::SRR0:
    COMP->mov(SPRPtr(SRR0), rSValue);
    break;
  case eXenonSPR::SRR1:
    COMP->mov(SPRPtr(SRR1), rSValue);
    break;
  case eXenonSPR::CFAR:
    COMP->mov(SPRPtr(CFAR), rSValue);
    break;
  case eXenonSPR::CTRLWR: {
    // Invoke a helper to do the CTRL reg setting.
    InvokeNode *ctrlwrInv = nullptr;
    Xe::JITCompat::Invoke(COMP, ctrlwrInv, imm((void *)JITHelper_MTCTRLWR), FuncSignature::build<void, sPPEState *>());
    Xe::JITCompat::SetArg(ctrlwrInv, 0, b->ppeState->Base());
  } break;
  case eXenonSPR::VRSAVE:
    COMP->mov(SPRPtr(VRSAVE), rSValue);
    break;
  case eXenonSPR::SPRG0:
    COMP->mov(SPRPtr(SPRG0), rSValue);
    break;
  case eXenonSPR::SPRG1:
    COMP->mov(SPRPtr(SPRG1), rSValue);
    break;
  case eXenonSPR::SPRG2:
    COMP->mov(SPRPtr(SPRG2), rSValue);
    break;
  case eXenonSPR::SPRG3:
    COMP->mov(SPRPtr(SPRG3), rSValue);
    break;
  case eXenonSPR::TBLWO: {
    // TODO: Properly fix this
    x86::Gp tmp = newGP64();
    //COMP->mov(tmp, SharedSPRPtr(TB));
    //COMP->and_(rSValue, imm(0x00000000FFFFFFFF));
    //COMP->and_(tmp, imm(0xFFFFFFFF00000000));
    //COMP->or_(rSValue, tmp);
    COMP->mov(SharedSPRPtr(TB), rSValue);
  } break;
  case eXenonSPR::TBUWO: {
    // TODO: Properly fix this
    x86::Gp tmp = newGP64();
    //COMP->mov(tmp, SharedSPRPtr(TB));
    //COMP->and_(rSValue, imm(0x00000000FFFFFFFF));
    //COMP->shl(rSValue, 32);
    //COMP->and_(tmp, imm(0x00000000FFFFFFFF));
    //COMP->or_(rSValue, tmp);
    COMP->mov(SharedSPRPtr(TB), rSValue);
  } break;
  case eXenonSPR::HSPRG0:
    COMP->mov(SPRPtr(HSPRG0), rSValue);
    break;
  case eXenonSPR::HSPRG1:
    COMP->mov(SPRPtr(HSPRG1), rSValue);
    break;
  case eXenonSPR::HDEC:
    COMP->mov(SharedSPRPtr(HDEC), rSValue.r32());
    break;
  case eXenonSPR::RMOR:
    COMP->mov(SharedSPRPtr(RMOR), rSValue);
    break;
  case eXenonSPR::HRMOR:
    COMP->mov(SharedSPRPtr(HRMOR), rSValue);
    break;
  case eXenonSPR::LPCR:
    COMP->mov(SharedSPRPtr(LPCR), rSValue);
    break;
  case eXenonSPR::LPIDR:
    COMP->mov(SharedSPRPtr(LPIDR), rSValue.r32());
    break;
  case eXenonSPR::TSCR:
    COMP->mov(SharedSPRPtr(TSCR), rSValue.r32());
    break;
  case eXenonSPR::TTR:
    COMP->mov(SharedSPRPtr(TTR), rSValue);
    break;
  case eXenonSPR::PPE_TLB_Index:
    COMP->mov(SharedSPRPtr(PPE_TLB_Index), rSValue);
    break;
  case eXenonSPR::PPE_TLB_Index_Hint:
    COMP->mov(SPRPtr(PPE_TLB_Index_Hint), rSValue);
    break;
  case eXenonSPR::PPE_TLB_VPN: {
    COMP->mov(SharedSPRPtr(PPE_TLB_VPN), rSValue);
    // Add new tlb entry via function call
    InvokeNode *addTLBEntryInv = nullptr;
    Xe::JITCompat::Invoke(COMP, addTLBEntryInv, imm((void *)PPCInterpreter::mmuAddTlbEntry), FuncSignature::build<void, sPPEState *>());
    Xe::JITCompat::SetArg(addTLBEntryInv, 0, b->ppeState->Base());
  } break;
  case eXenonSPR::PPE_TLB_RPN:
    COMP->mov(SharedSPRPtr(PPE_TLB_RPN), rSValue);
    break;
  case eXenonSPR::HID0:
    COMP->mov(SharedSPRPtr(HID0), rSValue);
    break;
  case eXenonSPR::HID1:
    COMP->mov(SharedSPRPtr(HID1), rSValue);
    break;
  case eXenonSPR::HID4:
    COMP->mov(SharedSPRPtr(HID4), rSValue);
    break;
  case eXenonSPR::HID6:
    COMP->mov(SharedSPRPtr(HID6), rSValue);
    break;
  case eXenonSPR::DABR:
    COMP->mov(SPRPtr(DABR), rSValue);
    break;
  case eXenonSPR::DABRX:
    COMP->mov(SPRPtr(DABRX), rSValue);
    break;
  default:
    break;
  }
}

REGISTER_EMITTER(OPCODE_CALL_HLE_FUNCTION, Emit_CALL_HLE_FUNCTION)
  static void Emit_CALL_HLE_FUNCTION(x86CodeGenBackend *b, const HIR::Instr *instr) {
  void *fnPtr = reinterpret_cast<void *>(instr->src1.offset);
  InvokeNode *hleInv = nullptr;
  Xe::JITCompat::Invoke(COMP, hleInv, imm(fnPtr), FuncSignature::build<int, void *>());
  Xe::JITCompat::SetArg(hleInv, 0, b->ppeState->Base());
}

//
// Call Interpreter (system instruction fallback)
//

// Stores the raw PPC instruction into sPPUThread::CI, then invokes
// the interpreter function with sPPEState* as its argument.
REGISTER_EMITTER(OPCODE_CALL_INTERPRETER, Emit_CALL_INTERPRETER)
static void Emit_CALL_INTERPRETER(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // 1. Store the raw instruction data into sPPUThread::CI
  u32 rawInstr = instr->currentInstrData.opcode;
  COMP->mov(b->GetThreadContext()->scalar(&sPPUThread::CI).Ptr<u32>(), asmjit::Imm(rawInstr));

  // 2. Load the interpreter function pointer into a register
  x86::Gp fnReg = newGP64();
  COMP->mov(fnReg, asmjit::Imm(static_cast<int64_t>(instr->src1.offset)));

  // 3. Invoke the interpreter function: void(sPPEState*)
  asmjit::InvokeNode *invokeNode = nullptr;
  Xe::JITCompat::Invoke(COMP, invokeNode, fnReg,
    asmjit::FuncSignature::build<void, void *>());

  // Pass sPPEState* as the first argument
  Xe::JITCompat::SetArg(invokeNode, 0, b->ppeState->Base());
}

//
// Trap
//

// Unconditional trap: set program exception and trap type.
// sig: X  (no dest, no src, trapCode in instr->flags)
REGISTER_EMITTER(OPCODE_TRAP, Emit_TRAP)
static void Emit_TRAP(x86CodeGenBackend *b, const HIR::Instr *instr) {
  x86::Gp exceptReg = newGP16();
  COMP->mov(exceptReg, EXPtr());
  COMP->or_(exceptReg, asmjit::Imm(ppuProgramEx));
  COMP->mov(EXPtr(), exceptReg);
  x86::Gp trapType = newGP16();
  COMP->mov(trapType, asmjit::Imm(ppuProgExTypeTRAP));
  COMP->mov(b->GetThreadContext()->scalar(&sPPUThread::progExceptionType), trapType);
}

// Conditional trap: if cond != 0, set program exception and trap type.
// sig: X_V  (no dest, src1 = condition, trapCode in instr->flags)
REGISTER_EMITTER(OPCODE_TRAP_TRUE, Emit_TRAP_TRUE)
static void Emit_TRAP_TRUE(x86CodeGenBackend *b, const HIR::Instr *instr) {
  x86::Gp cond = LoadValueGp(b, instr->src1.value);
  Label end = newLabel();

  COMP->test(cond.r8(), cond.r8());
  COMP->jz(end);

  // Trap: set program exception
  x86::Gp exceptReg = newGP16();
  COMP->mov(exceptReg, EXPtr());
  COMP->or_(exceptReg, asmjit::Imm(ppuProgramEx));
  COMP->mov(EXPtr(), exceptReg);
  x86::Gp trapType = newGP16();
  COMP->mov(trapType, asmjit::Imm(ppuProgExTypeTRAP));
  COMP->mov(b->GetThreadContext()->scalar(&sPPUThread::progExceptionType), trapType);

  COMP->bind(end);
}

//
// Context Load/Store
//

// Builds a raw memory operand at ctx-base + fieldOffset, sized for T.
// Avoids ASMJitPtr<u8>, whose member-pointer-typed helpers
// (scalar/array/substruct/member) don't make sense for a byte-granular
// field and fail to instantiate.
template <typename T>
static asmjit::x86::Mem ContextFieldMem(x86CodeGenBackend *b, u64 fieldOffset) {
  return asmjit::x86::ptr(b->GetThreadContext()->Base(),
                          b->GetThreadContext()->Offset() + fieldOffset, sizeof(T));
}

REGISTER_EMITTER(OPCODE_LOAD_CONTEXT, Emit_LOAD_CONTEXT)
static void Emit_LOAD_CONTEXT(x86CodeGenBackend *b, const HIR::Instr *instr) {
  u64 offset = instr->src1.offset;
  auto *comp = b->GetCompiler();

  switch (instr->dest->type) {
  case HIR::INT8_TYPE: {
    asmjit::x86::Gp dst = newGP8();
    comp->mov(dst, ContextFieldMem<u8>(b, offset));
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT16_TYPE: {
    asmjit::x86::Gp dst = newGP16();
    comp->mov(dst, ContextFieldMem<u16>(b, offset));
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT32_TYPE: {
    asmjit::x86::Gp dst = newGP32();
    comp->mov(dst, ContextFieldMem<u32>(b, offset));
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::INT64_TYPE: {
    asmjit::x86::Gp dst = newGP64();
    comp->mov(dst, ContextFieldMem<u64>(b, offset));
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::FLOAT32_TYPE: {
    asmjit::x86::Vec dst = newXMM();
    comp->vmovss(dst, ContextFieldMem<f32>(b, offset));
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::FLOAT64_TYPE: {
    asmjit::x86::Vec dst = newXMM();
    comp->vmovsd(dst, ContextFieldMem<f64>(b, offset));
    TagStoreReg(instr->dest, dst);
  } break;
  case HIR::VEC128_TYPE: {
    asmjit::x86::Vec dst = newXMM();
    comp->vmovdqu(dst, ContextFieldMem<u128>(b, offset));
    TagStoreReg(instr->dest, dst);
  } break;
  default:
    LOG_WARNING(Xenon, "[x86Backend]: LOAD_CONTEXT unsupported type {}", static_cast<int>(instr->dest->type));
    break;
  }
}

REGISTER_EMITTER(OPCODE_STORE_CONTEXT, Emit_STORE_CONTEXT)
static void Emit_STORE_CONTEXT(x86CodeGenBackend *b, const HIR::Instr *instr) {
  u64 offset = instr->src1.offset;
  HIR::Value *val = instr->src2.value;
  auto *comp = b->GetCompiler();

  if (val->IsConstant()) {
    // Store immediate constant directly to context memory.
    switch (val->type) {
    case HIR::INT8_TYPE:
      comp->mov(ContextFieldMem<u8>(b, offset), asmjit::Imm(val->constant.u8));
      break;
    case HIR::INT16_TYPE:
      comp->mov(ContextFieldMem<u16>(b, offset), asmjit::Imm(val->constant.u16));
      break;
    case HIR::INT32_TYPE:
      comp->mov(ContextFieldMem<u32>(b, offset), asmjit::Imm(val->constant.u32));
      break;
    case HIR::INT64_TYPE: {
      asmjit::x86::Gp tmp = newGP64();
      comp->mov(tmp, asmjit::Imm(val->constant.i64));
      comp->mov(ContextFieldMem<u64>(b, offset), tmp);
    } break;
    case HIR::VEC128_TYPE: {
      // Store each 32-bit dword of the 128-bit vector constant individually.
      asmjit::x86::Gp tmp = newGP32();
      for (u32 i = 0; i < 4; i++) {
        comp->mov(tmp, asmjit::Imm(val->constant.v128.dsword[i]));
        comp->mov(ContextFieldMem<u32>(b, offset + i * sizeof(u32)), tmp);
      }
    } break;
    default:
      LOG_WARNING(Xenon, "[x86Backend]: STORE_CONTEXT constant type {} not yet handled",
        static_cast<int>(val->type));
      break;
    }
    return;
  }

  // Value produced by a prior instruction — recover the virtual register.
  switch (val->type) {
  case HIR::INT8_TYPE: {
    asmjit::x86::Gp src = TagLoadGp(val);
    comp->mov(ContextFieldMem<u8>(b, offset), src.r8());
  } break;
  case HIR::INT16_TYPE: {
    asmjit::x86::Gp src = TagLoadGp(val);
    comp->mov(ContextFieldMem<u16>(b, offset), src.r16());
  } break;
  case HIR::INT32_TYPE: {
    asmjit::x86::Gp src = TagLoadGp(val);
    comp->mov(ContextFieldMem<u32>(b, offset), src.r32());
  } break;
  case HIR::INT64_TYPE: {
    asmjit::x86::Gp src = TagLoadGp(val);
    comp->mov(ContextFieldMem<u64>(b, offset), src.r64());
  } break;
  case HIR::FLOAT32_TYPE: {
    asmjit::x86::Vec src = TagLoadXmm(val);
    comp->vmovss(ContextFieldMem<f32>(b, offset), src);
  } break;
  case HIR::FLOAT64_TYPE: {
    asmjit::x86::Vec src = TagLoadXmm(val);
    comp->vmovsd(ContextFieldMem<f64>(b, offset), src);
  } break;
  case HIR::VEC128_TYPE: {
    asmjit::x86::Vec src = TagLoadXmm(val);
    comp->vmovdqu(ContextFieldMem<u128>(b, offset), src);
  } break;
  default:
    LOG_WARNING(Xenon, "[x86Backend]: STORE_CONTEXT SSA type {} not yet handled",
      static_cast<int>(val->type));
    break;
  }
}

REGISTER_EMITTER(OPCODE_LOAD_TIME_BASE, Emit_LOAD_TIME_BASE)
  static void Emit_LOAD_TIME_BASE(x86CodeGenBackend *b, const HIR::Instr *instr) {
  // Loads time base register
  x86::Gp timeBase = newGP64();
  COMP->mov(timeBase, SharedSPRPtr(TB));
  TagStoreReg(instr->dest, timeBase);
}

//
// Branch instructions
// currentInstrData carries the raw PPC instruction (bo/bi/aa/lk/li/ds), same
// as the interpreter and legacy x86 JIT backend use.
//

// Truncates NIA (and LR, if just written) to 32 bits when MSR.SF == 0.
static void TruncateNIAIf32Bit(x86CodeGenBackend *b) {
  Label use64 = newLabel();
  x86::Gp msr = newGP64();
  COMP->mov(msr, SPRPtr(MSR));
  COMP->bt(msr, 63); // MSR.SF
  COMP->jc(use64);
  x86::Gp tmp32 = newGP32();
  COMP->mov(tmp32, NIAPtr());
  COMP->and_(tmp32, imm<u32>(0xFFFFFFFF));
  COMP->mov(NIAPtr(), tmp32);
  COMP->bind(use64);
}

// Unconditional branch (b/ba/bl/bla).
REGISTER_EMITTER(OPCODE_BRANCH, Emit_BRANCH)
static void Emit_BRANCH(x86CodeGenBackend *b, const HIR::Instr *instr) {
  const uPPCInstr &ppc = instr->currentInstrData;

  x86::Gp CIA = newGP64();
  x86::Gp target = newGP64();
  COMP->mov(CIA, CIAPtr());

  s32 offset = EXTS(ppc.li, 24) << 2;
  if (ppc.aa) {
    COMP->mov(target, offset);
  } else {
    COMP->mov(target, CIA);
    COMP->add(target, offset);
  }
  COMP->mov(NIAPtr(), target);

  if (ppc.lk) {
    COMP->add(CIA, imm<u32>(4));
    COMP->mov(LRPtr(), CIA);
  }

  TruncateNIAIf32Bit(b);
}

// Branch Conditional (bc/bca/bcl/bcla).
REGISTER_EMITTER(OPCODE_BRANCH_CONDITIONAL, Emit_BRANCH_CONDITIONAL)
static void Emit_BRANCH_CONDITIONAL(x86CodeGenBackend *b, const HIR::Instr *instr) {
  const uPPCInstr &ppc = instr->currentInstrData;

  Label fail = newLabel();

  // If BO[2] == 0 then CTR -= 1.
  if ((ppc.bo & 0x4) == 0) {
    x86::Gp ctr = newGP64();
    COMP->mov(ctr, SPRPtr(CTR));
    COMP->sub(ctr, imm<u64>(1));
    COMP->mov(SPRPtr(CTR), ctr);
  }

  // CTR condition.
  if (!(ppc.bo & 0x4)) {
    x86::Gp ctr = newGP64();
    COMP->mov(ctr, SPRPtr(CTR));
    COMP->test(ctr, ctr);
    if (ppc.bo & 0x2) {
      COMP->jne(fail); // BO[1] == 1 -> branch if CTR == 0
    } else {
      COMP->je(fail);  // BO[1] == 0 -> branch if CTR != 0
    }
  }

  // CR condition.
  if (!(ppc.bo & 0x10)) {
    x86::Gp crVal = newGP32();
    x86::Gp tmp = newGP32();
    COMP->mov(crVal, CRValPtr());
    const u32 shift = 31 - ppc.bi;
    COMP->mov(tmp, crVal);
    COMP->shr(tmp, imm(shift));
    COMP->and_(tmp, imm(1));
    COMP->test(tmp, tmp);
    if (ppc.bo & 0x8) {
      COMP->je(fail); // expect CR bit == 1
    } else {
      COMP->jne(fail); // expect CR bit == 0
    }
  }

  // Conditions passed: compute target and set NIA.
  x86::Gp CIA = newGP64();
  x86::Gp target = newGP64();
  COMP->mov(CIA, CIAPtr());

  s32 offset = EXTS(ppc.ds, 14) << 2;
  if (ppc.aa) {
    COMP->mov(target, offset);
  } else {
    COMP->mov(target, CIA);
    COMP->add(target, offset);
  }
  COMP->mov(NIAPtr(), target);

  if (ppc.lk) {
    COMP->add(CIA, imm<u32>(4));
    COMP->mov(LRPtr(), CIA);
  }

  TruncateNIAIf32Bit(b);
  COMP->bind(fail);
}

// Branch Conditional to Count Register (bcctr/bcctrl).
REGISTER_EMITTER(OPCODE_BRANCH_CONDITIONAL_CTR, Emit_BRANCH_CONDITIONAL_CTR)
static void Emit_BRANCH_CONDITIONAL_CTR(x86CodeGenBackend *b, const HIR::Instr *instr) {
  const uPPCInstr &ppc = instr->currentInstrData;

  Label condTrue = newLabel();
  Label condEnd = newLabel();

  if (ppc.bo & 0x10) {
    COMP->jmp(condTrue);
  } else {
    x86::Gp crVal = newGP32();
    x86::Gp tmp = newGP32();
    COMP->mov(crVal, CRValPtr());
    const u32 shift = 31 - ppc.bi;
    COMP->mov(tmp, crVal);
    COMP->shr(tmp, imm(shift));
    COMP->and_(tmp, imm(1));
    COMP->test(tmp, tmp);
    if (ppc.bo & 0x8) {
      COMP->jne(condTrue); // branch if CR bit == 1
    } else {
      COMP->je(condTrue);  // branch if CR bit == 0
    }
    COMP->jmp(condEnd);
  }

  COMP->bind(condTrue);

  x86::Gp ctr = newGP64();
  COMP->mov(ctr, SPRPtr(CTR));
  COMP->and_(ctr, imm<u64>(~3ULL));
  COMP->mov(NIAPtr(), ctr);

  if (ppc.lk) {
    x86::Gp CIA = newGP64();
    COMP->mov(CIA, CIAPtr());
    COMP->add(CIA, imm<u32>(4));
    COMP->mov(LRPtr(), CIA);
  }

  TruncateNIAIf32Bit(b);
  COMP->bind(condEnd);
}

REGISTER_EMITTER(OPCODE_BRANCH_CONDITIONAL_LR, Emit_BRANCH_CONDITIONAL_LR)
static void Emit_BRANCH_CONDITIONAL_LR(x86CodeGenBackend *b, const HIR::Instr *instr) {
  const uPPCInstr &ppc = instr->currentInstrData;

  Label condTrue = newLabel();
  Label condEnd = newLabel();

  // If BO[2] == 0 then CTR -= 1.
  if ((ppc.bo & 0x4) == 0) {
    x86::Gp ctrDec = newGP64();
    COMP->mov(ctrDec, SPRPtr(CTR));
    COMP->sub(ctrDec, imm<u64>(1));
    COMP->mov(SPRPtr(CTR), ctrDec);
  }

  // CTR condition.
  if (!(ppc.bo & 0x4)) {
    x86::Gp ctrChk = newGP64();
    COMP->mov(ctrChk, SPRPtr(CTR));
    COMP->test(ctrChk, ctrChk);
    if (ppc.bo & 0x2) {
      COMP->jne(condEnd); // BO[1] == 1 -> branch when CTR == 0
    } else {
      COMP->je(condEnd);  // BO[1] == 0 -> branch when CTR != 0
    }
  }

  // CR condition.
  if (!(ppc.bo & 0x10)) {
    x86::Gp crVal = newGP32();
    x86::Gp tmp = newGP32();
    COMP->mov(crVal, CRValPtr());
    const u32 shift = 31 - ppc.bi;
    COMP->mov(tmp, crVal);
    COMP->shr(tmp, imm(shift));
    COMP->and_(tmp, imm(1));
    COMP->test(tmp, tmp);
    if (ppc.bo & 0x8) {
      COMP->je(condEnd);  // expect CR bit == 1
    } else {
      COMP->jne(condEnd); // expect CR bit == 0
    }
  }

  // Conditions passed.
  COMP->bind(condTrue);

  x86::Gp lr = newGP64();
  COMP->mov(lr, SPRPtr(LR));
  COMP->and_(lr, imm<u64>(~3ULL));
  COMP->mov(NIAPtr(), lr);

  if (ppc.lk) {
    x86::Gp CIA = newGP64();
    COMP->mov(CIA, CIAPtr());
    COMP->add(CIA, imm<u32>(4));
    COMP->mov(LRPtr(), CIA);
  }

  TruncateNIAIf32Bit(b);
  COMP->bind(condEnd);
}

//
// x86CodeGenBackend implementation
//

x86CodeGenBackend::x86CodeGenBackend(asmjit::JitRuntime *runtime, std::mutex *runtimeMutex)
  : jitRuntime(runtime), jitRuntimeMutex(runtimeMutex) {
  emittersTable.fill(nullptr);
}

bool x86CodeGenBackend::Initialize() {
  BuildEmitTable();
  return true;
}

void x86CodeGenBackend::BuildEmitTable() {
  emittersTable.fill(nullptr);
  for (const auto &key : GetEmitterKeyList()) {
    if (static_cast<u32>(key.opcode) < emittersTable.size()) {
      emittersTable[key.opcode] = key.handler;
    }
  }

  u32 registered = 0;
  for (auto fn : emittersTable) { if (fn) registered++; }
  LOG_DEBUG(Xenon, "[x86Backend]: {} / {} HIR opcodes registered",
    registered, (u32)HIR::__OPCODE_MAX_VALUE);
}

// Emits a signle HIR instruction as host code.
void x86CodeGenBackend::EmitInstruction(const HIR::Instr *instr) {
  if (!instr->opcode) return;

  HIR::Opcode op = instr->opcode->num;

  // Check if the opcode is within the bounds of the emitters table before dispatching.
  if (static_cast<u32>(op) >= emittersTable.size()) {
    LOG_WARNING(Xenon, "[x86Backend]: Opcode {} out of range", static_cast<u32>(op));
    return;
  }

  // Get emitter from the table
  x86HIREmitFn fn = emittersTable[op];

  // Execute emitter
  if (fn) {
    fn(this, instr);
  } else {
    // Unimplemented, skip silently for ignorable ops, warn for others.
    if (!(instr->opcode->flags & HIR::OPCODE_FLAG_IGNORE)) {
      LOG_WARNING(Xenon, "[x86Backend]: No emitter for HIR opcode '{}' ({})",
        instr->opcode->name ? instr->opcode->name : "???", static_cast<u32>(op));
    }
  }
}

// Emit code for a full HIR block.
bool x86CodeGenBackend::EmitBlock(HIR::HIRBlock *block, void **outCode, u64 *outCodeSize,
                                  ePPUThreadID threadId) {
  if (!block || !outCode || !outCodeSize)
    return false;

  *outCode = nullptr;
  *outCodeSize = 0;

  // Set up asmjit code holder for this block.
  codeHolder.reset();
  codeHolder.init(jitRuntime->environment(), jitRuntime->cpu_features());

  // Set logger if we need to do debug print
  if (printGeneratedCode) {
    codeHolder.set_logger(&asmLogger);
  }

  // Init compiler
  asmjit::x86::Compiler comp(&codeHolder);
  compiler = &comp;

  // Function signature: void(PPU*, sPPEState*, bool)
  // Matches JITFunc so HIR blocks can be called from the dispatch loop.
  // Arg0 (PPU*): received but unused by HIR code.
  // Arg1 (sPPEState*): used to derive thread context.
  // Arg2 (bool enableHalt): received but unused for now.
  asmjit::FuncNode *funcNode = nullptr;
  asmjit::Error funNodeInitResult = comp.add_func_node(Out(funcNode), asmjit::FuncSignature::build<void, PPU *, sPPEState *, bool>());
  if (funNodeInitResult != asmjit::Error::kOk || !funcNode) {
    LOG_ERROR(Xenon, "[x86Backend]: addFuncNode failed: {}", asmjit::DebugUtils::error_as_string(funNodeInitResult));
    compiler = nullptr;
    return false;
  }

  asmjit::x86::Gp ppuArg = Xe::JITCompat::NewGPZ(&comp, "ppu_unused");
  Xe::JITCompat::SetArg(funcNode, 0, ppuArg);

  asmjit::x86::Gp ppeBase = Xe::JITCompat::NewGPZ(&comp, "ppe");
  Xe::JITCompat::SetArg(funcNode, 1, ppeBase);

  asmjit::x86::Gp haltArg = Xe::JITCompat::NewGP8(&comp, "halt_unused");
  Xe::JITCompat::SetArg(funcNode, 2, haltArg);

  // Enable AVX
  funcNode->frame(). SET_AVX();

  // Compute thread context from sPPEState* + baked thread offset
  // This is identical to PPU_JIT::SetupContext.
  u64 threadOffset = static_cast<u8>(threadId) * sizeof(sPPUThread);
  asmjit::x86::Gp ctxBase = Xe::JITCompat::NewGPZ(&comp, "ctx");
  comp.lea(ctxBase, asmjit::x86::ptr(ppeBase, static_cast<s32>(threadOffset)));

  // Create the context pointer
  ASMJitPtr<sPPUThread> ctxPtr(ctxBase);
  ppuThreadCtx = &ctxPtr;

  // Create the PPE state pointer
  ASMJitPtr<sPPEState> ppePtr(ppeBase);
  ppeState = &ppePtr;

  // Walk the instruction linked list and emit host code for each instruction.
  // After instructions that can set exceptReg, emit a bailout check.
  for (const HIR::Instr *instr = block->instrHead; instr; instr = instr->next) {
    EmitInstruction(instr);
  }

  // Epilogue.
  comp.ret();
  Xe::JITCompat::EndFunc(&comp);

  asmjit::Error err = comp.finalize();
  compiler = nullptr;
  ppuThreadCtx = nullptr;
  ppeState = nullptr;

  if (err != asmjit::Error::kOk) {
    LOG_ERROR(Xenon, "[x86Backend]: asmjit finalize error: {}", asmjit::DebugUtils::error_as_string(err));
    DumpHIR(block);
    return false;
  }

  if (printGeneratedCode) {
    codeHolder.set_logger(nullptr);
    if (err == asmjit::Error::kOk) {
      const char *content = asmLogger.content().data();
      if (content && content[0] != '\0') {
        LOG_DEBUG(Xenon, "[x86Backend]: Generated assembly:\n{}", content);
      }
    }
  }



  // Add compiled code to the runtime. The pointer is now owned by the caller
  // via the shared JitRuntime; caller releases with ReleaseCode().
  void *codePtr = nullptr;
  {
    std::unique_lock<std::mutex> lock;
    if (jitRuntimeMutex) lock = std::unique_lock<std::mutex>(*jitRuntimeMutex);
    err = jitRuntime->add(&codePtr, &codeHolder);
  }
  if (err != asmjit::Error::kOk || !codePtr) {
    LOG_ERROR(Xenon, "[x86Backend]: asmjit runtime add error: {}", asmjit::DebugUtils::error_as_string(err));
    return false;
  }

  // Return generated code and code size
  *outCode = codePtr;
  *outCodeSize = codeHolder. CODE_SZ();
  return true;
}

// Releases generated code
void x86CodeGenBackend::ReleaseCode(void *codePtr) {
  if (codePtr && jitRuntime) {
    std::unique_lock<std::mutex> lock;
    if (jitRuntimeMutex)
      lock = std::unique_lock<std::mutex>(*jitRuntimeMutex);
    jitRuntime->release(codePtr);
  }
}

// Resets compiler, context and holder before each block building
void x86CodeGenBackend::Reset() {
  compiler = nullptr;
  ppuThreadCtx = nullptr;
  ppeState = nullptr;
  codeHolder.reset();
}


} // namespace Xe::XCPU::JIT

#endif
