/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "VulkanShader.h"

#ifndef NO_GFX

inline void InitGlslang() {
  static bool initialized = false;
  if (!initialized) {
    glslang::InitializeProcess();
    initialized = true;
  }
}

inline EShLanguage ToStage(Render::eShaderType type) {
  switch (type) {
  case Render::eShaderType::Vertex: return EShLangVertex;
  case Render::eShaderType::Fragment: return EShLangFragment;
  case Render::eShaderType::Compute:  return EShLangCompute;
  default: return EShLangCount;
  }
}

inline const TBuiltInResource &DefaultResources() {
  static TBuiltInResource resources = {
    .maxLights = 32,
    .maxClipPlanes = 6,
    .maxTextureUnits = 32,
    .maxTextureCoords = 32,
    .maxVertexAttribs = 64,
    .maxVertexUniformComponents = 4096,
    .maxVaryingFloats = 64,
    .maxVertexTextureImageUnits = 32,
    .maxCombinedTextureImageUnits = 80,
    .maxTextureImageUnits = 32,
    .maxFragmentUniformComponents = 4096,
    .maxDrawBuffers = 32,
    .maxVertexUniformVectors = 128,
    .maxVaryingVectors = 8,
    .maxFragmentUniformVectors = 16,
    .maxVertexOutputVectors = 16,
    .maxFragmentInputVectors = 15,
    .minProgramTexelOffset = -8,
    .maxProgramTexelOffset = 7,
    .maxClipDistances = 8,
    .maxComputeWorkGroupCountX = 65535,
    .maxComputeWorkGroupCountY = 65535,
    .maxComputeWorkGroupCountZ = 65535,
    .maxComputeWorkGroupSizeX = 1024,
    .maxComputeWorkGroupSizeY = 1024,
    .maxComputeWorkGroupSizeZ = 64,
    .maxComputeUniformComponents = 1024,
    .maxComputeTextureImageUnits = 16,
    .maxComputeImageUniforms = 8,
    .maxComputeAtomicCounters = 8,
    .maxComputeAtomicCounterBuffers = 1,
    .maxVaryingComponents = 60,
    .maxVertexOutputComponents = 64,
    .maxGeometryInputComponents = 64,
    .maxGeometryOutputComponents = 128,
    .maxFragmentInputComponents = 128,
    .maxImageUnits = 8,
    .maxCombinedImageUnitsAndFragmentOutputs = 8,
    .maxCombinedShaderOutputResources = 8,
    .maxImageSamples = 0,
    .maxVertexImageUniforms = 0,
    .maxTessControlImageUniforms = 0,
    .maxTessEvaluationImageUniforms = 0,
    .maxGeometryImageUniforms = 0,
    .maxFragmentImageUniforms = 8,
    .maxCombinedImageUniforms = 8,
    .maxGeometryTextureImageUnits = 16,
    .maxGeometryOutputVertices = 256,
    .maxGeometryTotalOutputComponents = 1024,
    .maxGeometryUniformComponents = 1024,
    .maxGeometryVaryingComponents = 64,
    .maxTessControlInputComponents = 128,
    .maxTessControlOutputComponents = 128,
    .maxTessControlTextureImageUnits = 16,
    .maxTessControlUniformComponents = 1024,
    .maxTessControlTotalOutputComponents = 4096,
    .maxTessEvaluationInputComponents = 128,
    .maxTessEvaluationOutputComponents = 128,
    .maxTessEvaluationTextureImageUnits = 16,
    .maxTessEvaluationUniformComponents = 1024,
    .maxTessPatchComponents = 120,
    .maxPatchVertices = 32,
    .maxTessGenLevel = 64,
    .maxViewports = 16,
    .maxVertexAtomicCounters = 0,
    .maxTessControlAtomicCounters = 0,
    .maxTessEvaluationAtomicCounters = 0,
    .maxGeometryAtomicCounters = 0,
    .maxFragmentAtomicCounters = 8,
    .maxCombinedAtomicCounters = 8,
    .maxAtomicCounterBindings = 1,
    .maxVertexAtomicCounterBuffers = 0,
    .maxTessControlAtomicCounterBuffers = 0,
    .maxTessEvaluationAtomicCounterBuffers = 0,
    .maxGeometryAtomicCounterBuffers = 0,
    .maxFragmentAtomicCounterBuffers = 1,
    .maxCombinedAtomicCounterBuffers = 1,
    .maxAtomicCounterBufferSize = 16384,
    .maxTransformFeedbackBuffers = 4,
    .maxTransformFeedbackInterleavedComponents = 64,
    .maxCullDistances = 8,
    .maxCombinedClipAndCullDistances = 8,
    .maxSamples = 4,
    .limits = {
        .nonInductiveForLoops = true,
        .whileLoops = true,
        .doWhileLoops = true,
        .generalUniformIndexing = true,
        .generalAttributeMatrixVectorIndexing = true,
        .generalVaryingIndexing = true,
        .generalSamplerIndexing = true,
        .generalVariableIndexing = true,
        .generalConstantMatrixVectorIndexing = true
    }
  };
  return resources;
}


