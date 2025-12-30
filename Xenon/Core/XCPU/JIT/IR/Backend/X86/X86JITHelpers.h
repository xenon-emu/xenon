/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "asmjit/x86.h"

#define COMP backEnd->mCompiler
#define newGP64()  COMP.newGpq()
#define newGP32()  COMP.newGpd()
#define newGP16()  COMP.newGpw()
#define newGP8()   COMP.newGpb()
#define newGPptr() COMP.newGpz()
#define newXmm() COMP.newXmm() // FP Reg/VMX Reg: 128 bit reg (32x4)

#define GPRPtr(x) context->threadCtx->array(&sPPUThread::GPR).Ptr(x)
#define SPRStruct(x) context->threadCtx->substruct(&sPPUThread::SPR).substruct(&sPPUThreadSPRs::x)
#define SPRPtr(x) context->threadCtx->substruct(&sPPUThread::SPR).scalar(&sPPUThreadSPRs::x)
#define SharedSPRStruct(x) context->ppeState->substruct(&sPPEState::SPR).substruct(&sPPESPRs::x)
#define SharedSPRPtr(x) context->ppeState->substruct(&sPPEState::SPR).scalar(&sPPUGlobalSPRs::x)
#define CRValPtr() context->threadCtx->scalar(&sPPUThread::CR)
#define CIAPtr() context->threadCtx->scalar(&sPPUThread::CIA)
#define NIAPtr() context->threadCtx->scalar(&sPPUThread::NIA)
#define EXPtr() context->threadCtx->scalar(&sPPUThread::exceptReg) 
#define LRPtr() SPRPtr(LR)



#if defined(ARCH_X86) || defined(ARCH_X86_64)
template <typename T, typename fT>
class ArrayFieldProxy {
public:
  ArrayFieldProxy(const asmjit::x86::Gp& base, u64 offset = 0) :
    base(base), offset(offset)
  {
  }
  ArrayFieldProxy(const ArrayFieldProxy& other)
    : base(other.base), offset(other.offset)
  {
  }

  asmjit::x86::Mem operator[](u64 index) const {
    using eT = typename std::remove_extent<fT>::type;
    return asmjit::x86::ptr(base, offset + index * sizeof(eT));
  }

  asmjit::x86::Mem Ptr(u64 index) {
    using eT = typename std::remove_extent<fT>::type;
    return asmjit::x86::ptr(base, offset + index * sizeof(eT));
  }

  asmjit::x86::Gp Base() const {
    return base;
  }

  u64 Offset() const {
    return offset;
  }
private:
  asmjit::x86::Gp base;
  u64 offset = 0;
};

template <typename T, typename fT>
class ScalarFieldProxy {
public:
  ScalarFieldProxy(const asmjit::x86::Gp& base, u64 offset)
    : base(base), offset(offset)
  {
  }
  ScalarFieldProxy(const ScalarFieldProxy& other)
    : base(other.base), offset(other.offset)
  {
  }

  operator asmjit::x86::Mem() const {
    return asmjit::x86::ptr(base, offset);
  }

  template <typename pT = u8>
  asmjit::x86::Mem Ptr(u64 size = sizeof(pT)) const {
    return asmjit::x86::ptr(base, offset, size);
  }

  asmjit::x86::Gp Base() const {
    return base;
  }

  u64 Offset() const {
    return offset;
  }
private:
  asmjit::x86::Gp base;
  u64 offset;
};

template <typename T>
class ASMJitPtr {
public:
  ASMJitPtr(const asmjit::x86::Gp& baseReg, u64 offset = 0) :
    base(baseReg), offset(offset)
  {
  }

  template<typename fT>
  ScalarFieldProxy<T, fT> scalar(fT T::* member) const {
    u64 off = reinterpret_cast<u64>(&(reinterpret_cast<T*>(0)->*member));
    return ScalarFieldProxy<T, fT>(base, offset + off);
  }

  template<typename fT>
  ArrayFieldProxy<T, fT> array(fT T::* member) const {
    u64 off = reinterpret_cast<u64>(&(reinterpret_cast<T*>(0)->*member));
    return ArrayFieldProxy<T, fT>(base, offset + off);
  }

  template<typename sT>
  ASMJitPtr<sT> substruct(sT T::* member) const {
    u64 off = reinterpret_cast<u64>(&(reinterpret_cast<T*>(0)->*member));
    return ASMJitPtr<sT>(base, offset + off);
  }

  template <typename pT = u8>
  asmjit::x86::Mem Ptr(u64 size = sizeof(pT)) const {
    return asmjit::x86::ptr(base, offset, size);
  }

  operator asmjit::x86::Gp() const {
    return base;
  }

  u64 Offset() {
    return offset;
  }

  asmjit::x86::Gp Base() const {
    return base;
  }
private:
  asmjit::x86::Gp base;
  u64 offset = 0;
};
#endif