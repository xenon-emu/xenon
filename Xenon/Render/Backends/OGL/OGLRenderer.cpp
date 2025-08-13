// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Core/XeMain.h"

#include "Render/GUI/OpenGL.h"
#include "Render/OpenGL/Factory/OGLResourceFactory.h"
#include "Render/OpenGL/OGLTexture.h"
#include "Render/Backends/OGL/OGLRenderer.h"
#include "Render/Backends/OGL/OGLShaders.h"

#define SANITY_CHECK(x) if (!x) { LOG_ERROR(Xenon, "Failed to initialize SDL: {}", SDL_GetError()); }

#ifndef NO_GFX
namespace Render {

OGLRenderer::OGLRenderer(RAM *ram) :
  Renderer(ram)
{}

std::string OGLRenderer::GLVersion() const {
  return reinterpret_cast<const char *>(glGetString(GL_VERSION));
}

std::string OGLRenderer::GLVendor() const {
  return reinterpret_cast<const char *>(glGetString(GL_VENDOR));
}

std::string OGLRenderer::GLRenderer() const {
  return reinterpret_cast<const char *>(glGetString(GL_RENDERER));
}

void OGLRenderer::BackendStart() {
  // Create the resource factory
  resourceFactory = std::make_unique<OGLResourceFactory>();
  fs::path shaderPath{ Base::FS::GetUserPath(Base::FS::PathType::ShaderDir) };
  bool gles = GetBackendID() == "GLES"_jLower;
  std::string versionString = FMT("#version {} {}\n", gles ? 310 : 430, gles ? "es" : "compatibility");
  shaderPath /= "opengl";
  computeShaderProgram = shaderFactory->LoadFromFiles("XeFbConvert", {
    { eShaderType::Compute, shaderPath / "fb_deswizzle.comp" }
  });
  if (!computeShaderProgram) {
    std::ofstream f{ shaderPath / "fb_deswizzle.comp" };
    f.write(versionString.data(), versionString.size());
    f.write(computeShaderSource, sizeof(computeShaderSource));
    f.close();
    computeShaderProgram = shaderFactory->LoadFromFiles("XeFbConvert", {
      { eShaderType::Compute, shaderPath / "fb_deswizzle.comp" }
    });
  }
  renderShaderPrograms = shaderFactory->LoadFromFiles("Render", {
    { eShaderType::Vertex, shaderPath / "framebuffer.vert" },
    { eShaderType::Fragment, shaderPath / "framebuffer.frag" }
  });
  if (!renderShaderPrograms) {
    std::ofstream vert{ shaderPath / "framebuffer.vert" };
    vert.write(versionString.data(), versionString.size());
    vert.write(vertexShaderSource, sizeof(vertexShaderSource));
    vert.close();
    std::ofstream frag{ shaderPath / "framebuffer.frag" };
    frag.write(versionString.data(), versionString.size());
    frag.write(fragmentShaderSource, sizeof(fragmentShaderSource));
    frag.close();
    renderShaderPrograms = shaderFactory->LoadFromFiles("Render", {
      { eShaderType::Vertex, shaderPath / "framebuffer.vert" },
      { eShaderType::Fragment, shaderPath / "framebuffer.frag" }
    });
  }

  // Create VAOs, and EBOs
  glGenVertexArrays(1, &dummyVAO);
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &EBO);

  // Set clear color
  glClearColor(0.f, 0.f, 0.f, 1.f);
  // Setup viewport
  glViewport(0, 0, width, height);
  // Disable unneeded things
  glDisable(GL_BLEND); // Xenos does not have alpha, and blending breaks anyways
  glDisable(GL_DEPTH_TEST);
}
void OGLRenderer::BackendSDLProperties(SDL_PropertiesID properties) {
  // Enable OpenGL
  SDL_SetNumberProperty(properties, "flags", SDL_WINDOW_OPENGL);
  SDL_SetBooleanProperty(properties, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
}
void OGLRenderer::BackendSDLInit() {
  // Set as a debug context 
  SANITY_CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG));
  // Set OpenGL SDL Properties
  SANITY_CHECK(SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1));
  SANITY_CHECK(SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24));
  SANITY_CHECK(SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8));
  SANITY_CHECK(SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1));
  // Set RGBA size (R8G8B8A8)
  SANITY_CHECK(SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8));
  SANITY_CHECK(SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8));
  SANITY_CHECK(SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8));
  SANITY_CHECK(SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8));
  // Set OpenGL version to 4.3 (earliest with CS)
  SANITY_CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4));
  SANITY_CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3));
  // Set as compat
  SANITY_CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE));
  // Create OpenGL handle for SDL
  context = SDL_GL_CreateContext(mainWindow);
  if (!context) {
    // Set GLES2 version to 3.1 (earliest with CS)
    SANITY_CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3));
    SANITY_CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1));
    // Set as ES
    SANITY_CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES));
    context = SDL_GL_CreateContext(mainWindow);
    if (!context) {
      LOG_ERROR(System, "Failed to create OpenGL context: {}", SDL_GetError());
    } else {
      gles = true;
      LOG_WARNING(System, "Using GLES, SPIR-V will not be avaliable");
    }
  }
  // Init GLAD
  if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
    LOG_ERROR(Render, "Failed to initialize GL: {}", SDL_GetError());
  } else {
    LOG_INFO(Render, "{} Version: {}", gles ? "GLES" : "OpenGL", OGLRenderer::GLVersion());
    LOG_INFO(Render, "OpenGL Vendor: {}", OGLRenderer::GLVendor());
    LOG_INFO(Render, "OpenGL Renderer: {}", OGLRenderer::GLRenderer());
  }
  if (gles) {
    if (!gladLoadGLES2Loader((GLADloadproc)SDL_GL_GetProcAddress)) {
      LOG_ERROR(Render, "Failed to initialize GLES2: {}", SDL_GetError());
    }
  }
  // Set VSYNC
  SANITY_CHECK(SDL_GL_SetSwapInterval(VSYNC));
  if (Config::rendering.debugValidation) {
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback([](GLenum source, GLenum type, GLuint id, GLenum severity,
      GLsizei length, const GLchar *message, const void *userParam) {
      LOG_INFO(Render, "GL: {}", message);
    }, nullptr);
  }
}

