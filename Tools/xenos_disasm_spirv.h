// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "xenos_disasm_mappings.h"

#include <sirit/sirit.h>
#include <spirv/unified1/GLSL.std.450.h>

struct XenosSpirvCompiler {
  explicit XenosSpirvCompiler(Xe::eShaderType type) :
    shaderType(type),
    isPixel(type == Xe::eShaderType::Pixel)
  {}

  void InitModule() {
    mod.AddCapability(spv::Capability::Shader);
    mod.SetMemoryModel(spv::AddressingModel::Logical, spv::MemoryModel::GLSL450);

    intType = mod.TypeInt(32, true);
    uintType = mod.TypeInt(32, false);
    floatType = mod.TypeFloat(32);
    vec2Type = mod.TypeVector(floatType, 2);
    vec4Type = mod.TypeVector(floatType, 4);
    vec4PtrInput = mod.TypePointer(spv::StorageClass::Input,  vec4Type);
    vec4PtrOutput = mod.TypePointer(spv::StorageClass::Output, vec4Type);
    vec4PtrFunction = mod.TypePointer(spv::StorageClass::Function, vec4Type);
    floatPtrFunction = mod.TypePointer(spv::StorageClass::Function, floatType);

    // UBO for c# constants: layout(set=0,binding=0) buffer ALUConsts { vec4 FloatConsts[512]; };
    Sirit::Id const512 = mod.Constant(uintType, 512);
    Sirit::Id vec4Array512 = mod.TypeArray(vec4Type, const512);
    uboStructType = mod.TypeStruct(vec4Array512);
    mod.Decorate(uboStructType, spv::Decoration::Block);
    mod.Name(uboStructType, "ALUConsts");
    mod.MemberName(uboStructType, 0, "FloatConsts");
    mod.Decorate(vec4Array512, spv::Decoration::ArrayStride, 16);
    mod.MemberDecorate(uboStructType, 0, spv::Decoration::Offset, 0);

    uboPtrType = mod.TypePointer(spv::StorageClass::Uniform, uboStructType);
    uboVar = mod.AddGlobalVariable(uboPtrType, spv::StorageClass::Uniform);
    mod.Decorate(uboVar, spv::Decoration::DescriptorSet, 0);
    mod.Decorate(uboVar, spv::Decoration::Binding, 0);
    mod.Name(uboVar, "ALUConstsBuffer");
    interfaceVars.push_back(uboVar);

    // Sampler2D type (we’ll assume 2D for TEX_FETCH for now)
    Sirit::Id image2D = mod.TypeImage(
      floatType,
      spv::Dim::Dim2D,
      /*depth=*/0,
      /*arrayed=*/0,
      /*ms=*/0,
      /*sampled=*/1,
      spv::ImageFormat::Unknown);
    sampledImage2DType = mod.TypeSampledImage(image2D);
  }

  void BeginMain() {
    voidType = mod.TypeVoid();
    Sirit::Id funcType = mod.TypeFunction(voidType);
    mainFunc = mod.OpFunction(voidType, spv::FunctionControlMask::MaskNone, funcType);
    mainLabel = mod.OpLabel();
    mod.AddLabel(mainLabel);
  }

  void EndMain() {
    mod.OpReturn();
    mod.OpFunctionEnd();

    spv::ExecutionModel model =
      isPixel ? spv::ExecutionModel::Fragment : spv::ExecutionModel::Vertex;

    if (isPixel) {
      mod.AddExecutionMode(mainFunc, spv::ExecutionMode::OriginUpperLeft);
    }

    mod.AddEntryPoint(model, mainFunc, "main", interfaceVars);
  }

