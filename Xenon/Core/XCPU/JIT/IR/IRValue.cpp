/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Core/XCPU/JIT/IR/IRTypes.h"

namespace Xe::XCPU::JIT::IR {

  void IRValue::SetZero(IRType newType) {
    type = newType;
    kind = ValueKind::Constant;
    isConstantVal = true;
    // This is an union, setting the vector as zero sets everything else.
    constValue.vec128.qword[0] = constValue.vec128.qword[1] = 0;
  }

  void IRValue::SetConstant(s8 value) {
    type = IRType::INT8;
    kind = ValueKind::Constant;
    isConstantVal = true;
    constValue.i64 = s64(value);
  }

  void IRValue::SetConstant(u8 value) {
    type = IRType::INT8;
    kind = ValueKind::Constant;
    isConstantVal = true;
    constValue.i64 = u64(value);
  }

  void IRValue::SetConstant(s16 value) {
    type = IRType::INT16;
    kind = ValueKind::Constant;
    isConstantVal = true;
    constValue.i64 = s64(value);
  }

  void IRValue::SetConstant(u16 value) {
    type = IRType::INT16;
    kind = ValueKind::Constant;
    isConstantVal = true;
    constValue.i64 = u64(value);
  }

  void IRValue::SetConstant(s32 value) {
    type = IRType::INT32;
    kind = ValueKind::Constant;
    isConstantVal = true;
    constValue.i64 = s64(value);
  }

  void IRValue::SetConstant(u32 value) {
    type = IRType::INT32;
    kind = ValueKind::Constant;
    isConstantVal = true;
    constValue.i64 = u64(value);
  }

  void IRValue::SetConstant(s64 value) {
    type = IRType::INT64;
    kind = ValueKind::Constant;
    isConstantVal = true;
    constValue.i64 = value;
  }

  void IRValue::SetConstant(u64 value) {
    type = IRType::INT64;
    kind = ValueKind::Constant;
    isConstantVal = true;
    constValue.i64 = value;
  }

  void IRValue::SetConstant(f32 value) {
    type = type = IRType::FLOAT32;
    kind = ValueKind::Constant;
    isConstantVal = true;
    constValue.flt32 = value;
  }

  void IRValue::SetConstant(f64 value) {
    type = IRType::FLOAT64;
    kind = ValueKind::Constant;
    isConstantVal = true;
    constValue.flt64 = value;
  }

  void IRValue::SetConstant(const Base::Vector128 &value) {
    type = IRType::VEC128;
    kind = ValueKind::Constant;
    isConstantVal = true;
    constValue.vec128 = value;
  }

  bool IRValue::IsConstantTrue() const {
    if (isConstantVal) {
      switch (type) {
      case IRType::INT8: return !!constValue.i8;
      case IRType::INT16: return !!constValue.i16;
      case IRType::INT32: return !!constValue.i32;
      case IRType::INT64: return !!constValue.i64;
      case IRType::FLOAT32: return !!constValue.flt32;
      case IRType::FLOAT64: return !!constValue.flt64;
      case IRType::VEC128: return constValue.vec128.qword[0] || constValue.vec128.qword[1];
      default:
        UNREACHABLE_MSG("Unhandled IsConstantTrue type");
        return false;
      }
    } else { return false; }
  }

  bool IRValue::IsConstantFalse() const {
    if (isConstantVal) {
      switch (type) {
      case IRType::INT8: return !constValue.i8;
      case IRType::INT16: return !constValue.i16;
      case IRType::INT32: return !constValue.i32;
      case IRType::INT64: return !constValue.i64;
      case IRType::FLOAT32: return !constValue.flt32;
      case IRType::FLOAT64: return !constValue.flt64;
      case IRType::VEC128: return !(constValue.vec128.qword[0] || constValue.vec128.qword[1]);
      default: UNREACHABLE_MSG("Unhandled IsConstantFalse type"); return false;
      }
    } else { return false; }
  }

