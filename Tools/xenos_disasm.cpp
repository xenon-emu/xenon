// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include <fstream>

#include "Base/Logging/Log.h"
#include "Base/Param.h"
#include "Base/Types.h"

#include "Core/XGPU/Microcode/Constants.h"

#include "xenos_disasm_spirv.h"

REQ_PARAM(file, "Path to shader binary file");

static s32 sext5(u32 v) {
  v &= 0x1F;
  return (v & 0x10) ? (s32)v - 0x20 : (s32)v;
}

static const char *DimToMnemonic(u32 dim) {
  switch ((Xe::instr_dimension_t)dim) {
    case Xe::DIMENSION_1D: return "tfetch1D";
    case Xe::DIMENSION_2D: return "tfetch2D";
    case Xe::DIMENSION_3D: return "tfetch3D";
    case Xe::DIMENSION_CUBE: return "tfetchCube";
  }
  return "tfetch2D";
}

static const char *VopToXpsMnemonic(Xe::instr_vector_opc_t vop) {
  switch (vop) {
    case Xe::ADDv: return "add";
    case Xe::MULv: return "mul";
    case Xe::MAXv: return "max";
    case Xe::MINv: return "min";
    case Xe::MULADDv: return "mad";
    case Xe::DOT4v: return "dot4";
    case Xe::DOT3v: return "dot3";
    case Xe::MOVAv: return "mov";
    default: return nullptr;
  }
}

static const char *SopToXpsMnemonic(Xe::instr_scalar_opc_t sop) {
  switch (sop) {
    case Xe::MUL_CONST_1: return "mulsc";
    case Xe::MULs: return "mulsc";
    case Xe::ADDs: return "addsc";
    default: return nullptr;
  }
}

struct XpsWriter {
  std::ostringstream out;
  bool isPixel;

  void Begin(Xe::eShaderType t) {
    isPixel = (t == Xe::eShaderType::Pixel);
    out << (isPixel ? "xps_3_0\n" : "xvs_3_0\n");
    out << "config AutoSerialize=false\n";
    if (isPixel) out << "config PsMaxReg=64\n";
  }

  void EmitDclTexcoord(s32 r, std::string mask = "xy") {
    out << "dcl_texcoord r" << r << "." << mask << "\n";
  }

  void EmitDefConst(s32 c, bool vec4 = true) {
    if (vec4)
      out << "defconst c" << c << ", float, vector, [1, 4], c" << c << "\n";
    else
      out << "defconst c" << c << ", float, scalar, [1, 1], c" << c << "\n";
  }

  void EmitLine(std::string s) {
    out << s << "\n";
  }

  void EmitExec(bool end) {
    out << (end ? "exece\n" : "exec\n");
  }

  void EmitAllocColors() {
    out << "alloc colors\n";
  }

  void EmitCnop() {
    out << "cnop\n";
  }

  void EmitXpsTexFetch(const Xe::instr_fetch_tex_t &t) {
    std::string coord;
    switch ((Xe::instr_dimension_t)t.dimension) {
      case Xe::DIMENSION_1D:
        coord = "r" + std::to_string(t.src_reg) + ".x";
        break;
      case Xe::DIMENSION_3D:
      case Xe::DIMENSION_CUBE:
        coord = "r" + std::to_string(t.src_reg) + ".xyz";
        break;
      default:
        coord = "r" + std::to_string(t.src_reg) + ".xy";
        break;
    }

    auto sx = sext5(t.offset_x);
    auto sy = sext5(t.offset_y);
    auto sz = sext5(t.offset_z);

    out << "tfetch2D r" << t.dst_reg << ", r" << t.src_reg << ".xy, tf" << t.const_idx;
    if (sx || sy || sz) out << " // offs=(" << sx << "," << sy << "," << sz << ")";
    out << "\n";
  }