  void AllocateAllRegisters() {
    // Create variables first (pass 1)
    for (auto& [reg, id] : regVars) {
      Sirit::Id var = mod.AddLocalVariable(vec4PtrFunction, spv::StorageClass::Function);
      mod.Name(var, ("r" + std::to_string(reg)).c_str());
      id = var;
    }

    AllocateScalars();

    // Initialize all registers after creation (pass 2)
    Sirit::Id zero = mod.Constant(floatType, 0.f);
    Sirit::Id zeroVec = mod.ConstantComposite(vec4Type, zero, zero, zero, zero);

    for (auto& [reg, var] : regVars) {
      mod.OpStore(var, zeroVec);
    }

    InitScalars();
  }

  void TouchRegister(u32 reg) {
    regVars.try_emplace(reg, Sirit::Id{});
  }

  void FinalizeRegisters() {
    registersFinalized = true;
  }

  Sirit::Id GetRegPtr(u32 index) {
    auto it = regVars.find(index);
    if (it == regVars.end()) {
      LOG_ERROR(Core, "Register r{} used but was never scanned!", index);
      ::abort();
    }

    if (!it->second.value) {  // not yet allocated
      LOG_ERROR(Core, "Register r{} accessed before AllocateAllRegisters()", index);
      ::abort();
    }

    return it->second;
  }

  void TouchScalar(u32 reg) {
    scalarRegs.try_emplace(reg, Sirit::Id{});
  }

  void AllocateScalars() {
    for (auto& [r, id] : scalarRegs) {
      Sirit::Id var = mod.AddLocalVariable(floatPtrFunction, spv::StorageClass::Function);
      mod.Name(var, ("s" + std::to_string(r)).c_str());
      id = var;
    }
  }

  void InitScalars() {
    Sirit::Id zero = mod.Constant(floatType, 0.f);
    for (auto& [r, var] : scalarRegs) {
      mod.OpStore(var, zero);
    }
  }

  Sirit::Id LoadScalar(u32 r) {
    return mod.OpLoad(floatType, scalarRegs.at(r));
  }

  void StoreScalar(u32 r, Sirit::Id v) {
    mod.OpStore(scalarRegs.at(r), v);
  }

  Sirit::Id LoadReg(u32 index) {
    Sirit::Id ptr = GetRegPtr(index);
    return mod.OpLoad(vec4Type, ptr);
  }

  void StoreReg(u32 index, Sirit::Id val) {
    Sirit::Id ptr = GetRegPtr(index);
    mod.OpStore(ptr, val);
  }

  Sirit::Id LoadConstVec4(u32 cIndex) {
    Sirit::Id zero = mod.Constant(uintType, 0);
    Sirit::Id idx  = mod.Constant(uintType, cIndex);
    Sirit::Id ptr  = mod.OpAccessChain(
      mod.TypePointer(spv::StorageClass::Uniform, vec4Type),
      uboVar,
      zero,
      idx);
    Sirit::Id val  = mod.OpLoad(vec4Type, ptr);
    mod.Name(val, ("c" + std::to_string(cIndex)).c_str());
    return val;
  }

  std::array<u32, 4> DecodeSwizzleIndices(u32 swz) {
    std::array<u32, 4> out{};
    for (u32 i = 0; i < 4; ++i) {
      out[i] = (swz >> (i * 2)) & 0x3;
    }
    return out;
  }

  Sirit::Id ApplySwizzle(Sirit::Id vec, u32 swz) {
    auto idx = DecodeSwizzleIndices(swz);
    return mod.OpVectorShuffle(vec4Type, vec, vec,
      idx[0], idx[1], idx[2], idx[3]);
  }

