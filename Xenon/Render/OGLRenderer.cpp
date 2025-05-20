// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "OGLRenderer.h"

#include "OpenGL/Factory/OGLResourceFactory.h"
#include "OpenGL/OGLTexture.h"
#include "GUI/OpenGL.h"

#define SANITY_CHECK(x) if (!x) { LOG_ERROR(Xenon, "Failed to initialize SDL: {}", SDL_GetError()); }

#ifndef NO_GFX
namespace Render {

OGLRenderer::OGLRenderer(RAM *ram) :
  Renderer(ram)
{}

std::string OGLRenderer::gl_version() const {
  const GLubyte* version = glGetString(GL_VERSION);
  const char* c_version = reinterpret_cast<const char*>(version);

  return c_version;
}

std::string OGLRenderer::gl_vendor() const {
  const GLubyte* vendor = glGetString(GL_VENDOR);
  const char* c_vendor = reinterpret_cast<const char*>(vendor);

  return c_vendor;
}

std::string OGLRenderer::gl_renderer() const {
  const GLubyte* renderer = glGetString(GL_RENDERER);
  const char* c_renderer = reinterpret_cast<const char*>(renderer);

  return c_renderer;
}

void OGLRenderer::BackendStart() {
  // Create the resource factory
  resourceFactory = std::make_unique<OGLResourceFactory>();
  // Create a dummy VAO
  glGenVertexArrays(1, &dummyVAO);

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
  SANITY_CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY));
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
    LOG_INFO(Render, "{} Version: {}", gles ? "GLES" : "OpenGL", OGLRenderer::gl_version());
    LOG_INFO(Render, "OpenGL Vendor: {}", OGLRenderer::gl_vendor());
    LOG_INFO(Render, "OpenGL Renderer: {}", OGLRenderer::gl_renderer());
  }
  if (gles) {
    if (!gladLoadGLES2Loader((GLADloadproc)SDL_GL_GetProcAddress)) {
      LOG_ERROR(Render, "Failed to initialize GLES2: {}", SDL_GetError());
    }
  }
  // Set VSYNC
  SANITY_CHECK(SDL_GL_SetSwapInterval(VSYNC));
}

void OGLRenderer::BackendShutdown() {
  glDeleteVertexArrays(1, &dummyVAO);
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

void OGLRenderer::Draw(Xe::XGPU::XeDrawParams params) {
  // TODO: Draw
  ePrimitiveType type = params.vgtDrawInitiator.primitiveType;
  s32 glPrimitive = ConvertToGLPrimitive(params.vgtDrawInitiator.primitiveType);
  u32 numIndices = params.vgtDrawInitiator.numIndices;
  for (u32 i = 0; i != params.shader.textures.size(); ++i) {
    glActiveTexture(GL_TEXTURE0 + i);
    params.shader.textures[i]->Bind();
    //glBindSampler(i, params.shader.textures[i]->Sampler);
  }
  glDrawArrays(glPrimitive, 0, params.vgtDrawInitiator.numIndices);
}

void OGLRenderer::DrawIndexed(Xe::XGPU::XeDrawParams params, Xe::XGPU::XeIndexBufferInfo indexBufferInfo) {
  // TODO: Draw
  ePrimitiveType type = params.vgtDrawInitiator.primitiveType;
  s32 glPrimitive = ConvertToGLPrimitive(params.vgtDrawInitiator.primitiveType);
  u32 numIndices = params.vgtDrawInitiator.numIndices;
  s32 indexType = indexBufferInfo.indexFormat == eIndexFormat::xeInt16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
  const u32 destInfo = params.state->copyDestInfo;
  const Xe::eEndianFormat endianFormat = static_cast<Xe::eEndianFormat>(destInfo & 7);
  const u32 destArray = (destInfo >> 3) & 1;
  const u32 destSlice = (destInfo >> 4) & 1;
  const Xe::eColorFormat destformat = static_cast<Xe::eColorFormat>((destInfo >> 7) & 0x3F);
  for (u32 i = 0; i != params.shader.textures.size(); ++i) {
    glActiveTexture(GL_TEXTURE0 + i);
    params.shader.textures[i]->Bind();
    //glBindSampler(i, params.shader.textures[i]->Sampler);
  }
  if (params.shader.vertexShader) {
    for (const auto *fetch : params.shader.vertexShader->vertexFetches) {
      u32 slot = fetch->fetchSlot;
      u32 offset = fetch->fetchOffset;

      // Bind vertex buffer
      u32 stride = params.state->vertexData.size;
      const u8 *basePtr = params.vertexBufferPtr + params.minVertexIndex * stride + offset;

      // Assume format is float4
      glEnableVertexAttribArray(slot);
      glVertexAttribPointer(slot, 4, GL_FLOAT, GL_FALSE, stride, (const void*)basePtr);
    }
  }
  glDrawElements(glPrimitive, numIndices, indexType, (void*)indexBufferInfo.elements);
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
  s32 x = static_cast<s32>(xoffset - xscale);
  s32 y = static_cast<s32>(yoffset - yscale);
  s32 width = static_cast<s32>(xscale * 2);
  s32 height = static_cast<s32>(yscale * 2);

  glViewport(x, y, width, height);

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
  glBindVertexArray(dummyVAO);
  glDrawArrays(GL_TRIANGLE_FAN, 0, 3);
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

void* OGLRenderer::GetBackendContext() {
  return reinterpret_cast<void*>(context);
}

u32 OGLRenderer::GetBackendID() {
  return gles ? "GLES"_j : "OpenGL"_j;
}

} //namespace Render
#endif