  void EmitXpsAlu(const Xe::instr_alu_t &alu) {
    auto vop = (Xe::instr_vector_opc_t)alu.vector_opc;
    const char *m = VopToXpsMnemonic(vop);
    if (!m) {
      out << "// unhandled vop " << (u32)alu.vector_opc << "\n";
      return;
    }

    std::string dstMask = MaskToString(alu.vector_write_mask);
    std::string dst = alu.export_data ? "oC0" : ("r" + std::to_string(alu.vector_dest));
    out << m << " " << dst;
    if (!dstMask.empty()) out << "." << dstMask;

    auto s1 = FormatSrcReg(alu.src1_reg, alu.src1_sel, alu.src1_swiz, alu.src1_reg_negate);
    auto s2 = FormatSrcReg(alu.src2_reg, alu.src2_sel, alu.src2_swiz, alu.src2_reg_negate);
    auto s3 = FormatSrcReg(alu.src3_reg, alu.src3_sel, alu.src3_swiz, alu.src3_reg_negate);

    // Operand count rules
    if (vop == Xe::MOVAv) {
      out << ", " << s1 << "\n";
    } else if (vop == Xe::MULADDv) {
      out << ", " << s1 << ", " << s2 << ", " << s3 << "\n";
    } else {
      // dot4/dot3/add/mul/min/max
      out << ", " << s1 << ", " << s2 << "\n";
    }

    // Scalar-side annotation
    auto sop = (Xe::instr_scalar_opc_t)alu.scalar_opc;
    if (sop != Xe::RETAIN_PREV) {
      const char* sm = SopToXpsMnemonic(sop);
      if (sm) {
        // super crude scalar lane: use .x and src1/src2 .x
        auto s1 = FormatSrcReg(alu.src1_reg, alu.src1_sel, alu.src1_swiz, alu.src1_reg_negate);
        auto s2 = FormatSrcReg(alu.src2_reg, alu.src2_sel, alu.src2_swiz, alu.src2_reg_negate);

        // pick scalar dest. If export_data and scalar mask hits W, write oC0.w else r<sdst>.x
        // (you may have scalar_write_mask somewhere; if not, assume .x for now)
        std::string sdst = alu.export_data ? "oC0.w" : ("r" + std::to_string(alu.scalar_dest) + ".x");

        out << sm << " " << sdst << ", " << s1 << ", " << s2 << "\n";
      } else {
        out << "// sop=" << ScalarOpName(sop) << " sdst=r" << alu.scalar_dest << "\n";
      }
    }
  }
};

XpsWriter xps;

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

  LOG_INFO(Core, "ALU raw: vop={} sop={} vdst={} vmask={:x} sdst={} "
              "s1(reg={},sel={},swiz={:02x},neg={}) "
              "s2(reg={},sel={},swiz={:02x},neg={}) "
              "s3(reg={},sel={},swiz={:02x},neg={}) export={}",
  alu.vector_opc, alu.scalar_opc, alu.vector_dest, alu.vector_write_mask, alu.scalar_dest,
  alu.src1_reg, alu.src1_sel, alu.src1_swiz, alu.src1_reg_negate,
  alu.src2_reg, alu.src2_sel, alu.src2_swiz, alu.src2_reg_negate,
  alu.src3_reg, alu.src3_sel, alu.src3_swiz, alu.src3_reg_negate,
  alu.export_data);
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
      "  TEX_FETCH r{} <- tex[r{}] tf={}, dim={}, offs=({}, {}, {})",
      t.dst_reg, t.src_reg, t.const_idx, (u32)t.dimension,
      sext5(t.offset_x), sext5(t.offset_y), sext5(t.offset_z));
  } else {
    LOG_INFO(Core, "  FETCH opc={}", static_cast<u32>(opc));
  }
}

enum class PacketKind { Fetch, Alu };

static PacketKind ClassifyPacket(const Xe::instr_fetch_t &f, bool isPixel) {
  auto opc = static_cast<Xe::instr_fetch_opc_t>(f.opc);

  // Pixel shaders: treat VTX as ALU
  if (isPixel && opc == Xe::instr_fetch_opc_t::VTX_FETCH)
    return PacketKind::Alu;

  auto looksLikeVtx = [&] {
    const auto &v = f.vtx;
    if (v.dst_reg >= 64 || v.src_reg >= 64)
      return false;
    if (v.const_index > 0xFF)
      return false;
    if (v.stride > 0x3F)
      return false;
    // Catches 3080959 / 65791
    if (v.offset > 0xFFFF)
      return false;
    return true;
  };

  auto looksLikeTex = [&] {
    const auto &t = f.tex;
    if (t.dst_reg >= 64 || t.src_reg >= 64)
      return false;
    if (t.const_idx > 0xFF)
      return false;
    if (t.dimension > 5)
      return false;
    // offsets are usually small 5-bit signed-ish; reject huge values
    if ((u32)std::abs((int)t.offset_x) > 32)
      return false;
    if ((u32)std::abs((int)t.offset_y) > 32)
      return false;
    if ((u32)std::abs((int)t.offset_z) > 32)
      return false;
    return true;
  };

  if (opc == Xe::instr_fetch_opc_t::VTX_FETCH)
    return looksLikeVtx() ? PacketKind::Fetch : PacketKind::Alu;

  if (opc == Xe::instr_fetch_opc_t::TEX_FETCH)
    return looksLikeTex() ? PacketKind::Fetch : PacketKind::Alu;

  return PacketKind::Alu;
}

