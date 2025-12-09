// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include <fstream>

#include "Base/Logging/Log.h"
#include "Base/Param.h"
#include "Base/Types.h"

#include "Core/XGPU/Microcode/Constants.h"

#include "xenos_disasm_spirv.h"

REQ_PARAM(file, "Path to shader binary file");

void DumpCF(const Xe::instr_cf_t &cf, u32 pc) {
  LOG_INFO(Core,
    "CF {:04}: {} addr={} count={}",
    pc,
    Xe::GetCFOpcodeName(
      static_cast<Xe::instr_cf_opc_t>(cf.opc)),
    cf.exec.address,
    cf.exec.count);
}

void DumpALU(const Xe::instr_alu_t &alu) {
  const auto vop = static_cast<Xe::instr_vector_opc_t>(alu.vector_opc);
  const auto sop = static_cast<Xe::instr_scalar_opc_t>(alu.scalar_opc);

  const char *vopName = VectorOpName(vop);
  const char *sopName = ScalarOpName(sop);

  std::string dstMask = MaskToString(alu.vector_write_mask);
  std::string vdst = "r" + std::to_string(alu.vector_dest) + "." + dstMask;
  std::string sdst = "r" + std::to_string(alu.scalar_dest);

  std::string src1 = FormatSrcReg(
    alu.src1_reg,
    alu.src1_sel,
    alu.src1_swiz,
    alu.src1_reg_negate != 0
  );
  std::string src2 = FormatSrcReg(
    alu.src2_reg,
    alu.src2_sel,
    alu.src2_swiz,
    alu.src2_reg_negate != 0
  );
  std::string src3 = FormatSrcReg(
    alu.src3_reg,
    alu.src3_sel,
    alu.src3_swiz,
    alu.src3_reg_negate != 0
  );

  const bool doExport = alu.export_data != 0;

  // Pseudo-ASM line:
  //   <VOP> vdst, src1, src2, src3 ; sdst=<>, <SOP>[, export]
  if (doExport) {
    LOG_INFO(Core,
      "  {} {}, {}, {}, {} ; sdst={}, {} export",
      vopName,
      vdst,
      src1,
      src2,
      src3,
      sdst,
      sopName);
  } else {
    LOG_INFO(Core,
      "  {} {}, {}, {}, {} ; sdst={}, {}",
      vopName,
      vdst,
      src1,
      src2,
      src3,
      sdst,
      sopName);
  }
}

void DumpFETCH(const Xe::instr_fetch_t &fetch) {
  auto opc = static_cast<Xe::instr_fetch_opc_t>(fetch.opc);

  if (opc == Xe::instr_fetch_opc_t::VTX_FETCH) {
    const auto &v = fetch.vtx;
    // NOTE: we’re not yet decoding swizzles/format fully here
    LOG_INFO(Core,
      "  VTX_FETCH r{} /*.xyzw*/ <- vtx[r{}], const_idx={} format={} stride={} offset={}",
      v.dst_reg,
      v.src_reg,
      v.const_index,
      static_cast<u32>(v.format),
      v.stride,
      v.offset);
  } else if (opc == Xe::instr_fetch_opc_t::TEX_FETCH) {
    const auto &t = fetch.tex;
    LOG_INFO(Core,
      "  TEX_FETCH r{} /*.xyzw*/ <- tex[r{}] samp={}, const_idx={}, dim={}, offs=({}, {}, {})",
      t.dst_reg,
      t.src_reg,
      t.const_idx,
      t.const_idx,
      static_cast<u32>(t.dimension),
      t.offset_x,
      t.offset_y,
      t.offset_z);
  } else {
    LOG_INFO(Core, "  FETCH opc={}", static_cast<u32>(opc));
  }
}

void ScanRegisters(XenosSpirvCompiler &compiler, ShaderType shaderType, u32 *words, u64 dwordCount, const Xe::instr_cf_t &cf) {
  if (!cf.is_exec())
    return;

  const u32 base = cf.exec.address * 3;

  for (u32 j = 0; j < cf.exec.count; j++) {
    const u32 idx = base + j * 3;
    if (idx + 2 >= dwordCount)
      break;

    Xe::instr_fetch_t fetch{};
    ::memcpy(&fetch, &words[idx], sizeof(fetch));

    auto opc = static_cast<Xe::instr_fetch_opc_t>(fetch.opc);
    bool isFetch = (opc == Xe::instr_fetch_opc_t::VTX_FETCH ||
                    opc == Xe::instr_fetch_opc_t::TEX_FETCH);

    if (compiler.isPixel &&
        opc == Xe::instr_fetch_opc_t::VTX_FETCH) {
      isFetch = false;
    }

    if (isFetch) {
      // dst + src registers
      compiler.TouchRegister(fetch.tex.dst_reg);
      compiler.TouchRegister(fetch.tex.src_reg);
    } else {
      Xe::instr_alu_t alu{};
      ::memcpy(&alu, &words[idx], sizeof(alu));

      compiler.TouchScalar(alu.scalar_dest);
      compiler.TouchScalar(alu.src1_reg);
      compiler.TouchScalar(alu.src2_reg);
      compiler.TouchRegister(alu.vector_dest);
      compiler.TouchRegister(alu.src1_reg);
      compiler.TouchRegister(alu.src2_reg);
      compiler.TouchRegister(alu.src3_reg);
    }
  }
}