  Sirit::Id DecodeVtxFormat(Sirit::Id rawVec, Xe::instr_surf_fmt_t fmt) {
    switch (fmt) {
      // This may need to be emulated properly.
      // It hackly treats it as a boolean,
      // because using a proper bitfield is costly.
      case Xe::FMT_1_REVERSE: {
        // Treat LSB as boolean, expand to float vector
        Sirit::Id x = mod.OpCompositeExtract(floatType, rawVec, 0);
        Sirit::Id zero = mod.Constant(floatType, 0.f);
        Sirit::Id one  = mod.Constant(floatType, 1.f);

        // x != 0 ? 1.0 : 0.0
        Sirit::Id cond = mod.OpFOrdNotEqual(mod.TypeBool(), x, zero);
        Sirit::Id fx   = mod.OpSelect(floatType, cond, one, zero);

        return mod.OpCompositeConstruct(vec4Type, fx, fx, fx, fx);
      }

      case Xe::FMT_32_FLOAT:
      case Xe::FMT_32: {
        Sirit::Id zero = mod.Constant(floatType, 0.f);
        Sirit::Id one = mod.Constant(floatType, 1.f);
        // Single float in .x
        Sirit::Id x = mod.OpCompositeExtract(floatType, rawVec, 0);
        return mod.OpCompositeConstruct(vec4Type, x, zero, zero, one);
      }

      case Xe::FMT_32_32_FLOAT:
      case Xe::FMT_32_32: {
        Sirit::Id zero = mod.Constant(floatType, 0.f);
        Sirit::Id one = mod.Constant(floatType, 1.f);
        // Two floats in .xy
        Sirit::Id x = mod.OpCompositeExtract(floatType, rawVec, 0);
        Sirit::Id y = mod.OpCompositeExtract(floatType, rawVec, 1);
        // Z=0, W=1 is a decent default for positions/texcoords
        return mod.OpCompositeConstruct(vec4Type, x, y, zero, one);
      }

      case Xe::FMT_32_32_32_32_FLOAT:
        return rawVec;

      case Xe::FMT_16_FLOAT: {
        // Packed half in X
        Sirit::Id packed = mod.OpBitcast(uintType, rawVec);
        Sirit::Id half2 = mod.OpUnpackHalf2x16(vec2Type, packed);
        Sirit::Id x = mod.OpCompositeExtract(floatType, half2, 0);
        return mod.OpCompositeConstruct(vec4Type, x, x, x, x);
      }

      case Xe::FMT_16_16_FLOAT: {
        Sirit::Id packed = mod.OpBitcast(uintType, rawVec);
        Sirit::Id half = mod.OpUnpackHalf2x16(vec2Type, packed);
        Sirit::Id x = mod.OpCompositeExtract(floatType, half, 0);
        Sirit::Id y = mod.OpCompositeExtract(floatType, half, 1);
        Sirit::Id zero = mod.Constant(floatType, 0.f);
        return mod.OpCompositeConstruct(vec4Type, x, y, zero, zero);
      }

      case Xe::FMT_16_16_16_16_FLOAT:
        // Already expanded by input layout
        return rawVec;

      case Xe::FMT_8: // single channel, UNORM8
      case Xe::FMT_8_A: { // treat as alpha-like single channel
        Sirit::Id zero = mod.Constant(floatType, 0.f);
        Sirit::Id one = mod.Constant(floatType, 1.f);

        Sirit::Id xFloat = mod.OpCompositeExtract(floatType, rawVec, 0);
        Sirit::Id packed = mod.OpBitcast(uintType, xFloat);

        Sirit::Id mask8 = mod.Constant(uintType, 0xFFu);
        Sirit::Id xi = mod.OpBitwiseAnd(uintType, packed, mask8);

        Sirit::Id xf = mod.OpConvertUToF(floatType, xi);
        Sirit::Id inv255 = mod.Constant(floatType, 1.0f / 255.0f);
        xf = mod.OpFMul(floatType, xf, inv255);

        // Put it in .x, leave the rest defaulted
        return mod.OpCompositeConstruct(vec4Type, xf, zero, zero, one);
      }

      case Xe::FMT_8_8: {
        // rawVec.x contains the packed 16-bit value in the low 16 bits
        Sirit::Id packed = mod.OpBitcast(uintType,
          mod.OpCompositeExtract(floatType, rawVec, 0));

        Sirit::Id mask8 = mod.Constant(uintType, 0xFF);

        // Extract bytes
        Sirit::Id xi = mod.OpBitwiseAnd(uintType, packed, mask8);
        Sirit::Id yi = mod.OpBitwiseAnd(uintType,
          mod.OpShiftRightLogical(uintType, packed, mod.Constant(uintType, 8)),
          mask8
        );

        // Convert to float
        Sirit::Id xf = mod.OpConvertUToF(floatType, xi);
        Sirit::Id yf = mod.OpConvertUToF(floatType, yi);

        // Normalize
        Sirit::Id inv255 = mod.Constant(floatType, 1.0f / 255.0f);
        xf = mod.OpFMul(floatType, xf, inv255);
        yf = mod.OpFMul(floatType, yf, inv255);

        Sirit::Id zero = mod.Constant(floatType, 0.0f);
        Sirit::Id one  = mod.Constant(floatType, 1.0f);

        return mod.OpCompositeConstruct(vec4Type, xf, yf, zero, one);
      }

      case Xe::FMT_8_8_8_8:
      case Xe::FMT_8_8_8_8_A: {
        Sirit::Id packed = mod.OpBitcast(uintType, rawVec);
        return mod.OpUnpackUnorm4x8(vec4Type, packed);
      }

      case Xe::FMT_16: {
        // 16-bit UNORM in X
        Sirit::Id packed = mod.OpBitcast(uintType, rawVec);
        Sirit::Id f = mod.OpUConvert(floatType, packed);
        return mod.OpCompositeConstruct(vec4Type, f, f, f, f);
      }

      case Xe::FMT_16_16: {
        Sirit::Id packed = mod.OpBitcast(uintType, rawVec);
        Sirit::Id xy = mod.OpUnpackUnorm2x16(vec2Type, packed);
        Sirit::Id x = mod.OpCompositeExtract(floatType, xy, 0);
        Sirit::Id y = mod.OpCompositeExtract(floatType, xy, 1);
        Sirit::Id zero = mod.Constant(floatType, 0.f);
        return mod.OpCompositeConstruct(vec4Type, x, y, zero, zero);
      }

      case Xe::FMT_16_16_16_16:
        // Already expanded to vec4
        return rawVec;

      case Xe::FMT_2_10_10_10: {
        // rawVec.x contains the packed 32-bit value
        Sirit::Id packed = mod.OpBitcast(uintType,
          mod.OpCompositeExtract(floatType, rawVec, 0));

        // Bit masks
        Sirit::Id mask10 = mod.Constant(uintType, 0x3FF); // 10 bits
        Sirit::Id mask2  = mod.Constant(uintType, 0x3); // 2 bits

        // Bit shifts
        Sirit::Id shiftX = mod.Constant(uintType, 0);
        Sirit::Id shiftY = mod.Constant(uintType, 10);
        Sirit::Id shiftZ = mod.Constant(uintType, 20);
        Sirit::Id shiftW = mod.Constant(uintType, 30);

        // Extract components
        Sirit::Id xi = mod.OpBitwiseAnd(uintType,
          mod.OpShiftRightLogical(uintType, packed, shiftX), mask10);

        Sirit::Id yi = mod.OpBitwiseAnd(uintType,
          mod.OpShiftRightLogical(uintType, packed, shiftY), mask10);

        Sirit::Id zi = mod.OpBitwiseAnd(uintType,
          mod.OpShiftRightLogical(uintType, packed, shiftZ), mask10);

        Sirit::Id wi = mod.OpBitwiseAnd(uintType,
          mod.OpShiftRightLogical(uintType, packed, shiftW), mask2);

        // Convert to float
        Sirit::Id xf = mod.OpConvertUToF(floatType, xi);
        Sirit::Id yf = mod.OpConvertUToF(floatType, yi);
        Sirit::Id zf = mod.OpConvertUToF(floatType, zi);
        Sirit::Id wf = mod.OpConvertUToF(floatType, wi);

        // Normalize
        Sirit::Id inv1023 = mod.Constant(floatType, 1.f / 1023.f);
        Sirit::Id inv3 = mod.Constant(floatType, 1.f / 3.f);

        xf = mod.OpFMul(floatType, xf, inv1023);
        yf = mod.OpFMul(floatType, yf, inv1023);
        zf = mod.OpFMul(floatType, zf, inv1023);
        wf = mod.OpFMul(floatType, wf, inv3);

        return mod.OpCompositeConstruct(vec4Type, xf, yf, zf, wf);
      }

      case Xe::FMT_16_MPEG:
      case Xe::FMT_16_16_MPEG:
      case Xe::FMT_16_MPEG_INTERLACED:
      case Xe::FMT_16_16_MPEG_INTERLACED:
      case Xe::FMT_DXT1:
      case Xe::FMT_DXT2_3:
      case Xe::FMT_DXT4_5:
      case Xe::FMT_DXT3A:
      case Xe::FMT_DXT5A:
      case Xe::FMT_CTX1:
        // These should never appear in VTX_FETCH for attributes
        return rawVec;

      default:
        LOG_ERROR(Core, "Unhandled VTX format {}", (u32)fmt);
        return rawVec;
    }
  }

