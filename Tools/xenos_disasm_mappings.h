// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include "Base/Logging/Log.h"
#include "Base/Types.h"

#include "Core/XGPU/Microcode/Constants.h"

#define CASE(x) case Xe::x: return #x

enum class ShaderType {
  Vertex,
  Pixel,
};

static ShaderType ParseShaderType(const std::string& path) {
  if (path.find("vertex") != std::string::npos)
    return ShaderType::Vertex;
  if (path.find("pixel") != std::string::npos)
    return ShaderType::Pixel;

  LOG_ERROR(Core, "Cannot infer shader type from path '{}' (expected 'vertex' or 'pixel' in name)", path);
  ::abort();
}

static const char *VectorOpName(Xe::instr_vector_opc_t op) {
  switch (op) {
    CASE(ADDv);
    CASE(MULv);
    CASE(MAXv);
    CASE(MINv);
    CASE(SETEv);
    CASE(SETGTv);
    CASE(SETGTEv);
    CASE(SETNEv);
    CASE(FRACv);
    CASE(TRUNCv);
    CASE(FLOORv);
    CASE(MULADDv);
    CASE(CNDEv);
    CASE(CNDGTEv);
    CASE(CNDGTv);
    CASE(DOT4v);
    CASE(DOT3v);
    CASE(DOT2ADDv);
    CASE(CUBEv);
    CASE(MAX4v);
    CASE(PRED_SETE_PUSHv);
    CASE(PRED_SETNE_PUSHv);
    CASE(PRED_SETGT_PUSHv);
    CASE(PRED_SETGTE_PUSHv);
    CASE(KILLEv);
    CASE(KILLGTv);
    CASE(KILLGTEv);
    CASE(KILLNEv);
    CASE(DSTv);
    CASE(MOVAv);
  }
  return "VOP?";
}

static const char *ScalarOpName(Xe::instr_scalar_opc_t op) {
  switch (op) {
    CASE(ADDs);
    CASE(ADD_PREVs);
    CASE(MULs);
    CASE(MUL_PREVs);
    CASE(MUL_PREV2s);
    CASE(MAXs);
    CASE(MINs);
    CASE(SETEs);
    CASE(SETGTs);
    CASE(SETGTEs);
    CASE(SETNEs);
    CASE(FRACs);
    CASE(TRUNCs);
    CASE(FLOORs);
    CASE(EXP_IEEE);
    CASE(LOG_CLAMP);
    CASE(LOG_IEEE);
    CASE(RECIP_CLAMP);
    CASE(RECIP_FF);
    CASE(RECIP_IEEE);
    CASE(RECIPSQ_CLAMP);
    CASE(RECIPSQ_FF);
    CASE(RECIPSQ_IEEE);
    CASE(MOVAs);
    CASE(MOVA_FLOORs);
    CASE(SUBs);
    CASE(SUB_PREVs);
    CASE(PRED_SETEs);
    CASE(PRED_SETNEs);
    CASE(PRED_SETGTs);
    CASE(PRED_SETGTEs);
    CASE(PRED_SET_INVs);
    CASE(PRED_SET_POPs);
    CASE(PRED_SET_CLRs);
    CASE(PRED_SET_RESTOREs);
    CASE(KILLEs);
    CASE(KILLGTs);
    CASE(KILLGTEs);
    CASE(KILLNEs);
    CASE(KILLONEs);
    CASE(SQRT_IEEE);
    CASE(MUL_CONST_0);
    CASE(MUL_CONST_1);
    CASE(ADD_CONST_0);
    CASE(ADD_CONST_1);
    CASE(SUB_CONST_0);
    CASE(SUB_CONST_1);
    CASE(SIN);
    CASE(COS);
    CASE(RETAIN_PREV);
  }
  return "SOP?";
}

// vector_write_mask is 4 bits: xyzw
static std::string MaskToString(u32 mask) {
  std::string out;
  if (mask & 0x1)
    out.push_back('x');
  if (mask & 0x2)
    out.push_back('y');
  if (mask & 0x4)
    out.push_back('z');
  if (mask & 0x8)
    out.push_back('w');
  if (out.empty())
    out = "0";
  return out;
}

// src*_swiz is 8 bits: 2 bits per component (xyzw)
static std::string SwizzleToString(u32 swiz8) {
  static const char comps[4] = { 'x', 'y', 'z', 'w' };
  std::string out;
  out.reserve(4);
  for (u8 i = 0; i != 4; ++i) {
    u32 c = (swiz8 >> (i * 2)) & 0x3;
    out.push_back(comps[c]);
  }
  return out;
}

// Our working assumption for src*_sel:
//   0 -> GPR register
//   1 -> Constant register
static std::string FormatSrcReg(u32 reg, u32 sel, u32 swiz8, bool negate) {
  std::string r;
  if (sel)
    r = "c" + std::to_string(reg);
  else
    r = "r" + std::to_string(reg);

  r.push_back('.');
  r += SwizzleToString(swiz8);

  if (negate)
    r = "-" + r;

  return r;
}

#undef CASE