void ScanRegisters(XenosSpirvCompiler &compiler, Xe::eShaderType shaderType, u32 *words, u64 dwordCount, const Xe::instr_cf_t &cf) {
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

    if (ClassifyPacket(fetch, compiler.isPixel) == PacketKind::Fetch) {
      // dst + src registers
      if (opc == Xe::instr_fetch_opc_t::VTX_FETCH) {
        compiler.TouchRegister(fetch.vtx.dst_reg);
        compiler.TouchRegister(fetch.vtx.src_reg);
      } else {
        compiler.TouchRegister(fetch.tex.dst_reg);
        compiler.TouchRegister(fetch.tex.src_reg);
      }
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

bool ProcessExec(XenosSpirvCompiler &compiler, const Xe::eShaderType shaderType, u32 *words, u64 dwordCount, const Xe::instr_cf_t &cf) {
  if (!cf.is_exec())
    return false;

  const u32 base = cf.exec.address * 3;
  for (u32 j = 0; j < cf.exec.count; j++) {
    const u32 idx = base + j * 3;
    if (idx + 2 >= dwordCount)
      break;

    Xe::instr_fetch_t fetch{};
    ::memcpy(&fetch, &words[idx], sizeof(fetch));

    if (ClassifyPacket(fetch, compiler.isPixel) == PacketKind::Fetch) {
      if (fetch.opc == Xe::instr_fetch_opc_t::TEX_FETCH)
        xps.EmitXpsTexFetch(fetch.tex);
      else
        xps.EmitLine("// vtx_fetch ... (todo)");
      compiler.EmitFetch(fetch);
      DumpFETCH(fetch);
    } else {
      Xe::instr_alu_t alu{};
      ::memcpy(&alu, &words[idx], sizeof(alu));
      compiler.EmitALU(alu);
      DumpALU(alu);
      xps.EmitXpsAlu(alu);
    }
  }

  return true;
}

struct XenosShaderHeader {
  u32 magic;
  u32 offset;
  u32 unk1[3];
  u32 off_constants;
  u32 off_shader;
  u32 unk2[2];
};

struct XenosShaderData {
  u32 sh_off, sh_size;
  u32 program_control, context_misc;

  u32 unk1[4];
};

s32 ToolMain() {
  std::string path = PARAM_file.Get();
  const Xe::eShaderType shaderType = ParseShaderType(path);

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

  std::vector<u32> words(data.size() / 4);
  ::memcpy(words.data(), data.data(), words.size() * sizeof(u32));
  auto fileSize = (u64)data.size();

  auto InRange = [&](u64 off, u64 size = 1) {
    return off <= fileSize && (off + size) <= fileSize;
  };

  auto LooksLikeShaderdata = [&](const XenosShaderData& sd_be, u32 hdrOffset) -> std::optional<u64> {
    XenosShaderData sd = sd_be;
    sd.sh_off = byteswap_be(sd.sh_off);
    sd.sh_size = byteswap_be(sd.sh_size);
    sd.program_control = byteswap_be(sd.program_control);
    sd.context_misc = byteswap_be(sd.context_misc);

    // Microcode start candidate
    u64 off = (u64)sd.sh_off + (u64)hdrOffset;

    if ((off & 3) != 0)
      return std::nullopt;
    if (!InRange(off, 12))
      return std::nullopt;
    if (sd.sh_size == 0 || sd.sh_size > fileSize)
      return std::nullopt;
    if ((sd.sh_size & 3) != 0)
      return std::nullopt;

    // 16MB guardrail
    if (sd.sh_off > 0x1000000)
      return std::nullopt;

    return off;
  };

  // Check for Xenos shader header
  if ((data.size() / 4) >= 8) { // header is 8 dwords
    XenosShaderHeader hdr{};
    std::memcpy(&hdr, data.data(), sizeof(hdr));
    hdr.magic = byteswap_be(hdr.magic);
    hdr.offset = byteswap_be(hdr.offset);
    hdr.off_constants = byteswap_be(hdr.off_constants);
    hdr.off_shader = byteswap_be(hdr.off_shader);
    std::cout << "Header magic: 0x" << std::hex << (hdr.magic >> 16) << std::endl;
    std::cout << "Shader code in binary: 0x" << std::hex << hdr.off_shader << std::endl;
    std::cout << "Shader constants in binary: 0x" << std::hex << hdr.off_constants << std::endl;
    std::cout << "Offset in binary: 0x" << std::hex << hdr.offset << std::endl;
    if ((hdr.magic >> 16) == 0x102A) {
      u64 shaderCodeOffset = ~0ull;
      if (hdr.off_shader != 0 && InRange(hdr.off_shader, sizeof(XenosShaderData))) {
        XenosShaderData sd_be{};
        std::memcpy(&sd_be, data.data() + hdr.off_shader, sizeof(sd_be));

        if (auto off = LooksLikeShaderdata(sd_be, hdr.offset)) {
          shaderCodeOffset = *off;
        }
      }
      if (shaderCodeOffset == ~0ull) {
        shaderCodeOffset = hdr.offset;
        LOG_INFO(Core, "Using older SR1-era microcode offset (hdr.offset)");
      }
      if (!InRange(shaderCodeOffset, 12)) {
        LOG_ERROR(Core, "Invalid microcode offset: 0x{:X}", shaderCodeOffset);
        return 1;
      }
      u8 *shaderCode = data.data() + shaderCodeOffset;
      LOG_INFO(Core, "Detected Xenos shader header, skipping {} dwords", (shaderCodeOffset) / 4);

      // Convert remaining shader data into a u32 vector
      words.clear();
      words.resize((data.size() - shaderCodeOffset) / 4);
      ::memcpy(words.data(), shaderCode, words.size() * sizeof(u32));

      // Byteswap each dword
      for (u32 &w : words) {
        w = byteswap_be(w);
      }
    }
  }

  LOG_INFO(Core, "Loaded {} dwords ({} bytes)", words.size(), words.size() * 4);
  LOG_INFO(Core, "Shader Type: {}", shaderType == Xe::eShaderType::Vertex ? "VS" : "PS");
  LOG_INFO(Core, "--------------------------------------------------");

  XenosSpirvCompiler compiler(shaderType);
  compiler.InitModule();
  compiler.BeginMain();

  // Scan all register values (pass 1)
  u32 pc = 0;
  for (u64 i = 0; i + 2 < words.size(); i += 3, pc += 2) {
    Xe::instr_cf_t cfa{};
    Xe::instr_cf_t cfb{};

    cfa.dword_0 = words[i + 0];
    cfa.dword_1 = words[i + 1] & 0xFFFF;

    cfb.dword_0 = (words[i + 1] >> 16) | (words[i + 2] << 16);
    cfb.dword_1 = words[i + 2] >> 16;

    ScanRegisters(compiler, shaderType, words.data(), words.size(), cfa);
    ScanRegisters(compiler, shaderType, words.data(), words.size(), cfb);

    if (cfa.opc == Xe::instr_cf_opc_t::EXEC_END ||
        cfb.opc == Xe::instr_cf_opc_t::EXEC_END)
      break;
  }

  compiler.FinalizeRegisters();

  compiler.AllocateAllRegisters();
  xps.Begin(shaderType);

  // Actually emit code (pass 2)
  pc = 0;
  for (u64 i = 0; i + 2 < words.size(); i += 3, pc += 2) {
    bool sawEnd = false;
    Xe::instr_cf_t cfa{};
    Xe::instr_cf_t cfb{};

    cfa.dword_0 = words[i + 0];
    cfa.dword_1 = words[i + 1] & 0xFFFF;

    cfb.dword_0 = (words[i + 1] >> 16) | (words[i + 2] << 16);
    cfb.dword_1 = words[i + 2] >> 16;

    auto EmitCfPreamble = [&](const Xe::instr_cf_t& cf) {
      switch ((Xe::instr_cf_opc_t)cf.opc) {
        case Xe::EXEC:
        case Xe::COND_EXEC:
        case Xe::COND_PRED_EXEC:
        case Xe::COND_EXEC_PRED_CLEAN:
          xps.EmitExec(false);
          break;

        case Xe::EXEC_END:
        case Xe::COND_EXEC_END:
        case Xe::COND_PRED_EXEC_END:
        case Xe::COND_EXEC_PRED_CLEAN_END:
          xps.EmitExec(true);
          break;

        case Xe::ALLOC:
          if (shaderType == Xe::eShaderType::Pixel)
            xps.EmitAllocColors();
          else
            xps.EmitLine("// alloc (todo)");
          break;

        default:
          break;
      }
    };

    EmitCfPreamble(cfa);
    DumpCF(cfa, pc);
    ProcessExec(compiler, shaderType, words.data(), words.size(), cfa);
    if ((Xe::instr_cf_opc_t)cfa.opc == Xe::EXEC_END) sawEnd = true;

    EmitCfPreamble(cfb);
    DumpCF(cfb, pc + 1);
    ProcessExec(compiler, shaderType, words.data(), words.size(), cfb);
    if ((Xe::instr_cf_opc_t)cfb.opc == Xe::EXEC_END) sawEnd = true;

    if (sawEnd) {
      xps.EmitCnop();
      break;
    }
  }

  compiler.EndMain();

  // Assemble SPIR-V and write to <file>.spv
  std::vector<u32> spirv = compiler.mod.Assemble();
  std::string outPath = path + ".spv";
  {
    std::ofstream out(outPath, std::ios::binary);
    out.write(reinterpret_cast<const char *>(spirv.data()), spirv.size() * sizeof(u32));
  }

  std::ofstream out(path + ".xps");
  out << xps.out.str();

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