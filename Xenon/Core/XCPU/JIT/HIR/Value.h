/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#pragma once

#include <cstdint>

#include "Base/Assert.h"
#include "Base/Vector128.h"
#include "Core/XCPU/JIT/HIR/Arena.h"
#include "Core/XCPU/JIT/HIR/Opcodes.h"

namespace Xe::XCPU::HIR {

class Instr;

using vec128_t = Base::Vector128;

enum TypeName {
  // Many tables rely on this ordering.
  INT8_TYPE = 0,
  INT16_TYPE = 1,
  INT32_TYPE = 2,
  INT64_TYPE = 3,
  FLOAT32_TYPE = 4,
  FLOAT64_TYPE = 5,
  VEC128_TYPE = 6,
  MAX_TYPENAME,
};

inline size_t GetTypeSize(TypeName type_name) {
  switch (type_name) {
  case INT8_TYPE:
    return 1;
  case INT16_TYPE:
    return 2;
  case INT32_TYPE:
    return 4;
  case INT64_TYPE:
    return 8;
  case FLOAT32_TYPE:
    return 4;
  case FLOAT64_TYPE:
    return 8;
  case VEC128_TYPE:
    return 16;
  default:
    UNREACHABLE();
    return 0;
  }
}

enum ValueFlags {
  VALUE_IS_CONSTANT = (1 << 1),
  VALUE_IS_ALLOCATED = (1 << 2),  // Used by backends. Do not set.
};

struct RegAssignment {
  //**********//const backend::MachineInfo::RegisterSet *set;
  s32 index;
};

class Value {
public:
  typedef struct Use_s {
    Instr *instr;
    Use_s *prev;
    Use_s *next;
  } Use;
  typedef union {
    s8 i8;
    uint8_t u8;
    s16 i16;
    uint16_t u16;
    s32 i32;
    uint32_t u32;
    s64 i64;
    uint64_t u64;
    float f32;
    double f64;
    vec128_t v128;
  } ConstantValue;

public:
  u32 ordinal;
  TypeName type;

  u32 flags;
  RegAssignment reg;
  ConstantValue constant;

  Instr *def;
  Use *use_head;
  // NOTE: for performance reasons this is not maintained during construction.
  Instr *last_use;
  Value *local_slot;

  // TODO(benvanik): remove to shrink size.
  void *tag;

  Use *AddUse(Arena *arena, Instr *instr);
  void RemoveUse(Use *use);

  void set_zero(TypeName newType) {
    type = newType;
    flags |= VALUE_IS_CONSTANT;
    constant.v128.qword[0] = constant.v128.qword[1] = 0;
  }
  void set_constant(s8 value) {
    type = INT8_TYPE;
    flags |= VALUE_IS_CONSTANT;
    constant.i64 = s64(value);
  }
  void set_constant(u8 value) {
    type = INT8_TYPE;
    flags |= VALUE_IS_CONSTANT;
    constant.i64 = u64(value);
  }
  void set_constant(s16 value) {
    type = INT16_TYPE;
    flags |= VALUE_IS_CONSTANT;
    constant.i64 = s64(value);
  }
  void set_constant(u16 value) {
    type = INT16_TYPE;
    flags |= VALUE_IS_CONSTANT;
    constant.i64 = u64(value);
  }
  void set_constant(s32 value) {
    type = INT32_TYPE;
    flags |= VALUE_IS_CONSTANT;
    constant.i64 = s64(value);
  }
  void set_constant(u32 value) {
    type = INT32_TYPE;
    flags |= VALUE_IS_CONSTANT;
    constant.i64 = u64(value);
  }
  void set_constant(s64 value) {
    type = INT64_TYPE;
    flags |= VALUE_IS_CONSTANT;
    constant.i64 = value;
  }
  void set_constant(u64 value) {
    type = INT64_TYPE;
    flags |= VALUE_IS_CONSTANT;
    constant.i64 = value;
  }
  void set_constant(f32 value) {
    type = FLOAT32_TYPE;
    flags |= VALUE_IS_CONSTANT;
    constant.f32 = value;
  }
  void set_constant(f64 value) {
    type = FLOAT64_TYPE;
    flags |= VALUE_IS_CONSTANT;
    constant.f64 = value;
  }
  void set_constant(const vec128_t &value) {
    type = VEC128_TYPE;
    flags |= VALUE_IS_CONSTANT;
    constant.v128 = value;
  }
  void set_from(const Value *other) {
    ASSERT(other->IsConstant());
    type = other->type;
    flags = other->flags;
    constant.v128 = other->constant.v128;
  }