void OGLRenderer::BackendShutdown() {
  glDeleteVertexArrays(1, &dummyVAO);
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &EBO);
}
void OGLRenderer::BackendSDLShutdown() {
  SDL_GL_DestroyContext(context);
}

void OGLRenderer::BackendResize(s32 x, s32 y) {
  glViewport(0, 0, x, y);
}

void OGLRenderer::UpdateScissor(s32 x, s32 y, u32 width, u32 height) {
  glScissor(x, y, width, height);
}

void OGLRenderer::UpdateViewport(s32 x, s32 y, u32 width, u32 height) {
  glViewport(x, y, width, height);
}

void OGLRenderer::Clear() {
  glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
}

s32 ConvertToGLPrimitive(ePrimitiveType prim) {
  switch (prim) {
    case ePrimitiveType::xePointList: return GL_POINTS;
    case ePrimitiveType::xeLineList: return GL_LINES;
    case ePrimitiveType::xeLineStrip: return GL_LINE_STRIP;
    case ePrimitiveType::xeTriangleList: return GL_TRIANGLES;
    case ePrimitiveType::xeTriangleStrip: return GL_TRIANGLE_STRIP;
    default: return GL_TRIANGLES; // Fallback
  }
}

void OGLRenderer::VertexFetch(const u32 location, const u32 components, bool isFloat, bool isNormalized, const u32 fetchOffset, const u32 fetchStride) {
  const u32 type = isFloat ? GL_FLOAT:GL_UNSIGNED_INT;
  const u8 normalized = isNormalized ? GL_TRUE : GL_FALSE;
  if (location <= 32) {
    glEnableVertexAttribArray(location);
    glVertexAttribPointer(
      location,
      components,
      type,
      normalized,
      fetchStride,
      reinterpret_cast<void *>((u64)fetchOffset)
    );
  }
}

void OGLRenderer::Draw(Xe::XGPU::XeShader shader, Xe::XGPU::XeDrawParams params) {
  ePrimitiveType type = params.vgtDrawInitiator.primitiveType;
  s32 glPrimitive = ConvertToGLPrimitive(params.vgtDrawInitiator.primitiveType);
  u32 numIndices = params.vgtDrawInitiator.numIndices;
  // Bind the constants
  if (auto buffer = createdBuffers.find("FloatConsts"_jLower); buffer != createdBuffers.end())
    buffer->second->Bind(0);
  if (auto buffer = createdBuffers.find("CommonBoolConsts"_jLower); buffer != createdBuffers.end())
    buffer->second->Bind(1);
  // Bind the VAO
  glBindVertexArray(VAO);
  // Bind the VBO
  if (auto buffer = createdBuffers.find("VertexFetch"_jLower); buffer != createdBuffers.end())
    buffer->second->Bind();
  // Bind textures
  for (u32 i = 0; i != shader.textures.size(); ++i) {
    glActiveTexture(GL_TEXTURE0 + i);
    shader.textures[i]->Bind();
  }
  // Perform draw
  glDrawArrays(glPrimitive, 0, numIndices);
}

