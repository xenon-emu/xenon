// Copyright 2025 Xenon Emulator Project

#include "OGLRenderer.h"

#include "Base/Version.h"

#include "OpenGL/Factory/OGLResourceFactory.h"
#include "GUI/OpenGL.h"

#ifndef NO_GFX
namespace Render {

OGLRenderer::OGLRenderer(RAM *ram, SDL_Window *mainWindow) :
  Renderer(ram, mainWindow)
{}

OGLRenderer::~OGLRenderer() {
  Shutdown();
}

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
  // Set min size
  SDL_SetWindowMinimumSize(mainWindow, 640, 480);
  // Set OpenGL SDL Properties
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
  // Set RGBA size (R8G8B8A8)
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  // Set OpenGL version to 4.3 (earliest with CS)
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  // We aren't using compatibility profile
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  // Create OpenGL handle for SDL
  context = SDL_GL_CreateContext(mainWindow);
  if (!context) {
    LOG_ERROR(System, "Failed to create OpenGL context: {}", SDL_GetError());
  }
  // Init GLAD
  if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
    LOG_ERROR(Render, "Failed to initialize OpenGL Loader");
  } else {
    LOG_INFO(Render, "OpenGL Version: {}", OGLRenderer::gl_version());
    LOG_INFO(Render, "OpenGL Vendor: {}", OGLRenderer::gl_vendor());
    LOG_INFO(Render, "OpenGL Renderer: {}", OGLRenderer::gl_renderer());
  }
  // Set VSYNC
  SDL_GL_SetSwapInterval(VSYNC);
  // Set if we are in fullscreen mode or not
  SDL_SetWindowFullscreen(mainWindow, fullscreen);
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

} //namespace Render
#endif
