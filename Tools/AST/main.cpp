// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include <fstream>

#include "Base/CRCHash.h"
#include "Base/Logging/Log.h"
#include "Base/Param.h"

#include "Core/XGPU/ShaderConstants.h"

#include "Core/XGPU/Microcode/ASTBlock.h"
#include "Core/XGPU/Microcode/ASTNodeWriter.h"

namespace Xe::Microcode {

u32 pixelShaderHash = 0;
u32 vertexShaderHash = 0;
std::unordered_map<u32, AST::Shader*> shaders{};

void Write(u32 hash, eShaderType shaderType, std::vector<u32> code) {
  fs::path shaderPath{ "C:/Users/Vali/Desktop/shaders" };
  std::string typeString = shaderType == Xe::eShaderType::Pixel ? "pixel" : "vertex";
  std::string baseString = std::format("{}_shader_{:X}", typeString, hash);
  {
    std::ofstream f{ shaderPath / (baseString + ".spv"), std::ios::out | std::ios::binary };
    f.write(reinterpret_cast<char *>(code.data()), code.size() * 4);
    f.close();
  }
}

void Handle() {
  AST::Shader *vertexShader = shaders[vertexShaderHash];
  AST::Shader *pixelShader = shaders[pixelShaderHash];
  AST::ShaderCodeWriterSirit vertexWriter{ eShaderType::Vertex, vertexShader };
  if (vertexShader) {
    vertexShader->EmitShaderCode(vertexWriter);
  }
  std::vector<u32> vertexCode{ vertexWriter.module.Assemble() };
  Write(vertexShaderHash, eShaderType::Vertex, vertexCode);
  AST::ShaderCodeWriterSirit pixelWriter{ eShaderType::Pixel, pixelShader };
  if (pixelShader) {
    pixelShader->EmitShaderCode(pixelWriter);
  }
  std::vector<u32> pixelCode{ pixelWriter.module.Assemble() };
  Write(pixelShaderHash, eShaderType::Pixel, pixelCode);
}

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
  shaders.insert({ hash, shader });
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
    // Vertex: 0x3D136A96
    // Pixel: 0x514B8980
    crcHash = 0x3D136A96;
  }
  Xe::Microcode::pixelShaderHash = 0x514B8980;
  Xe::Microcode::vertexShaderHash = 0x3D136A96;
  Xe::Microcode::Run(Xe::Microcode::pixelShaderHash);
  Xe::Microcode::Run(Xe::Microcode::vertexShaderHash);
  Xe::Microcode::Handle();
  return 0;
}