void OGLRenderer::DrawIndexed(Xe::XGPU::XeShader shader, Xe::XGPU::XeDrawParams params, Xe::XGPU::XeIndexBufferInfo indexBufferInfo) {
  ePrimitiveType type = params.vgtDrawInitiator.primitiveType;
  s32 glPrimitive = ConvertToGLPrimitive(params.vgtDrawInitiator.primitiveType);
  u32 numIndices = params.vgtDrawInitiator.numIndices;
  s32 indexType = indexBufferInfo.indexFormat == eIndexFormat::xeInt16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;

  const u32 destInfo = params.state->copyDestInfo.hexValue;
  const Xe::eEndianFormat endianFormat = static_cast<Xe::eEndianFormat>(destInfo & 7);
  const u32 destArray = (destInfo >> 3) & 1;
  const u32 destSlice = (destInfo >> 4) & 1;
  const eColorFormat destformat = static_cast<eColorFormat>((destInfo >> 7) & 0x3F);
  // Bind the constants
  if (auto buffer = createdBuffers.find("FloatConsts"_j); buffer != createdBuffers.end())
    buffer->second->Bind(0);
  if (auto buffer = createdBuffers.find("CommonBoolConsts"_j); buffer != createdBuffers.end())
    buffer->second->Bind(1);
  // Bind the shader
  if (shader.program)
    shader.program->Bind();
  // Bind the VAO
  glBindVertexArray(VAO);
  // Bind the VBO
  if (auto buffer = createdBuffers.find("VertexFetch"_jLower); buffer != createdBuffers.end())
    buffer->second->Bind();
  // Bind and upload index buffer (TODO: Fix this)
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBufferInfo.count, indexBufferInfo.elements, GL_STATIC_DRAW);
  // Bind textures
  for (u32 i = 0; i != shader.textures.size(); ++i) {
    glActiveTexture(GL_TEXTURE0 + i);
    shader.textures[i]->Bind();
  }
  // Perform draw
  glDrawElements(glPrimitive, numIndices, indexType, 0);
  // Unbind VAO
  glBindVertexArray(0);
}

void OGLRenderer::UpdateViewportFromState(const Xe::XGPU::XenosState *state) {
  auto f = [](u32 val) {
    f32 fval;
    memcpy(&fval, &val, sizeof(f32));
    return fval;
  };

  f32 xscale = f(state->viewportXScale);
  f32 xoffset = f(state->viewportXOffset);
  f32 yscale = f(state->viewportYScale);
  f32 yoffset = f(state->viewportYOffset);
  f32 zscale = f(state->viewportZScale);
  f32 zoffset = f(state->viewportZOffset);

  // Compute viewport rectangle
  s32 newWidth = static_cast<s32>(std::abs(xscale * 2));
  s32 newHeight = static_cast<s32>(std::abs(yscale * 2));
  s32 x = static_cast<s32>(xoffset - std::abs(xscale));
  s32 y = static_cast<s32>(yoffset - std::abs(yscale));

  if (newWidth != 32 && newHeight != 32) {
    Resize(newWidth, newHeight);
    glViewport(x, y, newWidth, newHeight);
  }

  // Clamp to valid ranges (just in case)
  f32 nearZ = std::max(0.f, std::min(1.f, zoffset));
  f32 farZ = std::max(nearZ, std::min(1.f, zoffset + zscale));

  glDepthRangef(nearZ, farZ);
}

void OGLRenderer::UpdateClearColor(u8 r, u8 b, u8 g, u8 a) {
  glClearColor((static_cast<f32>(r) / 255.f), (static_cast<f32>(g) / 255.f), (static_cast<f32>(b) / 255.f), (static_cast<f32>(a) / 255.f));
}

void OGLRenderer::UpdateClearDepth(f64 depth) {
  glClearDepth(depth);
}

void OGLRenderer::OnCompute() {
  glDispatchCompute(width / 16, height / 16, 1);
  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

void OGLRenderer::OnBind() {
  if (XeMain::xenos && XeMain::xenos->RenderingTo2DFramebuffer()) {
    // Bind VAO
    glBindVertexArray(dummyVAO);
    // Draw fullscreen triangle
    glDrawArrays(GL_TRIANGLE_FAN, 0, 3);
    // Unbind VAO
    glBindVertexArray(0);
  } else {
    // Bind the VAO
    glBindVertexArray(VAO);
  }
}

void OGLRenderer::OnSwap(SDL_Window *window) {
  SDL_GL_SwapWindow(window);
}

s32 OGLRenderer::GetBackbufferFlags() {
  // Set our texture flags & depth
  return eCreationFlags::glTextureWrapS_GL_CLAMP_TO_EDGE | eCreationFlags::glTextureWrapT_GL_CLAMP_TO_EDGE |
        eCreationFlags::glTextureMinFilter_GL_NEAREST | eCreationFlags::glTextureMagFilter_GL_NEAREST |
        eTextureDepth::R32U;
}

s32 OGLRenderer::GetXenosFlags() {
  // Set our texture flags & depth
  return eCreationFlags::glTextureWrapS_GL_CLAMP_TO_EDGE | eCreationFlags::glTextureWrapT_GL_CLAMP_TO_EDGE |
        eCreationFlags::glTextureMinFilter_GL_NEAREST | eCreationFlags::glTextureMagFilter_GL_NEAREST |
        eTextureDepth::R32U;
}

void* OGLRenderer::GetBackendContext() {
  return reinterpret_cast<void*>(context);
}

u32 OGLRenderer::GetBackendID() {
  return gles ? "GLES"_jLower : "OpenGL"_jLower;
}

} //namespace Render
#endif