  inline bool IsConstant() const { return !!(flags & VALUE_IS_CONSTANT); }
  bool IsConstantTrue() const {
    if (type == VEC128_TYPE) {
      UNREACHABLE();
    }
    if (flags & VALUE_IS_CONSTANT) {
      switch (type) {
      case INT8_TYPE:
        return !!constant.i8;
      case INT16_TYPE:
        return !!constant.i16;
      case INT32_TYPE:
        return !!constant.i32;
      case INT64_TYPE:
        return !!constant.i64;
      case FLOAT32_TYPE:
        return !!constant.f32;
      case FLOAT64_TYPE:
        return !!constant.f64;
      case VEC128_TYPE:
        return constant.v128.qword[0] || constant.v128.qword[1];
      default:
        UNREACHABLE();
        return false;
      }
    }
    else {
      return false;
    }
  }
  bool IsConstantFalse() const {
    if (flags & VALUE_IS_CONSTANT) {
      switch (type) {
      case INT8_TYPE:
        return !constant.i8;
      case INT16_TYPE:
        return !constant.i16;
      case INT32_TYPE:
        return !constant.i32;
      case INT64_TYPE:
        return !constant.i64;
      case FLOAT32_TYPE:
        return !constant.f32;
      case FLOAT64_TYPE:
        return !constant.f64;
      case VEC128_TYPE:
        return !(constant.v128.qword[0] || constant.v128.qword[1]);
      default:
        UNREACHABLE();
        return false;
      }
    } else {
      return false;
    }
  }
  bool IsConstantZero() const {
    if (flags & VALUE_IS_CONSTANT) {
      switch (type) {
      case INT8_TYPE:
        return !constant.i8;
      case INT16_TYPE:
        return !constant.i16;
      case INT32_TYPE:
        return !constant.i32;
      case INT64_TYPE:
        return !constant.i64;
      case FLOAT32_TYPE:
        return !constant.f32;
      case FLOAT64_TYPE:
        return !constant.f64;
      case VEC128_TYPE:
        return !constant.v128.qword[0] && !constant.v128.qword[1];
      default:
        UNREACHABLE();
        return false;
      }
    } else {
      return false;
    }
  }
  bool IsConstantOne() const {
    if (flags & VALUE_IS_CONSTANT) {
      switch (type) {
      case INT8_TYPE:
        return constant.i8 == 1;
      case INT16_TYPE:
        return constant.i16 == 1;
      case INT32_TYPE:
        return constant.i32 == 1;
      case INT64_TYPE:
        return constant.i64 == 1;
      case FLOAT32_TYPE:
        return constant.f32 == 1.f;
      case FLOAT64_TYPE:
        return constant.f64 == 1.0;
      default:
        UNREACHABLE();
        return false;
      }
    } else {
      return false;
    }
  }
  bool IsConstantEQ(Value *other) const {
    if (type == VEC128_TYPE) {
      UNREACHABLE();
    }
    if ((flags & VALUE_IS_CONSTANT) && (other->flags & VALUE_IS_CONSTANT)) {
      switch (type) {
      case INT8_TYPE:
        return constant.i8 == other->constant.i8;
      case INT16_TYPE:
        return constant.i16 == other->constant.i16;
      case INT32_TYPE:
        return constant.i32 == other->constant.i32;
      case INT64_TYPE:
        return constant.i64 == other->constant.i64;
      case FLOAT32_TYPE:
        return constant.f32 == other->constant.f32;
      case FLOAT64_TYPE:
        return constant.f64 == other->constant.f64;
      default:
        UNREACHABLE();
        return false;
      }
    } else {
      return false;
    }
  }
  bool IsConstantNE(Value *other) const {
    if (type == VEC128_TYPE) {
      UNREACHABLE();
    }
    if ((flags & VALUE_IS_CONSTANT) && (other->flags & VALUE_IS_CONSTANT)) {
      switch (type) {
      case INT8_TYPE:
        return constant.i8 != other->constant.i8;
      case INT16_TYPE:
        return constant.i16 != other->constant.i16;
      case INT32_TYPE:
        return constant.i32 != other->constant.i32;
      case INT64_TYPE:
        return constant.i64 != other->constant.i64;
      case FLOAT32_TYPE:
        return constant.f32 != other->constant.f32;
      case FLOAT64_TYPE:
        return constant.f64 != other->constant.f64;
      default:
        UNREACHABLE();
        return false;
      }
    } else {
      return false;
    }
  }
  bool IsConstantSLT(Value *other) const {
    ASSERT(flags & VALUE_IS_CONSTANT && other->flags & VALUE_IS_CONSTANT);
    switch (type) {
    case INT8_TYPE:
      return constant.i8 < other->constant.i8;
    case INT16_TYPE:
      return constant.i16 < other->constant.i16;
    case INT32_TYPE:
      return constant.i32 < other->constant.i32;
    case INT64_TYPE:
      return constant.i64 < other->constant.i64;
    case FLOAT32_TYPE:
      return constant.f32 < other->constant.f32;
    case FLOAT64_TYPE:
      return constant.f64 < other->constant.f64;
    default:
      UNREACHABLE();
      return false;
    }
  }
  bool IsConstantSLE(Value *other) const {
    ASSERT(flags & VALUE_IS_CONSTANT && other->flags & VALUE_IS_CONSTANT);
    switch (type) {
    case INT8_TYPE:
      return constant.i8 <= other->constant.i8;
    case INT16_TYPE:
      return constant.i16 <= other->constant.i16;
    case INT32_TYPE:
      return constant.i32 <= other->constant.i32;
    case INT64_TYPE:
      return constant.i64 <= other->constant.i64;
    case FLOAT32_TYPE:
      return constant.f32 <= other->constant.f32;
    case FLOAT64_TYPE:
      return constant.f64 <= other->constant.f64;
    default:
      UNREACHABLE();
      return false;
    }
  }
  bool IsConstantSGT(Value *other) const {
    ASSERT(flags & VALUE_IS_CONSTANT && other->flags & VALUE_IS_CONSTANT);
    switch (type) {
    case INT8_TYPE:
      return constant.i8 > other->constant.i8;
    case INT16_TYPE:
      return constant.i16 > other->constant.i16;
    case INT32_TYPE:
      return constant.i32 > other->constant.i32;
    case INT64_TYPE:
      return constant.i64 > other->constant.i64;
    case FLOAT32_TYPE:
      return constant.f32 > other->constant.f32;
    case FLOAT64_TYPE:
      return constant.f64 > other->constant.f64;
    default:
      UNREACHABLE();
      return false;
    }
  }
  bool IsConstantSGE(Value *other) const {
    ASSERT(flags & VALUE_IS_CONSTANT && other->flags & VALUE_IS_CONSTANT);
    switch (type) {
    case INT8_TYPE:
      return constant.i8 >= other->constant.i8;
    case INT16_TYPE:
      return constant.i16 >= other->constant.i16;
    case INT32_TYPE:
      return constant.i32 >= other->constant.i32;
    case INT64_TYPE:
      return constant.i64 >= other->constant.i64;
    case FLOAT32_TYPE:
      return constant.f32 >= other->constant.f32;
    case FLOAT64_TYPE:
      return constant.f64 >= other->constant.f64;
    default:
      UNREACHABLE();
      return false;
    }
  }
  bool IsConstantULT(Value *other) const {
    ASSERT(flags & VALUE_IS_CONSTANT && other->flags & VALUE_IS_CONSTANT);
    switch (type) {
    case INT8_TYPE:
      return (u8)constant.i8 < (u8)other->constant.i8;
    case INT16_TYPE:
      return (u16)constant.i16 < (u16)other->constant.i16;
    case INT32_TYPE:
      return (u32)constant.i32 < (u32)other->constant.i32;
    case INT64_TYPE:
      return (u64)constant.i64 < (u64)other->constant.i64;
    case FLOAT32_TYPE:
      return constant.f32 < other->constant.f32;
    case FLOAT64_TYPE:
      return constant.f64 < other->constant.f64;
    default:
      UNREACHABLE();
      return false;
    }
  }
  bool IsConstantULE(Value *other) const {
    ASSERT(flags & VALUE_IS_CONSTANT && other->flags & VALUE_IS_CONSTANT);
    switch (type) {
    case INT8_TYPE:
      return (u8)constant.i8 <= (u8)other->constant.i8;
    case INT16_TYPE:
      return (u16)constant.i16 <= (u16)other->constant.i16;
    case INT32_TYPE:
      return (u32)constant.i32 <= (u32)other->constant.i32;
    case INT64_TYPE:
      return (u64)constant.i64 <= (u64)other->constant.i64;
    case FLOAT32_TYPE:
      return constant.f32 <= other->constant.f32;
    case FLOAT64_TYPE:
      return constant.f64 <= other->constant.f64;
    default:
      UNREACHABLE();
      return false;
    }
  }
  bool IsConstantUGT(Value *other) const {
    ASSERT(flags & VALUE_IS_CONSTANT && other->flags & VALUE_IS_CONSTANT);
    switch (type) {
    case INT8_TYPE:
      return (u8)constant.i8 > (u8)other->constant.i8;
    case INT16_TYPE:
      return (u16)constant.i16 > (u16)other->constant.i16;
    case INT32_TYPE:
      return (u32)constant.i32 > (u32)other->constant.i32;
    case INT64_TYPE:
      return (u64)constant.i64 > (u64)other->constant.i64;
    case FLOAT32_TYPE:
      return constant.f32 > other->constant.f32;
    case FLOAT64_TYPE:
      return constant.f64 > other->constant.f64;
    default:
      UNREACHABLE();
      return false;
    }
  }
  bool IsConstantUGE(Value *other) const {
    ASSERT(flags & VALUE_IS_CONSTANT && other->flags & VALUE_IS_CONSTANT);
    switch (type) {
    case INT8_TYPE:
      return (u8)constant.i8 >= (u8)other->constant.i8;
    case INT16_TYPE:
      return (u16)constant.i16 >= (u16)other->constant.i16;
    case INT32_TYPE:
      return (u32)constant.i32 >= (u32)other->constant.i32;
    case INT64_TYPE:
      return (u64)constant.i64 >= (u64)other->constant.i64;
    case FLOAT32_TYPE:
      return constant.f32 >= other->constant.f32;
    case FLOAT64_TYPE:
      return constant.f64 >= other->constant.f64;
    default:
      UNREACHABLE();
      return false;
    }
  }
  u32 AsUint32();
  u64 AsUint64();