  bool IRValue::IsConstantZero() const {
    if (isConstantVal) {
      switch (type) {
      case IRType::INT8: return !constValue.i8;
      case IRType::INT16: return !constValue.i16;
      case IRType::INT32: return !constValue.i32;
      case IRType::INT64: return !constValue.i64;
      case IRType::FLOAT32: return !constValue.flt32;
      case IRType::FLOAT64: return !constValue.flt64;
      case IRType::VEC128: return !constValue.vec128.qword[0] && !constValue.vec128.qword[1];
      default: UNREACHABLE_MSG("Unhandled IsConstantZero type"); return false;
      }
    } else { return false; }
  }

  bool IRValue::IsConstantOne() const {
    if (isConstantVal) {
      switch (type) {
      case IRType::INT8: return constValue.i8 == 1;
      case IRType::INT16: return constValue.i16 == 1;
      case IRType::INT32: return constValue.i32 == 1;
      case IRType::INT64: return constValue.i64 == 1;
      case IRType::FLOAT32: return constValue.flt32 == 1.0f;
      case IRType::FLOAT64: return constValue.flt64 == 1.0;
      default: UNREACHABLE_MSG("Unhandled IsConstantOne type"); return false;
      }
    } else { return false; }
  }

  bool IRValue::IsConstantEQ(IRValue *operand) const {
    if ((isConstantVal) && (operand->isConstantVal)) {
      switch (type) {
      case IRType::INT8: return constValue.i8 == operand->constValue.i8;
      case IRType::INT16: return constValue.i16 == operand->constValue.i16;
      case IRType::INT32: return constValue.i32 == operand->constValue.i32;
      case IRType::INT64: return constValue.i64 == operand->constValue.i64;
      case IRType::FLOAT32: return constValue.flt32 == operand->constValue.flt32;
      case IRType::FLOAT64: return constValue.flt64 == operand->constValue.flt64;
      case IRType::VEC128: return constValue.vec128.qword[0] == operand->constValue.vec128.qword[0] &&
        constValue.vec128.qword[1] == operand->constValue.vec128.qword[1];
      default: UNREACHABLE_MSG("Unhandled IsConstantEQ type"); return false;
      }
    } else { return false; }
  }

  bool IRValue::IsConstantNE(IRValue *operand) const {
    ASSERT(type == IRType::VEC128);

    if ((isConstantVal) && (operand->isConstantVal)) {
      switch (type) {
      case IRType::INT8: return constValue.i8 != operand->constValue.i8;
      case IRType::INT16: return constValue.i16 != operand->constValue.i16;
      case IRType::INT32: return constValue.i32 != operand->constValue.i32;
      case IRType::INT64: return constValue.i64 != operand->constValue.i64;
      case IRType::FLOAT32: return constValue.flt32 != operand->constValue.flt32;
      case IRType::FLOAT64: return constValue.flt64 != operand->constValue.flt64;
      default: UNREACHABLE_MSG("Unhandled IsConstantNE type"); return false; }
    } else { return false; }
  }

  bool IRValue::IsConstantSLT(IRValue *operand) const {
    if ((isConstantVal) && (operand->isConstantVal)) {
    switch (type) {
    case IRType::INT8: return constValue.i8 < operand->constValue.i8;
    case IRType::INT16: return constValue.i16 < operand->constValue.i16;
    case IRType::INT32: return constValue.i32 < operand->constValue.i32;
    case IRType::INT64: return constValue.i64 < operand->constValue.i64;
    case IRType::FLOAT32: return constValue.flt32 < operand->constValue.flt32;
    case IRType::FLOAT64: return constValue.flt64 < operand->constValue.flt64;
    default: UNREACHABLE_MSG("Unhandled IsConstantSLT type"); return false; }
    } else { return false; }
  }

  bool IRValue::IsConstantSLE(IRValue *operand) const {
    if ((isConstantVal) && (operand->isConstantVal)) {
      switch (type) {
      case IRType::INT8: return constValue.i8 <= operand->constValue.i8;
      case IRType::INT16: return constValue.i16 <= operand->constValue.i16;
      case IRType::INT32: return constValue.i32 <= operand->constValue.i32;
      case IRType::INT64: return constValue.i64 <= operand->constValue.i64;
      case IRType::FLOAT32: return constValue.flt32 <= operand->constValue.flt32;
      case IRType::FLOAT64: return constValue.flt64 <= operand->constValue.flt64;
      default: UNREACHABLE_MSG("Unhandled IsConstantSLE type"); return false; }
    } else { return false; }
  }