void Render::VulkanShader::CompileFromSource(eShaderType type, const char *source) {
  InitGlslang();

  EShLanguage stage = ToStage(type);
  if (stage == EShLangCount) {
    LOG_ERROR(Render, "Unsupported shader type.");
    return;
  }

  glslang::TShader shader(stage);
  shader.setStrings(&source, 1);

  shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100);
  shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
  shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);

  EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
  if (!shader.parse(&DefaultResources(), 450, false, messages)) {
    LOG_ERROR(Render, "Shader parse failed: {}", shader.getInfoLog());
    return;
  }

  glslang::TProgram program;
  program.addShader(&shader);
  if (!program.link(messages)) {
    LOG_ERROR(Render, "Shader link failed: {}", program.getInfoLog());
    return;
  }

  std::vector<uint32_t> spirv;
  glslang::SpvOptions spvOptions;
  spvOptions.generateDebugInfo = true;
  spvOptions.disableOptimizer = false;
  spvOptions.optimizeSize = false;

  glslang::GlslangToSpv(*program.getIntermediate(stage), spirv, &spvOptions);

  if (!spirv.empty()) {
    CompileFromBinary(type, reinterpret_cast<const u8*>(spirv.data()), spirv.size() * sizeof(uint32_t));
  } else {
    LOG_ERROR(Render, "Failed to generate SPIR-V.");
  }
}

void Render::VulkanShader::CompileFromBinary(eShaderType type, const u8 *data, u64 size) {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = size;
  createInfo.pCode = reinterpret_cast<const u32 *>(data);

  VkShaderModule shaderModule = VK_NULL_HANDLE;
  if (renderer->dispatch.createShaderModule(&createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
    LOG_ERROR(Render, "Failed to create Vulkan shader module");
    return;
  }

  switch (type) {
  case eShaderType::Vertex:
    vertexShader = shaderModule;
    break;
  case eShaderType::Fragment:
    fragmentShader = shaderModule;
    break;
  case eShaderType::Compute:
    computeShader = shaderModule;
    break;
  default:
    LOG_ERROR(Render, "Unsupported shader type.");
    renderer->dispatch.destroyShaderModule(shaderModule, nullptr);
    break;
  }
}

s32 Render::VulkanShader::GetUniformLocation(const std::string &name) {

  return 0;
}

void Render::VulkanShader::SetUniformInt(const std::string &name, s32 value) {

}

void Render::VulkanShader::SetUniformFloat(const std::string &name, f32 value) {

}

void Render::VulkanShader::SetVertexShaderConsts(u32 baseVector, u32 count, const f32 *data) {

}

void Render::VulkanShader::SetPixelShaderConsts(u32 baseVector, u32 count, const f32 *data) {

}

void Render::VulkanShader::SetBooleanConstants(const u32 *data) {

}

bool Render::VulkanShader::Link() {
  return true;
}

void Render::VulkanShader::Bind() {

}

void Render::VulkanShader::Unbind() {

}

void Render::VulkanShader::Destroy() {
  if (vertexShader) {
    renderer->dispatch.destroyShaderModule(vertexShader, nullptr);
    vertexShader = VK_NULL_HANDLE;
  }
  if (fragmentShader) {
    renderer->dispatch.destroyShaderModule(fragmentShader, nullptr);
    fragmentShader = VK_NULL_HANDLE;
  }
  if (computeShader) {
    renderer->dispatch.destroyShaderModule(computeShader, nullptr);
    computeShader = VK_NULL_HANDLE;
  }
}

#endif