bool ProcessExec(XenosSpirvCompiler &compiler, const ShaderType shaderType, u32 *words, u64 dwordCount, const Xe::instr_cf_t &cf) {
  if (!cf.is_exec())
    return false;

  const u32 base = cf.exec.address * 3;
  for (u32 j = 0; j < cf.exec.count; j++) {
    const u32 idx = base + j * 3;
    if (idx + 2 >= dwordCount)
      break;

    Xe::instr_fetch_t fetch{};
    ::memcpy(&fetch, &words[idx], sizeof(fetch));

    auto opc = static_cast<Xe::instr_fetch_opc_t>(fetch.opc);
    bool isFetch = (opc == Xe::instr_fetch_opc_t::VTX_FETCH ||
                    opc == Xe::instr_fetch_opc_t::TEX_FETCH);

    // In pixel shaders, ignore bogus VTX_FETCH classification
    if (compiler.isPixel &&
        opc == Xe::instr_fetch_opc_t::VTX_FETCH) {
      isFetch = false;
    }

    if (isFetch) {
      compiler.EmitFetch(fetch);
      DumpFETCH(fetch);
    } else {
      Xe::instr_alu_t alu{};
      ::memcpy(&alu, &words[idx], sizeof(alu));
      compiler.EmitALU(alu);
      DumpALU(alu);
    }
  }

  return true;
}

s32 ToolMain() {
  std::string path = PARAM_file.Get();
  const ShaderType shaderType = ParseShaderType(path);

  // Read file
  std::vector<u8> data{};
  {
    std::ifstream f{ path, std::ios::in | std::ios::binary };
    if (!f.is_open()) {
      LOG_ERROR(Core, "Failed to open '{}'", path);
      return 1;
    }
    f.seekg(0, std::ios::end);
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);
    if (size < 0) {
      LOG_ERROR(Core, "Failed to get size for '{}'", path);
      return 1;
    }
    data.resize(static_cast<u64>(size));
    f.read(reinterpret_cast<char *>(data.data()), size);
  }

  if (data.size() % 4 != 0) {
    LOG_ERROR(Core, "Shader size not aligned to dword");
    return 1;
  }

  u32 *words = reinterpret_cast<u32 *>(data.data());
  u64 dwordCount = data.size() / 4;

  LOG_INFO(Core, "Loaded {} dwords ({} bytes)", dwordCount, data.size());
  LOG_INFO(Core, "Shader Type: {}", shaderType == ShaderType::Vertex ? "VS" : "PS");
  LOG_INFO(Core, "--------------------------------------------------");

  XenosSpirvCompiler compiler(shaderType);
  compiler.InitModule();
  compiler.BeginMain();

  // Scan all register values (pass 1)
  u32 pc = 0;
  for (u64 i = 0; i + 2 < dwordCount; i += 3, pc += 2) {
    Xe::instr_cf_t cfa{};
    Xe::instr_cf_t cfb{};

    cfa.dword_0 = words[i + 0];
    cfa.dword_1 = words[i + 1] & 0xFFFF;

    cfb.dword_0 = (words[i + 1] >> 16) | (words[i + 2] << 16);
    cfb.dword_1 = words[i + 2] >> 16;

    ScanRegisters(compiler, shaderType, words, dwordCount, cfa);
    ScanRegisters(compiler, shaderType, words, dwordCount, cfb);

    if (cfa.opc == Xe::instr_cf_opc_t::EXEC_END ||
        cfb.opc == Xe::instr_cf_opc_t::EXEC_END)
      break;
  }

  compiler.FinalizeRegisters();

  compiler.AllocateAllRegisters();

  // Actually emit code (pass 2)
  pc = 0;
  for (u64 i = 0; i + 2 < dwordCount; i += 3, pc += 2) {
    Xe::instr_cf_t cfa{};
    Xe::instr_cf_t cfb{};

    cfa.dword_0 = words[i + 0];
    cfa.dword_1 = words[i + 1] & 0xFFFF;

    cfb.dword_0 = (words[i + 1] >> 16) | (words[i + 2] << 16);
    cfb.dword_1 = words[i + 2] >> 16;

    DumpCF(cfa, pc);
    ProcessExec(compiler, shaderType, words, dwordCount, cfa);

    DumpCF(cfb, pc + 1);
    ProcessExec(compiler, shaderType, words, dwordCount, cfb);

    if (cfa.opc == Xe::instr_cf_opc_t::EXEC_END ||
        cfb.opc == Xe::instr_cf_opc_t::EXEC_END)
      break;
  }

  compiler.EndMain();

  // Assemble SPIR-V and write to <file>.spv
  std::vector<u32> spirv = compiler.mod.Assemble();
  std::string outPath = path + ".spv";
  {
    std::ofstream out(outPath, std::ios::binary);
    out.write(reinterpret_cast<const char *>(spirv.data()), spirv.size() * sizeof(u32));
  }

  LOG_INFO(Core, "Wrote SPIR-V to '{}'", outPath);

  return 0;
}

extern s32 ToolMain();
PARAM(help, "Prints this message", false);
s32 main(s32 argc, char *argv[]) {
  // Init params
  Base::Param::Init(argc, argv);
  // Handle help param
  if (PARAM_help.Present()) {
    ::Base::Param::Help();
    return 0;
  }
  return ToolMain();
}