  Sirit::Id MaybeNegate(Sirit::Id val, bool neg) {
    if (!neg)
        return val;
    return mod.OpFNegate(vec4Type, val);
  }

  Sirit::Id GetVertexInputVar(u32 slot) {
    auto it = vertexInputs.find(slot);
    if (it != vertexInputs.end())
      return it->second;

    Sirit::Id var = mod.AddGlobalVariable(vec4PtrInput, spv::StorageClass::Input);
    mod.Decorate(var, spv::Decoration::Location, nextInputLocation++);
    mod.Name(var, ("v" + std::to_string(slot)).c_str());
    vertexInputs[slot] = var;
    interfaceVars.push_back(var);
    return var;
  }

  Sirit::Id LoadVertexInput(u32 slot) {
    return mod.OpLoad(vec4Type, GetVertexInputVar(slot));
  }

  Sirit::Id GetOutputVarPS(u32 reg) {
    auto it = outputs.find(reg);
    if (it != outputs.end())
      return it->second;

    Sirit::Id var = mod.AddGlobalVariable(vec4PtrOutput, spv::StorageClass::Output);

    u32 location = (reg == 0) ? 0 : vsExportLocationMap.at(reg); // match VS
    mod.Decorate(var, spv::Decoration::Location, location);
    mod.Name(var, ("COLOR" + std::to_string(location)).c_str());

    outputs[reg] = var;
    interfaceVars.push_back(var);
    return var;
  }