  bool IRValue::IsConstantSGT(IRValue *operand) const {
    if ((isConstantVal) && (operand->isConstantVal)) {
    switch (type) {
    case IRType::INT8: return constValue.i8 > operand->constValue.i8;
    case IRType::INT16: return constValue.i16 > operand->constValue.i16;
    case IRType::INT32: return constValue.i32 > operand->constValue.i32;
    case IRType::INT64: return constValue.i64 > operand->constValue.i64;
    case IRType::FLOAT32: return constValue.flt32 > operand->constValue.flt32;
    case IRType::FLOAT64: return constValue.flt64 > operand->constValue.flt64;
    default: UNREACHABLE_MSG("Unhandled IsConstantSGT type"); return false; }
    } else { return false; }
  }

  bool IRValue::IsConstantSGE(IRValue *operand) const {
    if ((isConstantVal) && (operand->isConstantVal)) {
    switch (type) {
    case IRType::INT8: return constValue.i8 >= operand->constValue.i8;
    case IRType::INT16: return constValue.i16 >= operand->constValue.i16;
    case IRType::INT32: return constValue.i32 >= operand->constValue.i32;
    case IRType::INT64: return constValue.i64 >= operand->constValue.i64;
    case IRType::FLOAT32: return constValue.flt32 >= operand->constValue.flt32;
    case IRType::FLOAT64: return constValue.flt64 >= operand->constValue.flt64;
    default: UNREACHABLE_MSG("Unhandled IsConstantSGE type"); return false; }
    } else { return false; }
  }

  bool IRValue::IsConstantULT(IRValue *operand) const {
    if ((isConstantVal) && (operand->isConstantVal)) {
    switch (type) {
    case IRType::INT8: return (u8)constValue.i8 < (u8)operand->constValue.i8;
    case IRType::INT16: return (u16)constValue.i16 < (u16)operand->constValue.i16;
    case IRType::INT32: return (u32)constValue.i32 < (u32)operand->constValue.i32;
    case IRType::INT64: return (u64)constValue.i64 < (u64)operand->constValue.i64;
    case IRType::FLOAT32: return constValue.flt32 < operand->constValue.flt32;
    case IRType::FLOAT64: return constValue.flt64 < operand->constValue.flt64;
    default: UNREACHABLE_MSG("Unhandled IsConstantULT type"); return false; }
    } else { return false; }
  }

  bool IRValue::IsConstantULE(IRValue *operand) const {
    if ((isConstantVal) && (operand->isConstantVal)) {
    switch (type) {
    case IRType::INT8: return (u8)constValue.i8 <= (u8)operand->constValue.i8;
    case IRType::INT16: return (u16)constValue.i16 <= (u16)operand->constValue.i16;
    case IRType::INT32: return (u32)constValue.i32 <= (u32)operand->constValue.i32;
    case IRType::INT64: return (u64)constValue.i64 <= (u64)operand->constValue.i64;
    case IRType::FLOAT32: return constValue.flt32 <= operand->constValue.flt32;
    case IRType::FLOAT64: return constValue.flt64 <= operand->constValue.flt64;
    default: UNREACHABLE_MSG("Unhandled IsConstantULE type"); return false; }
    } else { return false; }
  }

  bool IRValue::IsConstantUGT(IRValue *operand) const {
    if ((isConstantVal) && (operand->isConstantVal)) {
    switch (type) {
    case IRType::INT8: return (u8)constValue.i8 > (u8)operand->constValue.i8;
    case IRType::INT16: return (u16)constValue.i16 > (u16)operand->constValue.i16;
    case IRType::INT32: return (u32)constValue.i32 > (u32)operand->constValue.i32;
    case IRType::INT64: return (u64)constValue.i64 > (u64)operand->constValue.i64;
    case IRType::FLOAT32: return constValue.flt32 > operand->constValue.flt32;
    case IRType::FLOAT64: return constValue.flt64 > operand->constValue.flt64;
    default: UNREACHABLE_MSG("Unhandled IsConstantUGT type"); return false; }
    } else { return false; }
  }

