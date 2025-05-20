// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include <fstream>

#include "Base/CRCHash.h"
#include "Base/Logging/Log.h"
#include "Base/Param.h"

#include "Core/XGPU/ShaderConstants.h"

#include "Core/XGPU/Microcode/ASTBlock.h"
#include "Core/XGPU/Microcode/ASTNodeWriter.h"

namespace Xe::Microcode {
  
class GlobalInstructionExtractor : public Microcode::AST::ExpressionNode::Visitor {
public:
  virtual void OnExprStart(Microcode::AST::ExpressionNode::Ptr n) override final {
    if (n->GetType() == Microcode::AST::eExprType::VFETCH) {
      vfetch.push_back(static_cast<Microcode::AST::VertexFetch::Ptr>(n));
    }
    else if (n->GetType() == Microcode::AST::eExprType::TFETCH) {
      tfetch.push_back(static_cast<Microcode::AST::TextureFetch::Ptr>(n));
    }
    else if (n->GetType() == Microcode::AST::eExprType::EXPORT) {
      exports.push_back(static_cast<Microcode::AST::WriteExportRegister::Ptr>(n));
    }
    else {
      const s32 regIndex = n->GetRegisterIndex();
      if (regIndex != -1) {
        usedRegisters.insert((u32)regIndex);
      }
    }
  }

  virtual void OnExprEnd(Microcode::AST::ExpressionNode::Ptr n) override final
  {}

  std::vector<Microcode::AST::VertexFetch::Ptr> vfetch;
  std::vector<Microcode::AST::TextureFetch::Ptr> tfetch;
  std::vector<Microcode::AST::WriteExportRegister::Ptr> exports;
  std::set<u32> usedRegisters;
};


void Run(u32 hash) {
  eShaderType shaderType = eShaderType::Unknown;
  fs::path shaderPath{ "C:/Users/Vali/Desktop/shaders" };
  std::string fileName = std::format("vertex_shader_{:X}.bin", hash);
  std::ifstream f{ shaderPath / fileName, std::ios::out | std::ios::binary };
  if (!f.is_open()) {
    shaderType = eShaderType::Pixel;
    fileName = std::format("pixel_shader_{:X}.bin", hash);
    f.open(shaderPath / fileName, std::ios::out | std::ios::binary);
    if (!f.is_open()) {
      LOG_ERROR(Filesystem, "Invalid file! CRC: 0x{:X}", hash);
      return;
    }
    LOG_INFO(Base, "Loaded Pixel Shader '{}'", fileName);
  } else {
    shaderType = eShaderType::Vertex;
    LOG_INFO(Base, "Loaded Vertex Shader '{}'", fileName);
  }
  u64 fileSize = fs::file_size(shaderPath / fileName);
  LOG_INFO(Base, "Shader size: {} (0x{:X})", fileSize, fileSize);

  std::vector<u32> data{};
  data.resize(fileSize / 4);
  f.read(reinterpret_cast<char*>(data.data()), fileSize);
  f.close();
  LOG_INFO(Base, "Shader data:");
  {
    u32 lastR = -1;
    u32 i = 0, r = 0, c = 0;
    for (u32 &v : data) {
      if (lastR != r) {
        LOG_INFO_BASE(Base, "[{}] ", r);
        lastR = r;
      }
      std::cout << std::format("0x{:08X}", v);
      i++;
      c++;
      if (i == 4) {
        r++;
        std::cout << std::endl;
        i = 0;
      } else if (c != data.size()) {
          std::cout << ", ";
      }
    }
  }

  AST::Shader *shader = AST::Shader::DecompileMicroCode(reinterpret_cast<u8*>(data.data()), data.size() * 4, shaderType);
  AST::ShaderCodeWriterSirit writer{ shaderType, shader };
  if (shader) {
    shader->EmitShaderCode(writer);
  }
  std::vector<u32> code{ writer.module.Assemble() };
  std::string typeString = shaderType == Xe::eShaderType::Pixel ? "pixel" : "vertex";
  std::string baseString = std::format("{}_shader_{:X}", typeString, hash);
  {
    std::ofstream f{ shaderPath / (baseString + ".spv"), std::ios::out | std::ios::binary };
    f.write(reinterpret_cast<char*>(code.data()), code.size() * 4);
    f.close();
  }

  LOG_INFO(Base, "SPIR-V data:");
  {
    u32 lastR = -1;
    u32 i = 0, r = 0, c = 0;
    for (u32 &v : code) {
      if (lastR != r) {
        LOG_INFO_BASE(Base, "[{}] ", r);
        lastR = r;
      }
      std::cout << std::format("0x{:08X}", v);
      i++;
      c++;
      if (i == 4) {
        r++;
        std::cout << std::endl;
        i = 0;
      } else if (c != data.size()) {
          std::cout << ", ";
      }
    }
  }
}

}

PARAM(crc, "CRC Hash to the shader");
PARAM(help, "Prints this message", false);

s32 main(s32 argc, char *argv[]) {
  // Init params
  Base::Param::Init(argc, argv);
  // Handle help param
  if (PARAM_help.Present()) {
    ::Base::Param::Help();
    return 0;
  }
  u32 crcHash = PARAM_crc.Get<u32>();
  if (crcHash == 0) {
    // Vertex: 0x2F4DC4B6
    // Pixel: 0x65924BD7
    crcHash = 0x3D136A96;
  }
  Xe::Microcode::Run(crcHash);
  //Xe::Microcode::Run(0x65924BD7);
  return 0;
}