  Sirit::Id GetOutputVarVS(u32 reg) {
    auto it = outputs.find(reg);
    if (it != outputs.end())
      return it->second;

    Sirit::Id var{};
    if (reg == 0) {
        // POSITION is built-in
      var = mod.AddGlobalVariable(vec4PtrOutput, spv::StorageClass::Output);
      mod.Decorate(var, spv::Decoration::BuiltIn, static_cast<u32>(spv::BuiltIn::Position));
      mod.Name(var, "POSITION");
    } else {
      var = mod.AddGlobalVariable(vec4PtrOutput, spv::StorageClass::Output);
      u32 location = nextOutputLocation++;
      mod.Decorate(var, spv::Decoration::Location, location);
      vsExportLocationMap[reg] = location; // track for PS
      mod.Name(var, ("OUT" + std::to_string(location)).c_str());
    }

    outputs[reg] = var;
    interfaceVars.push_back(var);
    return var;
  }

  void ExportFromReg(u32 reg, Sirit::Id value) {
    if (isPixel) {
      Sirit::Id out = GetOutputVarPS(reg);
      mod.OpStore(out, value);
    } else {
      Sirit::Id out = GetOutputVarVS(reg);
      mod.OpStore(out, value);
    }
  }

  Sirit::Id GetTextureVar(u32 slot) {
    auto it = textures.find(slot);
    if (it != textures.end())
      return it->second;

    Sirit::Id ptrType = mod.TypePointer(spv::StorageClass::UniformConstant, sampledImage2DType);
    Sirit::Id var = mod.AddGlobalVariable(ptrType, spv::StorageClass::UniformConstant);
    mod.Decorate(var, spv::Decoration::DescriptorSet, 0);
    mod.Decorate(var, spv::Decoration::Binding, slot);
    mod.Name(var, ("TextureSlot" + std::to_string(slot)).c_str());
    textures[slot] = var;
    // Add to entry point interface
    interfaceVars.push_back(var);
    return var;
  }

