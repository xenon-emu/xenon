/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <cstring>

#include "Core/XCPU/JIT/HIR/Value.h"
#include "Core/XCPU/JIT/JITCompat.h"
#include "asmjit/x86.h"

namespace Xe::XCPU::JIT {

//
// Allocates a new general purpose x86 register
//
#define newGP64()  Xe::JITCompat::NewGP64(b->GetCompiler())
#define newGP32()  Xe::JITCompat::NewGP32(b->GetCompiler())
#define newGP16()  Xe::JITCompat::NewGP16(b->GetCompiler())
#define newGP8()   Xe::JITCompat::NewGP8(b->GetCompiler())
#define newGPptr() Xe::JITCompat::NewGPZ(b->GetCompiler())

#define newLabel() Xe::JITCompat::NewLabel(b->GetCompiler())
#define newStack(x, y) Xe::JITCompat::NewStack(b->GetCompiler(), x, y)

//
// Allocates a new XMM register for floating-point operations
//
#define newXMM() Xe::JITCompat::NewXmm(b->GetCompiler())

#define newYMM() Xe::JITCompat::NewYmm(b->GetCompiler())
#define newZMM() Xe::JITCompat::NewZmm(b->GetCompiler())

//
// Pointer Helpers
//

#define COMP                b->GetCompiler()
#define GPRPtr(x)           b->GetThreadContext()->array(&sPPUThread::GPR).Ptr(x)
#define SPRStruct(x)        b->GetThreadContext()->substruct(&sPPUThread::SPR).substruct(&sPPUThreadSPRs::x)
#define SPRPtr(x)           b->GetThreadContext()->substruct(&sPPUThread::SPR).scalar(&sPPUThreadSPRs::x)
#define SharedSPRStruct(x)  b->ppeState->substruct(&sPPEState::SPR).substruct(&sPPESPRs::x)
#define SharedSPRPtr(x)     b->ppeState->substruct(&sPPEState::SPR).scalar(&sPPUGlobalSPRs::x)
#define CRValPtr()          b->GetThreadContext()->scalar(&sPPUThread::CR)
#define CIAPtr()            b->GetThreadContext()->scalar(&sPPUThread::CIA)
#define NIAPtr()            b->GetThreadContext()->scalar(&sPPUThread::NIA)
#define EXPtr()             b->GetThreadContext()->scalar(&sPPUThread::exceptReg)
#define LRPtr()             SPRPtr(LR)

//
// Value <-> Virtual Register helpers.
// Packs the asmjit register signature (u32) and id (u32) into the
// Value::tag pointer so that downstream emitters can recover the
// full virtual register from a prior instruction's result.
//

// Store a virtual register into a Value's tag.
inline void TagStoreReg(HIR::Value *val, const asmjit::Reg &reg) {
  u64 packed = (u64(reg.signature()._bits) << 32) | u64(reg.id());
  val->tag = reinterpret_cast<void *>(packed);
}

// Recover a GP virtual register from a Value's tag.
inline asmjit::x86::Gp TagLoadGp(const HIR::Value *val) {
  u64 packed = reinterpret_cast<u64>(val->tag);
  asmjit::x86::Gp reg;
  reg.set_signature(static_cast<u32>(packed >> 32));
  reg.set_id(static_cast<u32>(packed & 0xFFFFFFFF));
  return reg;
}

// Recover an XMM virtual register from a Value's tag.
inline asmjit::x86::Vec TagLoadXmm(const HIR::Value *val) {
  u64 packed = reinterpret_cast<u64>(val->tag);
  asmjit::x86::Vec reg;
  reg.set_signature(static_cast<u32>(packed >> 32));
  reg.set_id(static_cast<u32>(packed & 0xFFFFFFFF));
  return reg;
}

// Load a HIR Value into an XMM virtual register.
// Handles both constant values (materializes from memory) and SSA
// values produced by prior instructions (recovers the virtual register).
inline asmjit::x86::Vec LoadValueXmm(x86CodeGenBackend *b, HIR::Value *val) {
  if (val->IsConstant()) {
    switch (val->type) {
    case HIR::FLOAT32_TYPE: {
      auto reg = newXMM();
      auto tmp = newGP32();
      u32 bits;
      std::memcpy(&bits, &val->constant.f32, sizeof(bits));
      COMP->mov(tmp, asmjit::Imm(bits));
      COMP->vmovd(reg, tmp);
      return reg;
    }
    case HIR::FLOAT64_TYPE: {
      auto reg = newXMM();
      auto tmp = newGP64();
      u64 bits;
      std::memcpy(&bits, &val->constant.f64, sizeof(bits));
      COMP->mov(tmp, asmjit::Imm(static_cast<s64>(bits)));
      COMP->vmovq(reg, tmp);
      return reg;
    }
    case HIR::VEC128_TYPE: {
      auto reg = newXMM();
      // Materialize 128-bit constant via two 64-bit halves
      auto tmpLo = newGP64();
      auto tmpHi = newGP64();
      u64 lo = val->constant.v128.qword[0];
      u64 hi = val->constant.v128.qword[1];
      COMP->mov(tmpLo, asmjit::Imm(static_cast<s64>(lo)));
      COMP->vmovq(reg, tmpLo);
      if (hi != 0) {
        auto regHi = newXMM();
        COMP->mov(tmpHi, asmjit::Imm(static_cast<s64>(hi)));
        COMP->vmovq(regHi, tmpHi);
        COMP->vpunpcklqdq(reg, reg, regHi);
      }
      return reg;
    }
    default:
      break;
    }
  }
  return TagLoadXmm(val);
}

// Load a HIR Value into a GP virtual register.
// Handles both constant values (materializes an immediate) and SSA
// values produced by prior instructions (recovers the virtual register).
inline asmjit::x86::Gp LoadValueGp(x86CodeGenBackend *b, HIR::Value *val) {
  if (val->IsConstant()) {
    switch (val->type) {
    case HIR::INT8_TYPE: {
      auto reg = newGP8();
      COMP->mov(reg, asmjit::Imm(val->constant.u8));
      return reg;
    }
    case HIR::INT16_TYPE: {
      auto reg = newGP16();
      COMP->mov(reg, asmjit::Imm(val->constant.u16));
      return reg;
    }
    case HIR::INT32_TYPE: {
      auto reg = newGP32();
      COMP->mov(reg, asmjit::Imm(val->constant.u32));
      return reg;
    }
    case HIR::INT64_TYPE: {
      auto reg = newGP64();
      COMP->mov(reg, asmjit::Imm(val->constant.i64));
      return reg;
    }
    default:
      break;
    }
  }
  return TagLoadGp(val);
}

// Helper: load a static Vector128 constant into an XMM register.
static x86::Vec LoadXmmConst(x86CodeGenBackend *b, const Base::Vector128 &v) {
  x86::Vec reg = newXMM();
  x86::Gp addr = newGP64();
  COMP->mov(addr, asmjit::Imm(reinterpret_cast<u64>(&v)));
  COMP->vmovaps(reg, x86::ptr(addr));
  return reg;
}

} // namespace Xe::XCPU::JIT
