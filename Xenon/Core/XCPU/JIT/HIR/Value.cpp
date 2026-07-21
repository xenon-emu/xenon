/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2021 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <cmath>
#include <cstdlib>

#include "Base/Assert.h"
//#include "Base/MathUtils.h"
#include "Core/XCPU/JIT/HIR/Value.h"

namespace Xe::XCPU::HIR {

Value::Use *Value::AddUse(Arena *arena, Instr *instr) {
  Use *use = arena->Alloc<Use>();
  use->instr = instr;
  use->prev = NULL;
  use->next = use_head;
  if (use_head) {
    use_head->prev = use;
  }
  use_head = use;
  return use;
}

void Value::RemoveUse(Use *use) {
  if (use == use_head) {
    use_head = use->next;
  } else {
    use->prev->next = use->next;
  }

  if (use->next) {
    use->next->prev = use->prev;
  }
}

u32 Value::AsUint32() {
  ASSERT(IsConstant());
  switch (type) {
  case INT8_TYPE:
    return constant.u8;
  case INT16_TYPE:
    return constant.u16;
  case INT32_TYPE:
    return constant.u32;
  case INT64_TYPE:
    return (u32)constant.u64;
  default:
    UNREACHABLE();
    return 0;
  }
}

u64 Value::AsUint64() {
  ASSERT(IsConstant());
  switch (type) {
  case INT8_TYPE:
    return constant.u8;
  case INT16_TYPE:
    return constant.u16;
  case INT32_TYPE:
    return constant.u32;
  case INT64_TYPE:
    return constant.u64;
  default:
    UNREACHABLE();
    return 0;
  }
}

void Value::Cast(TypeName targetType) {
  // Only need a type change.
  type = targetType;
}

void Value::ZeroExtend(TypeName targetType) {
  switch (type) {
  case INT8_TYPE:
    type = targetType;
    constant.u64 = constant.u8;
    return;
  case INT16_TYPE:
    type = targetType;
    constant.u64 = constant.u16;
    return;
  case INT32_TYPE:
    type = targetType;
    constant.u64 = constant.u32;
    return;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::SignExtend(TypeName targetType) {
  switch (type) {
  case INT8_TYPE:
    type = targetType;
    switch (targetType) {
    case INT16_TYPE:
      constant.i16 = constant.i8;
      return;
    case INT32_TYPE:
      constant.i32 = constant.i8;
      return;
    case INT64_TYPE:
      constant.i64 = constant.i8;
      return;
    default:
      UNREACHABLE();
      return;
    }
  case INT16_TYPE:
    type = targetType;
    switch (targetType) {
    case INT32_TYPE:
      constant.i32 = constant.i16;
      return;
    case INT64_TYPE:
      constant.i64 = constant.i16;
      return;
    default:
      UNREACHABLE();
      return;
    }
  case INT32_TYPE:
    type = targetType;
    switch (targetType) {
    case INT64_TYPE:
      constant.i64 = constant.i32;
      return;
    default:
      UNREACHABLE();
      return;
    }
  default:
    UNREACHABLE();
    return;
  }
}

void Value::Truncate(TypeName targetType) {
  switch (type) {
  case INT16_TYPE:
    switch (targetType) {
    case INT8_TYPE:
      type = targetType;
      constant.i64 = constant.i64 & 0xFF;
      return;
    default:
      UNREACHABLE();
      return;
    }
  case INT32_TYPE:
    switch (targetType) {
    case INT8_TYPE:
      type = targetType;
      constant.i64 = constant.i64 & 0xFF;
      return;
    case INT16_TYPE:
      type = targetType;
      constant.i64 = constant.i64 & 0xFFFF;
      return;
    default:
      UNREACHABLE();
      return;
    }
  case INT64_TYPE:
    switch (targetType) {
    case INT8_TYPE:
      type = targetType;
      constant.i64 = constant.i64 & 0xFF;
      return;
    case INT16_TYPE:
      type = targetType;
      constant.i64 = constant.i64 & 0xFFFF;
      return;
    case INT32_TYPE:
      type = targetType;
      constant.i64 = constant.i64 & 0xFFFFFFFF;
      return;
    default:
      UNREACHABLE();
      return;
    }
  default:
    UNREACHABLE();
    return;
  }
}

void Value::Convert(TypeName targetType, RoundMode round_mode) {
  switch (type) {
  case FLOAT32_TYPE:
    switch (targetType) {
    case FLOAT64_TYPE:
      type = targetType;
      constant.f64 = constant.f32;
      return;
    default:
      UNREACHABLE();
      return;
    }
  case INT64_TYPE:
    switch (targetType) {
    case FLOAT64_TYPE:
      type = targetType;
      constant.f64 = (f64)constant.i64;
      return;
    default:
      UNREACHABLE();
      return;
    }
  case FLOAT64_TYPE:
    switch (targetType) {
    case FLOAT32_TYPE:
      type = targetType;
      constant.f32 = (f32)constant.f64;
      return;
    case INT32_TYPE:
      type = targetType;
      constant.i32 = (s32)constant.f64;
      return;
    case INT64_TYPE:
      type = targetType;
      constant.i64 = (s64)constant.f64;
      return;
    default:
      UNREACHABLE();
      return;
    }
  default:
    UNREACHABLE();
    return;
  }
}

template <typename T>
T __inline RoundValue(RoundMode round_mode, T value) {
  switch (round_mode) {
  case ROUND_TO_ZERO:
    return std::trunc(value);
  case ROUND_TO_NEAREST:
    return std::round(value);
  case ROUND_TO_MINUS_INFINITY:
    return std::floor(value);
  case ROUND_TO_POSITIVE_INFINITY:
    return std::ceil(value);
  default:
    UNREACHABLE();
    return value;
  }
}

void Value::Round(RoundMode round_mode) {
  switch (type) {
  case FLOAT32_TYPE:
    constant.f32 = RoundValue(round_mode, constant.f32);
    return;
  case FLOAT64_TYPE:
    constant.f64 = RoundValue(round_mode, constant.f64);
    return;
  case VEC128_TYPE:
    for (s32 i = 0; i < 4; i++) {
      constant.v128.flt[i] = RoundValue(round_mode, constant.v128.flt[i]);
    }
    return;
  default:
    UNREACHABLE();
    return;
  }
}

bool Value::Add(Value *other) {
#define CHECK_DID_CARRY(v1, v2) (((u64)v2) > ~((u64)v1))
#define ADD_DID_CARRY(a, b) CHECK_DID_CARRY(a, b)
  ASSERT(type == other->type);
  bool did_carry = false;
  switch (type) {
  case INT8_TYPE:
    did_carry = ADD_DID_CARRY(constant.i8, other->constant.i8);
    constant.i8 += other->constant.i8;
    break;
  case INT16_TYPE:
    did_carry = ADD_DID_CARRY(constant.i16, other->constant.i16);
    constant.i16 += other->constant.i16;
    break;
  case INT32_TYPE:
    did_carry = ADD_DID_CARRY(constant.i32, other->constant.i32);
    constant.i32 += other->constant.i32;
    break;
  case INT64_TYPE:
    did_carry = ADD_DID_CARRY(constant.i64, other->constant.i64);
    constant.i64 += other->constant.i64;
    break;
  case FLOAT32_TYPE:
    constant.f32 += other->constant.f32;
    break;
  case FLOAT64_TYPE:
    constant.f64 += other->constant.f64;
    break;
  default:
    UNREACHABLE();
    break;
  }
  return did_carry;
}

bool Value::Sub(Value *other) {
#define SUB_DID_CARRY(a, b) (b == 0 || a > (~(0 - b)))
  ASSERT(type == other->type);
  bool did_carry = false;
  switch (type) {
  case INT8_TYPE:
    did_carry =
      SUB_DID_CARRY(u16(constant.i8), u16(other->constant.i8));
    constant.i8 -= other->constant.i8;
    break;
  case INT16_TYPE:
    did_carry =
      SUB_DID_CARRY(u16(constant.i16), u16(other->constant.i16));
    constant.i16 -= other->constant.i16;
    break;
  case INT32_TYPE:
    did_carry =
      SUB_DID_CARRY(u32(constant.i32), u32(other->constant.i32));
    constant.i32 -= other->constant.i32;
    break;
  case INT64_TYPE:
    did_carry =
      SUB_DID_CARRY(u64(constant.i64), u64(other->constant.i64));
    constant.i64 -= other->constant.i64;
    break;
  case FLOAT32_TYPE:
    constant.f32 -= other->constant.f32;
    break;
  case FLOAT64_TYPE:
    constant.f64 -= other->constant.f64;
    break;
  default:
    UNREACHABLE();
    break;
  }
  return did_carry;
}

void Value::Mul(Value *other) {
  ASSERT(type == other->type);
  switch (type) {
  case INT8_TYPE:
    constant.i8 *= other->constant.i8;
    break;
  case INT16_TYPE:
    constant.i16 *= other->constant.i16;
    break;
  case INT32_TYPE:
    constant.i32 *= other->constant.i32;
    break;
  case INT64_TYPE:
    constant.i64 *= other->constant.i64;
    break;
  case FLOAT32_TYPE:
    constant.f32 *= other->constant.f32;
    break;
  case FLOAT64_TYPE:
    constant.f64 *= other->constant.f64;
    break;
  case VEC128_TYPE:
    for (s32 i = 0; i < 4; i++) {
      constant.v128.flt[i] *= other->constant.v128.flt[i];
    }
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::MulHi(Value *other, bool isUnsigned) {
  ASSERT(type == other->type);
  switch (type) {
  case INT32_TYPE:
    if (isUnsigned) {
      constant.i32 = (s32)(((u64)((u32)constant.i32) *
        (u32)other->constant.i32) >>
        32);
    }
    else {
      constant.i32 =
        (s32)(((s64)constant.i32 * (s64)other->constant.i32) >>
          32);
    }
    break;
  case INT64_TYPE:
#if _MSC_VER
    if (isUnsigned) {
      constant.i64 = __umulh(constant.i64, other->constant.i64);
    } else {
      constant.i64 = __mulh(constant.i64, other->constant.i64);
    }
#else
    if (isUnsigned) {
      constant.i64 = static_cast<u64>(
        static_cast<unsigned __int128>(constant.i64) *
        static_cast<unsigned __int128>(other->constant.i64));
    } else {
      constant.i64 =
        static_cast<u64>(static_cast<__int128>(constant.i64) *
          static_cast<__int128>(other->constant.i64));
    }
#endif // XE_COMPILER_MSVC
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::Div(Value *other, bool isUnsigned) {
  ASSERT(type == other->type);
  switch (type) {
  case INT8_TYPE:
    if (isUnsigned) {
      constant.i8 /= u8(other->constant.i8);
    }
    else {
      constant.i8 /= other->constant.i8;
    }
    break;
  case INT16_TYPE:
    if (isUnsigned) {
      constant.i16 /= u16(other->constant.i16);
    }
    else {
      constant.i16 /= other->constant.i16;
    }
    break;
  case INT32_TYPE:
    if (isUnsigned) {
      constant.i32 /= u32(other->constant.i32);
    }
    else {
      constant.i32 /= other->constant.i32;
    }
    break;
  case INT64_TYPE:
    if (isUnsigned) {
      constant.i64 /= u64(other->constant.i64);
    }
    else {
      constant.i64 /= other->constant.i64;
    }
    break;
  case FLOAT32_TYPE:
    constant.f32 /= other->constant.f32;
    break;
  case FLOAT64_TYPE:
    constant.f64 /= other->constant.f64;
    break;
  case VEC128_TYPE:
    for (s32 i = 0; i < 4; i++) {
      constant.v128.flt[i] /= other->constant.v128.flt[i];
    }
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::Max(Value *other) {
  ASSERT(type == other->type);
  switch (type) {
  case FLOAT32_TYPE:
    constant.f32 = std::max(constant.f32, other->constant.f32);
    break;
  case FLOAT64_TYPE:
    constant.f64 = std::max(constant.f64, other->constant.f64);
    break;
  case VEC128_TYPE:
    for (s32 i = 0; i < 4; i++) {
      constant.v128.flt[i] =
        std::max(constant.v128.flt[i], other->constant.v128.flt[i]);
    }
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::MulAdd(Value *dest, Value *value1, Value *value2, Value *value3) {
  switch (dest->type) {
  case VEC128_TYPE:
    for (s32 i = 0; i < 4; i++) {
      dest->constant.v128.flt[i] =
        (value1->constant.v128.flt[i] * value2->constant.v128.flt[i]) +
        value3->constant.v128.flt[i];
    }
    break;
  case FLOAT32_TYPE:
    dest->constant.f32 =
      (value1->constant.f32 * value2->constant.f32) + value3->constant.f32;
    break;
  case FLOAT64_TYPE:
    dest->constant.f64 =
      (value1->constant.f64 * value2->constant.f64) + value3->constant.f64;
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::MulSub(Value *dest, Value *value1, Value *value2, Value *value3) {
  switch (dest->type) {
  case VEC128_TYPE:
    for (s32 i = 0; i < 4; i++) {
      dest->constant.v128.flt[i] =
        (value1->constant.v128.flt[i] * value2->constant.v128.flt[i]) -
        value3->constant.v128.flt[i];
    }
    break;
  case FLOAT32_TYPE:
    dest->constant.f32 =
      (value1->constant.f32 * value2->constant.f32) - value3->constant.f32;
    break;
  case FLOAT64_TYPE:
    dest->constant.f64 =
      (value1->constant.f64 * value2->constant.f64) - value3->constant.f64;
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::Neg() {
  switch (type) {
  case INT8_TYPE:
    constant.i8 = -constant.i8;
    break;
  case INT16_TYPE:
    constant.i16 = -constant.i16;
    break;
  case INT32_TYPE:
    constant.i32 = -constant.i32;
    break;
  case INT64_TYPE:
    constant.i64 = -constant.i64;
    break;
  case FLOAT32_TYPE:
    constant.f32 = -constant.f32;
    break;
  case FLOAT64_TYPE:
    constant.f64 = -constant.f64;
    break;
  case VEC128_TYPE:
    for (s32 i = 0; i < 4; ++i) {
      constant.v128.flt[i] = -constant.v128.flt[i];
    }
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::Abs() {
  switch (type) {
  case INT8_TYPE:
    constant.i8 = s8(std::abs(constant.i8));
    break;
  case INT16_TYPE:
    constant.i16 = s16(std::abs(constant.i16));
    break;
  case INT32_TYPE:
    constant.i32 = std::abs(constant.i32);
    break;
  case INT64_TYPE:
    constant.i64 = std::abs(constant.i64);
    break;
  case FLOAT32_TYPE:
    constant.f32 = std::abs(constant.f32);
    break;
  case FLOAT64_TYPE:
    constant.f64 = std::abs(constant.f64);
    break;
  case VEC128_TYPE:
    for (s32 i = 0; i < 4; ++i) {
      constant.v128.flt[i] = std::abs(constant.v128.flt[i]);
    }
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::Sqrt() {
  switch (type) {
  case FLOAT32_TYPE:
    constant.f32 = std::sqrt(constant.f32);
    break;
  case FLOAT64_TYPE:
    constant.f64 = std::sqrt(constant.f64);
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::RSqrt() {
  switch (type) {
  case FLOAT32_TYPE:
    constant.f32 = 1.0f / std::sqrt(constant.f32);
    break;
  case FLOAT64_TYPE:
    constant.f64 = 1.0f / std::sqrt(constant.f64);
    break;
  case VEC128_TYPE:
    for (s32 i = 0; i < 4; ++i) {
      constant.v128.flt[i] = 1.0f / std::sqrt(constant.v128.flt[i]);
    }
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::Recip() {
  switch (type) {
  case FLOAT32_TYPE:
    constant.f32 = 1.0f / constant.f32;
    break;
  case FLOAT64_TYPE:
    constant.f64 = 1.0f / constant.f64;
    break;
  case VEC128_TYPE:
    for (s32 i = 0; i < 4; i++) {
      constant.v128.flt[i] = 1.0f / constant.v128.flt[i];
    }
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::And(Value *other) {
  ASSERT(type == other->type);
  switch (type) {
  case INT8_TYPE:
    constant.i8 &= other->constant.i8;
    break;
  case INT16_TYPE:
    constant.i16 &= other->constant.i16;
    break;
  case INT32_TYPE:
    constant.i32 &= other->constant.i32;
    break;
  case INT64_TYPE:
    constant.i64 &= other->constant.i64;
    break;
  case VEC128_TYPE:
    constant.v128 &= other->constant.v128;
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::Or(Value *other) {
  ASSERT(type == other->type);
  switch (type) {
  case INT8_TYPE:
    constant.i8 |= other->constant.i8;
    break;
  case INT16_TYPE:
    constant.i16 |= other->constant.i16;
    break;
  case INT32_TYPE:
    constant.i32 |= other->constant.i32;
    break;
  case INT64_TYPE:
    constant.i64 |= other->constant.i64;
    break;
  case VEC128_TYPE:
    constant.v128 |= other->constant.v128;
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::Xor(Value *other) {
  ASSERT(type == other->type);
  switch (type) {
  case INT8_TYPE:
    constant.i8 ^= other->constant.i8;
    break;
  case INT16_TYPE:
    constant.i16 ^= other->constant.i16;
    break;
  case INT32_TYPE:
    constant.i32 ^= other->constant.i32;
    break;
  case INT64_TYPE:
    constant.i64 ^= other->constant.i64;
    break;
  case VEC128_TYPE:
    constant.v128 ^= other->constant.v128;
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::Not() {
  switch (type) {
  case INT8_TYPE:
    constant.i8 = ~constant.i8;
    break;
  case INT16_TYPE:
    constant.i16 = ~constant.i16;
    break;
  case INT32_TYPE:
    constant.i32 = ~constant.i32;
    break;
  case INT64_TYPE:
    constant.i64 = ~constant.i64;
    break;
  case VEC128_TYPE:
    constant.v128.qword[0] = ~constant.v128.qword[0];
    constant.v128.qword[1] = ~constant.v128.qword[1];
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::Shl(Value *other) {
  ASSERT(other->type == INT8_TYPE);
  switch (type) {
  case INT8_TYPE:
    constant.u8 <<= other->constant.u8;
    break;
  case INT16_TYPE:
    constant.u16 <<= other->constant.u8;
    break;
  case INT32_TYPE:
    constant.u32 <<= other->constant.u8;
    break;
  case INT64_TYPE:
    constant.u64 <<= other->constant.u8;
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::Shr(Value *other) {
  ASSERT(other->type == INT8_TYPE);
  switch (type) {
  case INT8_TYPE:
    constant.u8 = constant.u8 >> other->constant.u8;
    break;
  case INT16_TYPE:
    constant.u16 = constant.u16 >> other->constant.u8;
    break;
  case INT32_TYPE:
    constant.u32 = constant.u32 >> other->constant.u8;
    break;
  case INT64_TYPE:
    constant.u64 = constant.u64 >> other->constant.u8;
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::Sha(Value *other) {
  ASSERT(other->type == INT8_TYPE);
  switch (type) {
  case INT8_TYPE:
    constant.i8 = constant.i8 >> other->constant.u8;
    break;
  case INT16_TYPE:
    constant.i16 = constant.i16 >> other->constant.u8;
    break;
  case INT32_TYPE:
    constant.i32 = constant.i32 >> other->constant.u8;
    break;
  case INT64_TYPE:
    constant.i64 = constant.i64 >> other->constant.u8;
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::Extract(Value *vec, Value *index) {
  ASSERT(vec->type == VEC128_TYPE);
  switch (type) {
  case INT8_TYPE:
    constant.u8 = vec->constant.v128.bytes[index->constant.u8 & 0x1F];
    break;
  case INT16_TYPE:
    constant.u16 = vec->constant.v128.word[index->constant.u16 & 0x7];
    break;
  case INT32_TYPE:
    constant.u32 = vec->constant.v128.dword[index->constant.u32 & 0x3];
    break;
  case INT64_TYPE:
    constant.u64 = vec->constant.v128.qword[index->constant.u64 & 0x1];
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::Select(Value *other, Value *ctrl) {
  // TODO
  UNREACHABLE();
}

void Value::Splat(Value *other) {
  ASSERT(type == VEC128_TYPE);
  switch (other->type) {
  case INT8_TYPE:
    for (s32 i = 0; i < 16; i++) {
      constant.v128.sbytes[i] = other->constant.i8;
    }
    break;
  case INT16_TYPE:
    for (s32 i = 0; i < 8; i++) {
      constant.v128.sword[i] = other->constant.i16;
    }
    break;
  case INT32_TYPE:
  case FLOAT32_TYPE:
    for (s32 i = 0; i < 4; i++) {
      constant.v128.dsword[i] = other->constant.i32;
    }
    break;
  case INT64_TYPE:
  case FLOAT64_TYPE:
    for (s32 i = 0; i < 2; i++) {
      constant.v128.qsword[i] = other->constant.i64;
    }
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::VectorCompareEQ(Value *other, TypeName type) {
  ASSERT(this->type == VEC128_TYPE && other->type == VEC128_TYPE);
  switch (type) {
  case INT8_TYPE:
    for (s32 i = 0; i < 16; i++) {
      constant.v128.bytes[i] =
        constant.v128.bytes[i] == other->constant.v128.bytes[i] ? -1 : 0;
    }
    break;
  case INT16_TYPE:
    for (s32 i = 0; i < 8; i++) {
      constant.v128.word[i] =
        constant.v128.word[i] == other->constant.v128.word[i] ? -1 : 0;
    }
    break;
  case INT32_TYPE:
  case FLOAT32_TYPE:
    for (s32 i = 0; i < 4; i++) {
      constant.v128.dword[i] =
        constant.v128.dword[i] == other->constant.v128.dword[i] ? -1 : 0;
    }
    break;
  case INT64_TYPE:
  case FLOAT64_TYPE:
    for (s32 i = 0; i < 2; i++) {
      constant.v128.qword[i] =
        constant.v128.qword[i] == other->constant.v128.qword[i] ? -1 : 0;
    }
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::VectorCompareSGT(Value *other, TypeName type) {
  ASSERT(this->type == VEC128_TYPE && other->type == VEC128_TYPE);
  switch (type) {
  case INT8_TYPE:
    for (s32 i = 0; i < 16; i++) {
      constant.v128.bytes[i] =
        constant.v128.sbytes[i] > other->constant.v128.sbytes[i] ? -1 : 0;
    }
    break;
  case INT16_TYPE:
    for (s32 i = 0; i < 8; i++) {
      constant.v128.word[i] =
        constant.v128.sword[i] > other->constant.v128.sword[i] ? -1 : 0;
    }
    break;
  case INT32_TYPE:
    for (s32 i = 0; i < 4; i++) {
      constant.v128.dword[i] =
        constant.v128.dsword[i] > other->constant.v128.dsword[i] ? -1 : 0;
    }
    break;
  case INT64_TYPE:
    for (s32 i = 0; i < 2; i++) {
      constant.v128.qword[i] =
        constant.v128.qsword[i] > other->constant.v128.qsword[i] ? -1 : 0;
    }
    break;
  case FLOAT32_TYPE:
    for (s32 i = 0; i < 4; i++) {
      constant.v128.dword[i] =
        constant.v128.flt[i] > other->constant.v128.flt[i] ? -1 : 0;
    }
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::VectorCompareSGE(Value *other, TypeName type) {
  ASSERT(this->type == VEC128_TYPE && other->type == VEC128_TYPE);
  switch (type) {
  case INT8_TYPE:
    for (s32 i = 0; i < 16; i++) {
      constant.v128.bytes[i] =
        constant.v128.sbytes[i] >= other->constant.v128.sbytes[i] ? -1 : 0;
    }
    break;
  case INT16_TYPE:
    for (s32 i = 0; i < 8; i++) {
      constant.v128.word[i] =
        constant.v128.sword[i] >= other->constant.v128.sword[i] ? -1 : 0;
    }
    break;
  case INT32_TYPE:
    for (s32 i = 0; i < 4; i++) {
      constant.v128.dword[i] =
        constant.v128.dsword[i] >= other->constant.v128.dsword[i] ? -1 : 0;
    }
    break;
  case INT64_TYPE:
    for (s32 i = 0; i < 2; i++) {
      constant.v128.qword[i] =
        constant.v128.qsword[i] >= other->constant.v128.qsword[i] ? -1 : 0;
    }
    break;
  case FLOAT32_TYPE:
    for (s32 i = 0; i < 4; i++) {
      constant.v128.dword[i] =
        constant.v128.flt[i] >= other->constant.v128.flt[i] ? -1 : 0;
    }
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::VectorCompareUGT(Value *other, TypeName type) {
  ASSERT(this->type == VEC128_TYPE && other->type == VEC128_TYPE);
  switch (type) {
  case INT8_TYPE:
    for (s32 i = 0; i < 16; i++) {
      constant.v128.bytes[i] =
        constant.v128.bytes[i] > other->constant.v128.bytes[i] ? -1 : 0;
    }
    break;
  case INT16_TYPE:
    for (s32 i = 0; i < 8; i++) {
      constant.v128.word[i] =
        constant.v128.word[i] > other->constant.v128.word[i] ? -1 : 0;
    }
    break;
  case INT32_TYPE:
    for (s32 i = 0; i < 4; i++) {
      constant.v128.dword[i] =
        constant.v128.dword[i] > other->constant.v128.dword[i] ? -1 : 0;
    }
    break;
  case INT64_TYPE:
    for (s32 i = 0; i < 2; i++) {
      constant.v128.qword[i] =
        constant.v128.qword[i] > other->constant.v128.qword[i] ? -1 : 0;
    }
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::VectorCompareUGE(Value *other, TypeName type) {
  ASSERT(this->type == VEC128_TYPE && other->type == VEC128_TYPE);
  switch (type) {
  case INT8_TYPE:
    for (s32 i = 0; i < 16; i++) {
      constant.v128.bytes[i] =
        constant.v128.bytes[i] >= other->constant.v128.bytes[i] ? -1 : 0;
    }
    break;
  case INT16_TYPE:
    for (s32 i = 0; i < 8; i++) {
      constant.v128.word[i] =
        constant.v128.word[i] >= other->constant.v128.word[i] ? -1 : 0;
    }
    break;
  case INT32_TYPE:
    for (s32 i = 0; i < 4; i++) {
      constant.v128.dword[i] =
        constant.v128.dword[i] >= other->constant.v128.dword[i] ? -1 : 0;
    }
    break;
  case INT64_TYPE:
    for (s32 i = 0; i < 2; i++) {
      constant.v128.qword[i] =
        constant.v128.qword[i] >= other->constant.v128.qword[i] ? -1 : 0;
    }
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::VectorConvertI2F(Value *other, bool isUnsigned) {
  ASSERT(type == VEC128_TYPE);
  for (s32 i = 0; i < 4; i++) {
    if (isUnsigned) {
      constant.v128.flt[i] = (f32)other->constant.v128.dword[i];
    }
    else {
      constant.v128.flt[i] = (f32)other->constant.v128.dsword[i];
    }
  }
}

void Value::VectorConvertF2I(Value *other, bool isUnsigned) {
  ASSERT(type == VEC128_TYPE);

  // FIXME(DrChat): This does not saturate!
  for (s32 i = 0; i < 4; i++) {
    if (isUnsigned) {
      constant.v128.dword[i] = (u32)other->constant.v128.flt[i];
    }
    else {
      constant.v128.dsword[i] = (s32)other->constant.v128.flt[i];
    }
  }
}

void Value::VectorShl(Value *other, TypeName type) {
  ASSERT(this->type == VEC128_TYPE && other->type == VEC128_TYPE);
  switch (type) {
  case INT8_TYPE:
    for (s32 i = 0; i < 16; i++) {
      constant.v128.bytes[i] <<= other->constant.v128.bytes[i] & 0x7;
    }
    break;
  case INT16_TYPE:
    for (s32 i = 0; i < 8; i++) {
      constant.v128.word[i] <<= other->constant.v128.word[i] & 0xF;
    }
    break;
  case INT32_TYPE:
    for (s32 i = 0; i < 4; i++) {
      constant.v128.dword[i] <<= other->constant.v128.dword[i] & 0x1F;
    }
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::VectorShr(Value *other, TypeName type) {
  ASSERT(this->type == VEC128_TYPE && other->type == VEC128_TYPE);
  switch (type) {
  case INT8_TYPE:
    for (s32 i = 0; i < 16; i++) {
      constant.v128.bytes[i] >>= other->constant.v128.bytes[i] & 0x7;
    }
    break;
  case INT16_TYPE:
    for (s32 i = 0; i < 8; i++) {
      constant.v128.word[i] >>= other->constant.v128.word[i] & 0xF;
    }
    break;
  case INT32_TYPE:
    for (s32 i = 0; i < 4; i++) {
      constant.v128.dword[i] >>= other->constant.v128.dword[i] & 0x1F;
    }
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::VectorRol(Value *other, TypeName type) {
  ASSERT(this->type == VEC128_TYPE && other->type == VEC128_TYPE);
  switch (type) {
  case INT8_TYPE:
    for (s32 i = 0; i < 16; i++) {
      constant.v128.bytes[i] = std::rotl(constant.v128.bytes[i],
        other->constant.v128.sbytes[i] & 0x7);
    }
    break;
  case INT16_TYPE:
    for (s32 i = 0; i < 8; i++) {
      constant.v128.word[i] = std::rotl(
        constant.v128.word[i], other->constant.v128.word[i] & 0xF);
    }
    break;
  case INT32_TYPE:
    for (s32 i = 0; i < 4; i++) {
      constant.v128.dword[i] = std::rotl(
        constant.v128.dword[i], other->constant.v128.dword[i] & 0x1F);
    }
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::VectorAdd(Value *other, TypeName type, bool isUnsigned,
  bool saturate) {
  ASSERT(this->type == VEC128_TYPE && other->type == VEC128_TYPE);
  switch (type) {
  case FLOAT32_TYPE:
    if (saturate) {
      UNREACHABLE();
    }
    else {
      constant.v128.x += other->constant.v128.x;
      constant.v128.y += other->constant.v128.y;
      constant.v128.z += other->constant.v128.z;
      constant.v128.w += other->constant.v128.w;
    }
    break;
  case FLOAT64_TYPE:
    if (saturate) {
      UNREACHABLE();
    }
    else {
      constant.v128.dbl[0] += other->constant.v128.dbl[0];
      constant.v128.dbl[1] += other->constant.v128.dbl[1];
    }
    break;
  case INT8_TYPE:
    if (saturate) {
      for (s32 i = 0; i < 16; i++) {
        if (isUnsigned) {
          //constant.v128.bytes[i] = Base::MathUtils::SaturateAdd(constant.v128.bytes[i], other->constant.v128.bytes[i]);
        } else {
          //constant.v128.sbytes[i] = Base::MathUtils::SaturateAdd(constant.v128.sbytes[i], other->constant.v128.sbytes[i]);
        }
      }
      // TODO(Triang3l): Trace DID_SATURATE.
    }
    else {
      for (s32 i = 0; i < 16; i++) {
        if (isUnsigned) {
          constant.v128.bytes[i] += other->constant.v128.bytes[i];
        }
        else {
          constant.v128.sbytes[i] += other->constant.v128.sbytes[i];
        }
      }
    }
    break;
  case INT16_TYPE:
    if (saturate) {
      for (s32 i = 0; i < 8; i++) {
        if (isUnsigned) {
          //constant.v128.word[i] = Base::MathUtils::SaturateAdd(constant.v128.word[i], other->constant.v128.word[i]);
        } else {
          //constant.v128.sword[i] = Base::MathUtils::SaturateAdd(constant.v128.sword[i], other->constant.v128.sword[i]);
        }
      }
      // TODO(Triang3l): Trace DID_SATURATE.
    } else {
      for (s32 i = 0; i < 8; i++) {
        if (isUnsigned) {
          constant.v128.word[i] += other->constant.v128.word[i];
        } else {
          constant.v128.sword[i] += other->constant.v128.sword[i];
        }
      }
    }
    break;
  case INT32_TYPE:
    if (saturate) {
      for (s32 i = 0; i < 4; i++) {
        if (isUnsigned) {
          //constant.v128.dword[i] = Base::MathUtils::SaturateAdd(constant.v128.dword[i], other->constant.v128.dword[i]);
        } else {
          //constant.v128.dsword[i] = Base::MathUtils::SaturateAdd(constant.v128.dsword[i], other->constant.v128.dsword[i]);
        }
      }
      // TODO(Triang3l): Trace DID_SATURATE.
    } else {
      for (s32 i = 0; i < 4; i++) {
        if (isUnsigned) {
          constant.v128.dword[i] += other->constant.v128.dword[i];
        }
        else {
          constant.v128.dsword[i] += other->constant.v128.dsword[i];
        }
      }
    }
    break;
  case INT64_TYPE:
    if (saturate) {
      for (s32 i = 0; i < 2; i++) {
        if (isUnsigned) {
          //constant.v128.qword[i] = Base::MathUtils::SaturateAdd(constant.v128.qword[i], other->constant.v128.qword[i]);
        } else {
          //constant.v128.qsword[i] = Base::MathUtils::SaturateAdd(constant.v128.qsword[i], other->constant.v128.qsword[i]);
        }
      }
      // TODO(Triang3l): Trace DID_SATURATE.
    } else {
      if (isUnsigned) {
        constant.v128.qword[0] += other->constant.v128.qword[0];
        constant.v128.qword[1] += other->constant.v128.qword[1];
      }
      else {
        constant.v128.qsword[0] += other->constant.v128.qsword[0];
        constant.v128.qsword[1] += other->constant.v128.qsword[1];
      }
    }
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::VectorSub(Value *other, TypeName type, bool isUnsigned,
  bool saturate) {
  ASSERT(this->type == VEC128_TYPE && other->type == VEC128_TYPE);
  switch (type) {
  case FLOAT32_TYPE:
    if (saturate) {
      UNREACHABLE();
    } else {
      constant.v128.x -= other->constant.v128.x;
      constant.v128.y -= other->constant.v128.y;
      constant.v128.z -= other->constant.v128.z;
      constant.v128.w -= other->constant.v128.w;
    }
    break;
  case FLOAT64_TYPE:
    if (saturate) {
      UNREACHABLE();
    } else {
      constant.v128.dbl[0] -= other->constant.v128.dbl[0];
      constant.v128.dbl[1] -= other->constant.v128.dbl[1];
    }
    break;
  case INT8_TYPE:
    if (saturate) {
      for (s32 i = 0; i < 16; i++) {
        if (isUnsigned) {
          //constant.v128.bytes[i] = Base::MathUtils::SaturateSub(constant.v128.bytes[i], other->constant.v128.bytes[i]);
        } else {
          //constant.v128.sbytes[i] = Base::MathUtils::SaturateSub(constant.v128.sbytes[i], other->constant.v128.sbytes[i]);
        }
      }
      // TODO(Triang3l): Trace DID_SATURATE.
    } else {
      for (s32 i = 0; i < 16; i++) {
        if (isUnsigned) {
          constant.v128.bytes[i] -= other->constant.v128.bytes[i];
        } else {
          constant.v128.sbytes[i] -= other->constant.v128.sbytes[i];
        }
      }
    }
    break;
  case INT16_TYPE:
    if (saturate) {
      for (s32 i = 0; i < 8; i++) {
        if (isUnsigned) {
          //constant.v128.word[i] = Base::MathUtils::SaturateSub(constant.v128.word[i], other->constant.v128.word[i]);
        } else {
          //constant.v128.sword[i] = Base::MathUtils::SaturateSub(constant.v128.sword[i], other->constant.v128.sword[i]);
        }
      }
      // TODO(Triang3l): Trace DID_SATURATE.
    } else {
      for (s32 i = 0; i < 8; i++) {
        if (isUnsigned) {
          constant.v128.word[i] -= other->constant.v128.word[i];
        } else {
          constant.v128.sword[i] -= other->constant.v128.sword[i];
        }
      }
    }
    break;
  case INT32_TYPE:
    if (saturate) {
      for (s32 i = 0; i < 4; i++) {
        if (isUnsigned) {
          //constant.v128.dword[i] = Base::MathUtils::SaturateSub(constant.v128.dword[i], other->constant.v128.dword[i]);
        } else {
          //constant.v128.dsword[i] = Base::MathUtils::SaturateSub(constant.v128.dsword[i], other->constant.v128.dsword[i]);
        }
      }
      // TODO(Triang3l): Trace DID_SATURATE.
    } else {
      for (s32 i = 0; i < 4; i++) {
        if (isUnsigned) {
          constant.v128.dword[i] -= other->constant.v128.dword[i];
        } else {
          constant.v128.dsword[i] -= other->constant.v128.dsword[i];
        }
      }
    }
    break;
  case INT64_TYPE:
    if (saturate) {
      for (s32 i = 0; i < 2; i++) {
        if (isUnsigned) {
          //constant.v128.qword[i] = Base::MathUtils::SaturateSub(constant.v128.qword[i], other->constant.v128.qword[i]);
        } else {
          //constant.v128.qsword[i] = Base::MathUtils::SaturateSub(constant.v128.qsword[i], other->constant.v128.qsword[i]);
        }
      }
      // TODO(Triang3l): Trace DID_SATURATE.
    } else {
      if (isUnsigned) {
        constant.v128.qword[0] -= other->constant.v128.qword[0];
        constant.v128.qword[1] -= other->constant.v128.qword[1];
      } else {
        constant.v128.qsword[0] -= other->constant.v128.qsword[0];
        constant.v128.qsword[1] -= other->constant.v128.qsword[1];
      }
    }
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::DotProduct3(Value *other) {
  ASSERT(this->type == VEC128_TYPE && other->type == VEC128_TYPE);
  switch (type) {
  case VEC128_TYPE: {
    // TODO(rick): is this sane?
    type = FLOAT32_TYPE;
    // Using x86 DPPS ordering for consistency with x86-64 code generation:
    // (X1 * X2 + Y1 * Y2) + (Z1 * Z2 + 0.0f)
    // (+ 0.0f for zero sign, as zero imm8[4:7] bits result in zero terms,
    // not in complete exclusion of them)
    // TODO(Triang3l): NaN on overflow.
    constant.f32 =
      (constant.v128.flt[0] * other->constant.v128.flt[0] +
        constant.v128.flt[1] * other->constant.v128.flt[1]) +
      (constant.v128.flt[2] * other->constant.v128.flt[2] + 0.0f);
  } break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::DotProduct4(Value *other) {
  ASSERT(this->type == VEC128_TYPE && other->type == VEC128_TYPE);
  switch (type) {
  case VEC128_TYPE: {
    // TODO(rick): is this sane?
    type = FLOAT32_TYPE;
    // Using x86 DPPS ordering for consistency with x86-64 code generation:
    // (X1 * X2 + Y1 * Y2) + (Z1 * Z2 + W1 * W2)
    // TODO(Triang3l): NaN on overflow.
    constant.f32 = (constant.v128.flt[0] * other->constant.v128.flt[0] +
      constant.v128.flt[1] * other->constant.v128.flt[1]) +
      (constant.v128.flt[2] * other->constant.v128.flt[2] +
        constant.v128.flt[3] * other->constant.v128.flt[3]);
  } break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::VectorAverage(Value *other, TypeName type, bool isUnsigned,
  bool saturate) {
  ASSERT(this->type == VEC128_TYPE && other->type == VEC128_TYPE);
  switch (type) {
  case INT8_TYPE: {
    for (s32 i = 0; i < 16; i++) {
      if (isUnsigned) {
        constant.v128.bytes[i] =
          u8((u16(constant.v128.bytes[i]) +
            u16(other->constant.v128.bytes[i]) + 1) >>
            1);
      }
      else {
        constant.v128.sbytes[i] =
          s8((s16(constant.v128.sbytes[i]) +
            s16(other->constant.v128.sbytes[i]) + 1) >>
            1);
      }
    }
  } break;
  case INT16_TYPE: {
    for (s32 i = 0; i < 8; i++) {
      if (isUnsigned) {
        constant.v128.word[i] =
          u16((u32(constant.v128.word[i]) +
            u32(other->constant.v128.word[i]) + 1) >>
            1);
      }
      else {
        constant.v128.sword[i] =
          s16((s32(constant.v128.sword[i]) +
            s32(other->constant.v128.sword[i]) + 1) >>
            1);
      }
    }
  } break;
  case INT32_TYPE: {
    for (s32 i = 0; i < 4; i++) {
      if (isUnsigned) {
        constant.v128.dword[i] =
          u32((u64(constant.v128.dword[i]) +
            u64(other->constant.v128.dword[i]) + 1) >>
            1);
      }
      else {
        constant.v128.dsword[i] =
          s32((s64(constant.v128.dsword[i]) +
            s64(other->constant.v128.dsword[i]) + 1) >>
            1);
      }
    }
  } break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::ByteSwap() {
  switch (type) {
  case INT8_TYPE:
    constant.i8 = constant.i8;
    break;
  case INT16_TYPE:
    constant.i16 = byteswap_be(constant.i16);
    break;
  case INT32_TYPE:
    constant.i32 = byteswap_be(constant.i32);
    break;
  case INT64_TYPE:
    constant.i64 = byteswap_be(constant.i64);
    break;
  case VEC128_TYPE:
    for (s32 n = 0; n < 4; n++) {
      constant.v128.dword[n] = byteswap_be(constant.v128.dword[n]);
    }
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void Value::CountLeadingZeros(const Value *other) {
  switch (other->type) {
  case INT8_TYPE:
    constant.i8 = std::countl_zero(static_cast<u8>(other->constant.i8));
    break;
  case INT16_TYPE:
    constant.i8 = std::countl_zero(static_cast<u16>(other->constant.i16));
    break;
  case INT32_TYPE:
    constant.i8 = std::countl_zero(static_cast<u32>(other->constant.i32));
    break;
  case INT64_TYPE:
    constant.i8 = std::countl_zero(static_cast<u64>(other->constant.i64));
    break;
  default:
    UNREACHABLE();
    break;
  }
}

bool Value::Compare(Opcode opcode, Value *other) {
  ASSERT(type == other->type);
  switch (other->type) {
  case INT8_TYPE:
    return CompareInt8(opcode, this, other);
  case INT16_TYPE:
    return CompareInt16(opcode, this, other);
  case INT32_TYPE:
    return CompareInt32(opcode, this, other);
  case INT64_TYPE:
    return CompareInt64(opcode, this, other);
  default:
    UNREACHABLE();
    return false;
  }
}

bool Value::CompareInt8(Opcode opcode, Value *a, Value *b) {
  switch (opcode) {
  case OPCODE_COMPARE_EQ:
    return a->constant.i8 == b->constant.i8;
  case OPCODE_COMPARE_NE:
    return a->constant.i8 != b->constant.i8;
  case OPCODE_COMPARE_SLT:
    return a->constant.i8 < b->constant.i8;
  case OPCODE_COMPARE_SLE:
    return a->constant.i8 <= b->constant.i8;
  case OPCODE_COMPARE_SGT:
    return a->constant.i8 > b->constant.i8;
  case OPCODE_COMPARE_SGE:
    return a->constant.i8 >= b->constant.i8;
  case OPCODE_COMPARE_ULT:
    return u8(a->constant.i8) < u8(b->constant.i8);
  case OPCODE_COMPARE_ULE:
    return u8(a->constant.i8) <= u8(b->constant.i8);
  case OPCODE_COMPARE_UGT:
    return u8(a->constant.i8) > u8(b->constant.i8);
  case OPCODE_COMPARE_UGE:
    return u8(a->constant.i8) >= u8(b->constant.i8);
  default:
    UNREACHABLE();
    return false;
  }
}

bool Value::CompareInt16(Opcode opcode, Value *a, Value *b) {
  switch (opcode) {
  case OPCODE_COMPARE_EQ:
    return a->constant.i16 == b->constant.i16;
  case OPCODE_COMPARE_NE:
    return a->constant.i16 != b->constant.i16;
  case OPCODE_COMPARE_SLT:
    return a->constant.i16 < b->constant.i16;
  case OPCODE_COMPARE_SLE:
    return a->constant.i16 <= b->constant.i16;
  case OPCODE_COMPARE_SGT:
    return a->constant.i16 > b->constant.i16;
  case OPCODE_COMPARE_SGE:
    return a->constant.i16 >= b->constant.i16;
  case OPCODE_COMPARE_ULT:
    return u16(a->constant.i16) < u16(b->constant.i16);
  case OPCODE_COMPARE_ULE:
    return u16(a->constant.i16) <= u16(b->constant.i16);
  case OPCODE_COMPARE_UGT:
    return u16(a->constant.i16) > u16(b->constant.i16);
  case OPCODE_COMPARE_UGE:
    return u16(a->constant.i16) >= u16(b->constant.i16);
  default:
    UNREACHABLE();
    return false;
  }
}

bool Value::CompareInt32(Opcode opcode, Value *a, Value *b) {
  switch (opcode) {
  case OPCODE_COMPARE_EQ:
    return a->constant.i32 == b->constant.i32;
  case OPCODE_COMPARE_NE:
    return a->constant.i32 != b->constant.i32;
  case OPCODE_COMPARE_SLT:
    return a->constant.i32 < b->constant.i32;
  case OPCODE_COMPARE_SLE:
    return a->constant.i32 <= b->constant.i32;
  case OPCODE_COMPARE_SGT:
    return a->constant.i32 > b->constant.i32;
  case OPCODE_COMPARE_SGE:
    return a->constant.i32 >= b->constant.i32;
  case OPCODE_COMPARE_ULT:
    return u32(a->constant.i32) < u32(b->constant.i32);
  case OPCODE_COMPARE_ULE:
    return u32(a->constant.i32) <= u32(b->constant.i32);
  case OPCODE_COMPARE_UGT:
    return u32(a->constant.i32) > u32(b->constant.i32);
  case OPCODE_COMPARE_UGE:
    return u32(a->constant.i32) >= u32(b->constant.i32);
  default:
    UNREACHABLE();
    return false;
  }
}

bool Value::CompareInt64(Opcode opcode, Value *a, Value *b) {
  switch (opcode) {
  case OPCODE_COMPARE_EQ:
    return a->constant.i64 == b->constant.i64;
  case OPCODE_COMPARE_NE:
    return a->constant.i64 != b->constant.i64;
  case OPCODE_COMPARE_SLT:
    return a->constant.i64 < b->constant.i64;
  case OPCODE_COMPARE_SLE:
    return a->constant.i64 <= b->constant.i64;
  case OPCODE_COMPARE_SGT:
    return a->constant.i64 > b->constant.i64;
  case OPCODE_COMPARE_SGE:
    return a->constant.i64 >= b->constant.i64;
  case OPCODE_COMPARE_ULT:
    return u64(a->constant.i64) < u64(b->constant.i64);
  case OPCODE_COMPARE_ULE:
    return u64(a->constant.i64) <= u64(b->constant.i64);
  case OPCODE_COMPARE_UGT:
    return u64(a->constant.i64) > u64(b->constant.i64);
  case OPCODE_COMPARE_UGE:
    return u64(a->constant.i64) >= u64(b->constant.i64);
  default:
    UNREACHABLE();
    return false;
  }
}

}  // namespace Xe::XCPU::HIR