  Sirit::Id EmitTextureSample2D(Sirit::Id coordVec4, u32 slot, const Xe::instr_fetch_tex_t &t) {
    Sirit::Id texVar = GetTextureVar(slot);
    Sirit::Id sampled = mod.OpLoad(sampledImage2DType, texVar);

    Sirit::Id coord = mod.OpVectorShuffle(vec2Type, coordVec4, coordVec4, 0, 1);

    if (t.offset_x || t.offset_y) {
      Sirit::Id offX = mod.Constant(intType, (s32)t.offset_x);
      Sirit::Id offY = mod.Constant(intType, (s32)t.offset_y);
      coord = mod.OpCompositeConstruct(
        vec2Type,
        mod.OpFAdd(floatType, mod.OpCompositeExtract(floatType, coord, 0),
                  mod.OpConvertSToF(floatType, offX)),
        mod.OpFAdd(floatType, mod.OpCompositeExtract(floatType, coord, 1),
                  mod.OpConvertSToF(floatType, offY))
      );
    }

    if (!isPixel) {
      // Vertex shader MUST use explicit LOD
      Sirit::Id lod = mod.Constant(floatType, 0.f);
      return mod.OpImageSampleExplicitLod(
        vec4Type,
        sampled,
        coord,
        spv::ImageOperandsMask::Lod,
        lod
      );
    }

    // Fragment shader can use implicit LOD
    return mod.OpImageSampleImplicitLod(vec4Type, sampled, coord);
  }

  Sirit::Id EmitVectorOp(Xe::instr_vector_opc_t vop, Sirit::Id a, Sirit::Id b, Sirit::Id c) {
    switch (vop) {
    case Xe::ADDv:
      return mod.OpFAdd(vec4Type, a, b);
    case Xe::MULv:
      return mod.OpFMul(vec4Type, a, b);
    case Xe::MAXv:
      return mod.OpFMax(vec4Type, a, b);
    case Xe::MINv:
      return mod.OpFMin(vec4Type, a, b);
    case Xe::MULADDv: {
      Sirit::Id mul = mod.OpFMul(vec4Type, a, b);
      return mod.OpFAdd(vec4Type, mul, c);
    }
    case Xe::DOT4v: {
      Sirit::Id dot = mod.OpDot(floatType, a, b);
      return mod.OpCompositeConstruct(vec4Type, dot, dot, dot, dot);
    }
    default:
      LOG_ERROR(Core, "Unimplemented vector opcode {}", static_cast<u32>(vop));
      // Return zero to avoid crashing
      {
        Sirit::Id z = mod.Constant(floatType, 0.f);
        return mod.ConstantComposite(vec4Type, z, z, z, z);
      }
    }
  }