  bool IRValue::IsConstantUGE(IRValue *operand) const {
    if ((isConstantVal) && (operand->isConstantVal)) {
    switch (type) {
    case IRType::INT8: return (u8)constValue.i8 >= (u8)operand->constValue.i8;
    case IRType::INT16: return (u16)constValue.i16 >= (u16)operand->constValue.i16;
    case IRType::INT32: return (u32)constValue.i32 >= (u32)operand->constValue.i32;
    case IRType::INT64: return (u64)constValue.i64 >= (u64)operand->constValue.i64;
    case IRType::FLOAT32: return constValue.flt32 >= operand->constValue.flt32;
    case IRType::FLOAT64: return constValue.flt64 >= operand->constValue.flt64;
    default: UNREACHABLE_MSG("Unhandled IsConstantUGE type"); return false; }
    } else { return false; }
  }

  void IRValue::Cast(IRType newType) { type = newType; }

  void IRValue::ZeroExtend(IRType newType) {
    switch (type) {
    case IRType::INT8: type = newType; constValue.u64 = constValue.u8; return;
    case IRType::INT16: type = newType; constValue.u64 = constValue.u16; return;
    case IRType::INT32: type = newType; constValue.u64 = constValue.u32; return;
    default: UNREACHABLE_MSG("Unhandled ZeroExtend type"); break;
    }
  }

  void IRValue::SignExtend(IRType newType) {
    switch (type) {
    case IRType::INT8:
      type = newType;
      switch (newType) {
      case IRType::INT16: constValue.i16 = constValue.i8; return;
      case IRType::INT32: constValue.i32 = constValue.i8; return;
      case IRType::INT64: constValue.i64 = constValue.i8; return;
      default: UNREACHABLE_MSG("Unhandled type"); return;
      }
    case IRType::INT16:
      type = newType;
      switch (newType) {
      case IRType::INT32: constValue.i32 = constValue.i16; return;
      case IRType::INT64: constValue.i64 = constValue.i16; return;
      default: UNREACHABLE_MSG("Unhandled type"); return;
      }
    case IRType::INT32:
      type = newType;
      switch (newType) {
      case IRType::INT64: constValue.i64 = constValue.i32; return;
      default: UNREACHABLE_MSG("Unhandled type"); return;
      }
    default: UNREACHABLE_MSG("Unhandled type"); return;
    }
  }

  void IRValue::Truncate(IRType newType) {
    switch (type) {
    case IRType::INT16:
      switch (newType) {
      case IRType::INT8: type = newType; constValue.i64 = constValue.i64 & 0xFF; return;
      default: UNREACHABLE_MSG("Unhandled type"); return;
      }
    case IRType::INT32:
      switch (newType) {
      case IRType::INT8: type = newType; constValue.i64 = constValue.i64 & 0xFF; return;
      case IRType::INT16: type = newType; constValue.i64 = constValue.i64 & 0xFFFF; return;
      default: UNREACHABLE_MSG("Unhandled type"); return;
      }
    case IRType::INT64:
      switch (newType) {
      case IRType::INT8: type = newType; constValue.i64 = constValue.i64 & 0xFF; return;
      case IRType::INT16: type = newType; constValue.i64 = constValue.i64 & 0xFFFF; return;
      case IRType::INT32: type = newType; constValue.i64 = constValue.i64 & 0xFFFFFFFF; return;
      default: UNREACHABLE_MSG("Unhandled type"); return;
      }
    default: UNREACHABLE_MSG("Unhandled type"); return;
    }
  }