  void Cast(TypeName target_type);
  void ZeroExtend(TypeName target_type);
  void SignExtend(TypeName target_type);
  void Truncate(TypeName target_type);
  void Convert(TypeName target_type, RoundMode round_mode);
  void Round(RoundMode round_mode);
  bool Add(Value *other);
  bool Sub(Value *other);
  void Mul(Value *other);
  void MulHi(Value *other, bool isUnsigned);
  void Div(Value *other, bool isUnsigned);
  void Max(Value *other);
  static void MulAdd(Value *dest, Value *value1, Value *value2, Value *value3);
  static void MulSub(Value *dest, Value *value1, Value *value2, Value *value3);
  void Neg();
  void Abs();
  void Sqrt();
  void RSqrt();
  void Recip();
  void And(Value *other);
  void Or(Value *other);
  void Xor(Value *other);
  void Not();
  void Shl(Value *other);
  void Shr(Value *other);
  void Sha(Value *other);
  void Extract(Value *vec, Value *index);
  void Select(Value *other, Value *ctrl);
  void Splat(Value *other);
  void VectorCompareEQ(Value *other, TypeName type);
  void VectorCompareSGT(Value *other, TypeName type);
  void VectorCompareSGE(Value *other, TypeName type);
  void VectorCompareUGT(Value *other, TypeName type);
  void VectorCompareUGE(Value *other, TypeName type);
  void VectorConvertI2F(Value *other, bool isUnsigned);
  void VectorConvertF2I(Value *other, bool isUnsigned);
  void VectorShl(Value *other, TypeName type);
  void VectorShr(Value *other, TypeName type);
  void VectorRol(Value *other, TypeName type);
  void VectorAdd(Value *other, TypeName type, bool isUnsigned, bool saturate);
  void VectorSub(Value *other, TypeName type, bool isUnsigned, bool saturate);
  void DotProduct3(Value *other);
  void DotProduct4(Value *other);
  void VectorAverage(Value *other, TypeName type, bool isUnsigned,
    bool saturate);
  void ByteSwap();
  void CountLeadingZeros(const Value *other);
  bool Compare(Opcode opcode, Value *other);

private:
  static bool CompareInt8(Opcode opcode, Value *a, Value *b);
  static bool CompareInt16(Opcode opcode, Value *a, Value *b);
  static bool CompareInt32(Opcode opcode, Value *a, Value *b);
  static bool CompareInt64(Opcode opcode, Value *a, Value *b);
};

}  // namespace Xe::XCPU::HIR