  Sirit::Id EmitScalarOp(Xe::instr_scalar_opc_t sop, Sirit::Id a, Sirit::Id b) {
    switch (sop) {
      case Xe::ADDs: return mod.OpFAdd(floatType, a, b);
      case Xe::MULs: return mod.OpFMul(floatType, a, b);
      case Xe::MINs: return mod.OpFMin(floatType, a, b);
      case Xe::MAXs: return mod.OpFMax(floatType, a, b);
      case Xe::SUBs: return mod.OpFSub(floatType, a, b);

      case Xe::SETGTs:
        return mod.OpSelect(
          floatType,
          mod.OpFOrdGreaterThan(mod.TypeBool(), a, b),
          mod.Constant(floatType, 1.f),
          mod.Constant(floatType, 0.f));

      case Xe::SETGTEs:
        return mod.OpSelect(
          floatType,
          mod.OpFOrdGreaterThanEqual(mod.TypeBool(), a, b),
          mod.Constant(floatType, 1.f),
          mod.Constant(floatType, 0.f));

      case Xe::SETNEs:
        return mod.OpSelect(
          floatType,
          mod.OpFOrdNotEqual(mod.TypeBool(), a, b),
          mod.Constant(floatType, 1.f),
          mod.Constant(floatType, 0.f));

      case Xe::RETAIN_PREV:
        return a; // Return src1 unchanged

      default:
        LOG_ERROR(Core, "Unimplemented SOP {}", (u32)sop);
        return mod.Constant(floatType, 0.f);
    }
  }

  std::array<spv::Id, 4> DecodeMaskToIndices(u32 mask) {
    std::array<spv::Id, 4> out{};
    u32 idx = 0;
    for (u32 i = 0; i < 4 && idx < 4; ++i) {
      if (mask & (1u << i)) {
        out[idx++] = i;
      }
    }
    // If mask is empty, default to xyzw
    if (idx == 0) {
      out[0] = 0; out[1] = 1; out[2] = 2; out[3] = 3;
    }
    return out;
  }

  void EmitALU(const Xe::instr_alu_t &alu) {
    // Build 3 vector sources
    auto sop = static_cast<Xe::instr_scalar_opc_t>(alu.scalar_opc);
    auto vop = static_cast<Xe::instr_vector_opc_t>(alu.vector_opc);

    // src1
    Sirit::Id s1 =
      alu.src1_sel ? LoadConstVec4(alu.src1_reg)
                   : LoadReg(alu.src1_reg);
    s1 = ApplySwizzle(s1, alu.src1_swiz);
    s1 = MaybeNegate(s1, alu.src1_reg_negate != 0);

    // src2
    Sirit::Id s2 =
      alu.src2_sel ? LoadConstVec4(alu.src2_reg)
                   : LoadReg(alu.src2_reg);
    s2 = ApplySwizzle(s2, alu.src2_swiz);
    s2 = MaybeNegate(s2, alu.src2_reg_negate != 0);

    // src3
    Sirit::Id s3 =
      alu.src3_sel ? LoadConstVec4(alu.src3_reg)
                   : LoadReg(alu.src3_reg);
    s3 = ApplySwizzle(s3, alu.src3_swiz);
    s3 = MaybeNegate(s3, alu.src3_reg_negate != 0);

    Sirit::Id sa = LoadScalar(alu.src1_reg);
    Sirit::Id sb = LoadScalar(alu.src2_reg);
    Sirit::Id sout = EmitScalarOp(sop, sa, sb);
    StoreScalar(alu.scalar_dest, sout);

    switch (sop) {
      case Xe::KILLEs:
      case Xe::KILLNEs:
      case Xe::KILLGTs:
      case Xe::KILLGTEs:
      case Xe::KILLONEs:
        if (isPixel) {
          EmitKillFromPredicate(sout);
        }
        break;
      default:
        break;
    }

    Sirit::Id result = EmitVectorOp(vop, s1, s2, s3);

    // Apply write mask to r[vector_dest]
    Sirit::Id dstVal = LoadReg(alu.vector_dest);
    std::array<Sirit::Id, 4> comps{};
    for (u32 i = 0; i < 4; ++i) {
      comps[i] = mod.OpCompositeExtract(floatType, dstVal, i);
    }

    for (u32 i = 0; i < 4; ++i) {
      if (alu.vector_write_mask & (1u << i)) {
        Sirit::Id srcComp = mod.OpCompositeExtract(floatType, result, i);
        comps[i] = srcComp;
      }
    }

    Sirit::Id newVec = mod.OpCompositeConstruct(vec4Type,
      comps[0], comps[1], comps[2], comps[3]);
    StoreReg(alu.vector_dest, newVec);

    // Handle export (simple model: export full vector_dest)
    if (alu.export_data) {
      ExportFromReg(alu.vector_dest, newVec);
    }
  }