  void IRValue::Convert(IRType newType) {
    switch (type) {
    case IRType::FLOAT32:
      switch (newType) {
      case IRType::FLOAT64: type = newType; constValue.flt64 = constValue.flt32; return;
      default: UNREACHABLE_MSG("Unhandled type"); return;
      }
    case IRType::INT64:
      switch (newType) {
      case IRType::FLOAT64: type = newType; constValue.flt64 = (f64)constValue.i64; return;
      default: UNREACHABLE_MSG("Unhandled type"); return;
      }
    case IRType::FLOAT64:
      switch (newType) {
      case IRType::FLOAT32: type = newType; constValue.flt32 = (f32)constValue.flt64; return;
      case IRType::INT32: type = newType; constValue.i32 = (s32)constValue.flt64; return;
      case IRType::INT64: type = newType; constValue.i64 = (s64)constValue.flt64; return;
      default: UNREACHABLE_MSG("Unhandled type"); return;
      }
    default: UNREACHABLE_MSG("Unhandled type"); return;
    }
  }

  template <typename T>
  T __inline RoundValue(RoundingMode roundMode, T value) {
    switch (roundMode) {
    case RoundToZero: return std::trunc(value);
    case RoundToNear: return std::round(value);
    case RoundToMinusInf: return std::floor(value);
    case RoundToPlusInf: return std::ceil(value);
    default: UNREACHABLE_MSG("Unhandled type"); return value;
    }
  }

  void IRValue::Round(RoundingMode roundMode) {
    switch (type) {
    case IRType::FLOAT32: constValue.flt32 = RoundValue(roundMode, constValue.flt32); return;
    case IRType::FLOAT64: constValue.flt64 = RoundValue(roundMode, constValue.flt64); return;
    case IRType::VEC128: for (int i = 0; i < 4; i++) { constValue.vec128.flt[i] = RoundValue(roundMode, constValue.vec128.flt[i]); } return;
    default: UNREACHABLE_MSG("Unhandled type"); return;
    }
  }

  void IRValue::Add(IRValue *operand) {
    ASSERT(type == operand->type);

    switch (type) {
    case IRType::INT8: constValue.i8 += operand->constValue.i8; break;
    case IRType::INT16: constValue.i16 += operand->constValue.i16; break;
    case IRType::INT32: constValue.i32 += operand->constValue.i32; break;
    case IRType::INT64: constValue.i64 += operand->constValue.i64; break;
    case IRType::FLOAT32: constValue.flt32 += operand->constValue.flt32; break;
    case IRType::FLOAT64: constValue.flt64 += operand->constValue.flt64; break;
    case IRType::VEC128: for (int i = 0; i < 4; i++) { constValue.vec128.flt[i] += operand->constValue.vec128.flt[i]; } break;
    default: UNREACHABLE_MSG("Unhandled type"); break;
    }
  }

  void IRValue::Sub(IRValue *operand) {
    ASSERT(type == operand->type);

    switch (type) {
    case IRType::INT8: constValue.i8 -= operand->constValue.i8; break;
    case IRType::INT16: constValue.i16 -= operand->constValue.i16; break;
    case IRType::INT32: constValue.i32 -= operand->constValue.i32; break;
    case IRType::INT64: constValue.i64 -= operand->constValue.i64; break;
    case IRType::FLOAT32: constValue.flt32 -= operand->constValue.flt32; break;
    case IRType::FLOAT64: constValue.flt64 -= operand->constValue.flt64; break;
    case IRType::VEC128: for (int i = 0; i < 4; i++) { constValue.vec128.flt[i] -= operand->constValue.vec128.flt[i]; } break;
    default: UNREACHABLE_MSG("Unhandled type"); break;
    }
  }

  void IRValue::Mul(IRValue *operand) {
    ASSERT(type == operand->type);

    switch (type) {
    case IRType::INT8: constValue.i8 *= operand->constValue.i8; break;
    case IRType::INT16: constValue.i16 *= operand->constValue.i16; break;
    case IRType::INT32: constValue.i32 *= operand->constValue.i32; break;
    case IRType::INT64: constValue.i64 *= operand->constValue.i64; break;
    case IRType::FLOAT32: constValue.flt32 *= operand->constValue.flt32; break;
    case IRType::FLOAT64: constValue.flt64 *= operand->constValue.flt64; break;
    case IRType::VEC128: for (int i = 0; i < 4; i++) { constValue.vec128.flt[i] *= operand->constValue.vec128.flt[i]; } break;
    default: UNREACHABLE_MSG("Unhandled type"); break;
    }
  }
}