  void EmitFetch(const Xe::instr_fetch_t &fetch) {
    auto opc = static_cast<Xe::instr_fetch_opc_t>(fetch.opc);

    if (opc == Xe::instr_fetch_opc_t::TEX_FETCH) {
      const auto &t = fetch.tex;
      // Sample tex[t.const_idx] using coords from src_reg
      Sirit::Id coord = LoadReg(t.src_reg);
      Sirit::Id sampled = EmitTextureSample2D(coord, t.const_idx, t);
      StoreReg(t.dst_reg, sampled);
      return;
    }

    if (opc == Xe::instr_fetch_opc_t::VTX_FETCH && !isPixel) {
      const auto &v = fetch.vtx;
      Sirit::Id vin = LoadVertexInput(v.const_index);
      vin = DecodeVtxFormat(vin, static_cast<Xe::instr_surf_fmt_t>(v.format));
      StoreReg(v.dst_reg, vin);
      return;
    }

    // Other fetch opcodes currently ignored
  }

  void EmitKillFromPredicate(Sirit::Id pred) {
    Sirit::Id cond = mod.OpFOrdEqual(
      mod.TypeBool(),
      pred,
      mod.Constant(floatType, 0.f)
    );

    Sirit::Id killBlock  = mod.OpLabel();
    Sirit::Id mergeBlock = mod.OpLabel();

    mod.OpSelectionMerge(mergeBlock, spv::SelectionControlMask::MaskNone);
    mod.OpBranchConditional(cond, killBlock, mergeBlock);

    mod.AddLabel(killBlock);
    mod.OpKill();

    mod.AddLabel(mergeBlock);
  }

  Sirit::Module mod;
  Xe::eShaderType shaderType;
  bool isPixel{false};
  bool registersFinalized{false};

  // Types
  Sirit::Id voidType{};
  Sirit::Id floatType{};
  Sirit::Id intType{};
  Sirit::Id uintType{};
  Sirit::Id vec2Type{};
  Sirit::Id vec4Type{};
  Sirit::Id vec4PtrInput{};
  Sirit::Id vec4PtrOutput{};
  Sirit::Id vec4PtrFunction{};
  Sirit::Id uboStructType{};
  Sirit::Id uboPtrType{};
  Sirit::Id sampledImage2DType{};

  // UBO for c# constants
  Sirit::Id uboVar{};

  // Entry function
  Sirit::Id mainFunc{};
  Sirit::Id mainLabel{};

  // Register storage: r#
  std::unordered_map<u32, Sirit::Id> regVars; // ptrs (Function storage)

  // Scalar registers: r0.x .. r127.x modeled as float or int
  std::unordered_map<u32, Sirit::Id> scalarRegs;
  Sirit::Id floatPtrFunction{};

  // Vertex inputs (by some "slot" - here we use const_index for simplicity)
  std::unordered_map<u32, Sirit::Id> vertexInputs;

  // Track dense output locations
  std::unordered_map<u32, u32> vsExportLocationMap; // r# -> location
  u32 nextInputLocation = 0;
  u32 nextOutputLocation = 0;

  // Outputs (key: "export register index" = r#)
  std::unordered_map<u32, Sirit::Id> outputs;

  // Textures (by sampler slot)
  std::unordered_map<u32, Sirit::Id> textures;

  // Entry point interface vars (Input/Output)
  std::vector<Sirit::Id> interfaceVars;
};