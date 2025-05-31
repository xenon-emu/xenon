// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include <fstream>

#include <glad/glad.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>

#include "Base/Logging/Log.h"
#include "Base/CRCHash.h"
#include "Base/Param.h"
#include "Base/Hash.h"

#include "Core/XGPU/Microcode/ASTBlock.h"
#include "Core/XGPU/Microcode/ASTNodeWriter.h"
#include "Core/XGPU/ShaderConstants.h"
#include "Core/XGPU/Xenos.h"

#include "Render/Abstractions/Factory/ResourceFactory.h"
#include "Render/Abstractions/Factory/ShaderFactory.h"
#include "Render/OpenGL/Factory/OGLResourceFactory.h"
#include "Render/OpenGL/Factory/OGLShaderFactory.h"
#include "Render/Abstractions/Texture.h"

struct XeShader {
  u32 vertexShaderHash = 0;
  Xe::Microcode::AST::Shader *vertexShader = nullptr;
  u32 pixelShaderHash = 0;
  Xe::Microcode::AST::Shader * pixelShader = nullptr;
  std::vector<std::shared_ptr<Render::Texture>> textures{};
  std::shared_ptr<Render::Shader> program = {};
};

struct XeShaderFloatConsts {
  f32 values[256 * 4] = {};
};

struct XeShaderBoolConsts {
  u32 values[8 * 4] = {};
};

#define TILE(x) ((x + 31) >> 5) << 5
s32 width = TILE(1280), height = TILE(720);

SDL_Window *mainWindow = nullptr;
SDL_Event windowEvent = {};
SDL_WindowID windowID = {};
SDL_GLContext context;

std::unique_ptr<Render::ResourceFactory> resourceFactory{};
std::unique_ptr<Render::ShaderFactory> shaderFactory{};

namespace Xe::Microcode {

u32 pixelShaderHash = 0;
u32 vertexShaderHash = 0;
std::unordered_map<u32, std::pair<std::vector<u32>, AST::Shader*>> shaders{};
std::unordered_map<u64, XeShader> linkedShaderPrograms{};
std::unordered_map<u32, std::shared_ptr<Render::Buffer>> createdBuffers{};

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
  AST::Shader *vertexShader = shaders[vertexShaderHash].second;
  AST::Shader *pixelShader = shaders[pixelShaderHash].second;
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
  fs::path shaderPath{ "shaders" };
  std::string fileName = std::format("vertex_shader_{:X}.bin", hash);
  std::ifstream f{ shaderPath / fileName, std::ios::in | std::ios::binary };
  if (!f.is_open()) {
    shaderType = eShaderType::Pixel;
    fileName = std::format("pixel_shader_{:X}.bin", hash);
    f.open(shaderPath / fileName, std::ios::in | std::ios::binary);
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
  Microcode::AST::ShaderCodeWriterSirit writer{ shaderType, shader };
  if (shader) {
    shader->EmitShaderCode(writer);
  }
  std::vector<u32> code = writer.module.Assemble();
  std::string fileNameSpv = std::format("{}_shader_{:X}.spv", shaderType == eShaderType::Vertex ? "vertex" : "pixel", hash);
  std::ofstream fo{ shaderPath / fileNameSpv, std::ios::out | std::ios::binary };
  fo.write(reinterpret_cast<char*>(code.data()), code.size() * sizeof(*code.data()));
  fo.close();
  shaders.insert({ hash, { code, shader } });
}

void CreateShader() {
  auto pixelShader = shaders[pixelShaderHash];
  auto vertexShader = shaders[vertexShaderHash];
  u64 combinedHash = (static_cast<u64>(vertexShaderHash) << 32) | pixelShaderHash;
  std::shared_ptr<Render::Shader> shader = shaderFactory->LoadFromBinary(std::format("psvs_0x{:X}", combinedHash), {
    { Render::eShaderType::Vertex, vertexShader.first },
    { Render::eShaderType::Fragment, pixelShader.first }
  });
  XeShader xeShader{};
  xeShader.program = std::move(shader);
  xeShader.pixelShader = vertexShader.second;
  xeShader.pixelShaderHash = pixelShaderHash;
  xeShader.vertexShaderHash = vertexShaderHash;
  xeShader.vertexShader = vertexShader.second;
  if (xeShader.textures.empty()) {
    for (u64 i = 0; i != xeShader.pixelShader->usedTextures.size(); ++i) {
      xeShader.textures.push_back(resourceFactory->CreateTexture());
    }
    for (u64 i = 0; i != xeShader.vertexShader->usedTextures.size(); ++i) {
      xeShader.textures.push_back(resourceFactory->CreateTexture());
    }
    for (auto &texture : xeShader.textures) {
      texture->CreateTextureHandle(width, height, Render::eCreationFlags::glTextureWrapS_GL_CLAMP_TO_EDGE | Render::eCreationFlags::glTextureWrapT_GL_CLAMP_TO_EDGE |
        Render::eCreationFlags::glTextureMinFilter_GL_NEAREST | Render::eCreationFlags::glTextureMagFilter_GL_NEAREST |
        Render::eTextureDepth::R32U);
    }
  }
  linkedShaderPrograms.insert({ combinedHash, xeShader });
}

void CreateBuffers(XeShaderFloatConsts psConsts, XeShaderFloatConsts vsConsts, XeShaderBoolConsts boolConsts) {
  auto vsData = std::vector<u8>(
    reinterpret_cast<u8*>(&vsConsts),
    reinterpret_cast<u8*>(&vsConsts) + sizeof(vsConsts)
  );
  std::shared_ptr<Render::Buffer> vsBuffer = resourceFactory->CreateBuffer();
  vsBuffer->CreateBuffer(static_cast<u32>(vsData.size()), vsData.data(), Render::eBufferUsage::DynamicDraw, Render::eBufferType::Storage);
  auto psData = std::vector<u8>(
    reinterpret_cast<u8*>(&psConsts),
    reinterpret_cast<u8*>(&psConsts) + sizeof(psConsts)
  );
  std::shared_ptr<Render::Buffer> psBuffer = resourceFactory->CreateBuffer();
  psBuffer->CreateBuffer(static_cast<u32>(psData.size()), psData.data(), Render::eBufferUsage::DynamicDraw, Render::eBufferType::Storage);
  auto boolData = std::vector<u8>(
    reinterpret_cast<u8*>(&psConsts),
    reinterpret_cast<u8*>(&psConsts) + sizeof(psConsts)
  );
  std::shared_ptr<Render::Buffer> boolBuffer = resourceFactory->CreateBuffer();
  boolBuffer->CreateBuffer(static_cast<u32>(boolData.size()), boolData.data(), Render::eBufferUsage::DynamicDraw, Render::eBufferType::Storage);
  createdBuffers.insert({ "VertexConsts"_j, vsBuffer });
  createdBuffers.insert({ "PixelConsts"_j, psBuffer });
  createdBuffers.insert({ "CommonBoolConsts"_j, boolBuffer });
}

} // namespace Xe::Microcode

std::string gl_version() {
  const GLubyte *version = glGetString(GL_VERSION);
  return reinterpret_cast<const char*>(version);
}
std::string gl_vendor() {
  const GLubyte *vendor = glGetString(GL_VENDOR);
  return reinterpret_cast<const char*>(vendor);
}
std::string gl_renderer() {
  const GLubyte *renderer = glGetString(GL_RENDERER);
  return reinterpret_cast<const char*>(renderer);
}

void CreateWindow() {
  // Init SDL Events, Video, Joystick, and Gamepad
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
    LOG_ERROR(Xenon, "Failed to initialize SDL: {}", SDL_GetError());
  }

  // SDL3 window properties.
  SDL_PropertiesID props = SDL_CreateProperties();
  const std::string title = "AST Tests";
  SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title.c_str());
  // Set starting X and Y position to be centered
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);
  // Set width and height
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, 1280);
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, 720);
  // Allow resizing
  SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
  // Enable HiDPI
  SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
  SDL_SetNumberProperty(props, "flags", SDL_WINDOW_OPENGL);
  SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
  // Create window
  mainWindow = SDL_CreateWindowWithProperties(props);

  // Set min size
  SDL_SetWindowMinimumSize(mainWindow, 640, 480);

  if (!mainWindow) {
    LOG_ERROR(Render, "Failed to create window: {}", SDL_GetError());
  }

  // Destroy (no longer used) properties
  SDL_DestroyProperties(props);
  // Set if we are in fullscreen mode or not
  SDL_SetWindowFullscreen(mainWindow, false);
  // Get current window ID
  windowID = SDL_GetWindowID(mainWindow);
}

#define SANITY_CHECK(x) if (!x) { LOG_ERROR(Xenon, "Failed to initialize SDL: {}", SDL_GetError()); }
void InitOpenGL() {
  resourceFactory = std::make_unique<Render::OGLResourceFactory>();
  shaderFactory = resourceFactory->CreateShaderFactory();
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
  SANITY_CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY));
  // Create OpenGL handle for SDL
  context = SDL_GL_CreateContext(mainWindow);
  if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
    LOG_ERROR(Render, "Failed to initialize GL: {}", SDL_GetError());
  }
  else {
    LOG_INFO(Render, "{} Version: {}", "OpenGL", gl_version());
    LOG_INFO(Render, "OpenGL Vendor: {}", gl_vendor());
    LOG_INFO(Render, "OpenGL Renderer: {}", gl_renderer());
  }
  SANITY_CHECK(SDL_GL_SetSwapInterval(true));

  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback([](GLenum source, GLenum type, GLuint id, GLenum severity,
    GLsizei length, const GLchar *message, const void *userParam) {
    LOG_INFO(Render, "GL: {}", message);
  }, nullptr);
  glViewport(0, 0, width, height);
  glClearColor(0.1f, 0.1f, 0.1f, 1.f);
}

void Resize(s32 x, s32 y) {
  // Normalize our x and y for tiling
  width = TILE(x);
  height = TILE(y);

  LOG_DEBUG(Render, "Resized window to {}x{}", width, height);
}

void Clear() {
  glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
}

void HandleEvents() {
  // Process events.
  while (XeRunning && SDL_PollEvent(&windowEvent)) {
    switch (windowEvent.type) {
      case SDL_EVENT_WINDOW_RESIZED:
        if (windowEvent.window.windowID == windowID) {
          LOG_DEBUG(Render, "Resizing window...");
          Resize(windowEvent.window.data1, windowEvent.window.data2);
        }
        break;
      case SDL_EVENT_QUIT:
        XeRunning = false;
        break;
      case SDL_EVENT_KEY_DOWN:
        if (windowEvent.key.key == SDLK_F11) {
          SDL_WindowFlags flag = SDL_GetWindowFlags(mainWindow);
          bool fullscreenMode = flag & SDL_WINDOW_FULLSCREEN;
          SDL_SetWindowFullscreen(mainWindow, !fullscreenMode);
        }
        break;
      default:
        break;
    }
  }
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

u32 VAO = 0;
u32 VBO = 0;
void CreateVAOAndVBOFromShader(const Xe::Microcode::AST::Shader *vertexShader, u32 &vaoOut, u32 &vboOut) {
  if (!vertexShader)
    return;

  struct VertexElement {
    u32 slot;
    u32 components;
    Xe::Microcode::AST::VertexFetchResultType type;
  };

  std::map<u32, VertexElement> attributes;

  // Collect unique fetches by slot
  for (const auto *fetch : vertexShader->vertexFetches) {
    if (!attributes.contains(fetch->fetchSlot)) {
      attributes[fetch->fetchSlot] = {
        .slot = fetch->fetchSlot,
        .components = fetch->GetComponentCount(),
        .type = fetch->GetResultType()
      };
    }
  }
  std::unordered_map<u32, u32> fetchSlotToLocation;
  u32 currentLocation = 0;
  for (const auto &[slot, _] : attributes) {
    if (currentLocation >= 32) {
      LOG_ERROR(Xenos, "Too many vertex input attributes! Max supported is 32.");
      return;
    }
    fetchSlotToLocation[slot] = currentLocation++;
  }

  if (attributes.empty()) {
    LOG_ERROR(Xenos, "No vertex fetches in shader");
    return;
  }

  // Example: 3 vertices forming a triangle
  std::vector<float> vertexData = {
    // Vertex 0
    0.0f, 0.5f, 0.0f, 1.0f,
    // Vertex 1
   -0.5f, -0.5f, 0.0f, 1.0f,
   // Vertex 2
   0.5f, -0.5f, 0.0f, 1.0f
  };

  glGenVertexArrays(1, &vaoOut);
  glBindVertexArray(vaoOut);

  glGenBuffers(1, &vboOut);
  glBindBuffer(GL_ARRAY_BUFFER, vboOut);
  glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_STATIC_DRAW);

  u32 stride = 0;
  for (const auto &[slot, attr] : attributes)
    stride += attr.components * sizeof(float);

  u32 offset = 0;
  for (const auto &[slot, attr] : attributes) {
    GLenum type = GL_FLOAT;
    if (attr.type == Xe::Microcode::AST::VertexFetchResultType::Int)
      type = GL_INT;
    else if (attr.type == Xe::Microcode::AST::VertexFetchResultType::UInt)
      type = GL_UNSIGNED_INT;

    u32 glLocation = fetchSlotToLocation[slot];
    glEnableVertexAttribArray(glLocation);
    glVertexAttribPointer(glLocation, attr.components, type, GL_FALSE, stride, (void*)(u64)offset);

    offset += attr.components * sizeof(float);
  }

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void RenderFrame() {
  Clear();
  u64 combinedHash = (static_cast<u64>(Xe::Microcode::vertexShaderHash) << 32) | Xe::Microcode::pixelShaderHash;
  auto shader = Xe::Microcode::linkedShaderPrograms[combinedHash];
  if (shader.vertexShader) {
    if (auto buffer = Xe::Microcode::createdBuffers.find("VertexConsts"_j); buffer != Xe::Microcode::createdBuffers.end())
      buffer->second->Bind(0);
  }
  if (shader.pixelShader) {
    if (auto buffer = Xe::Microcode::createdBuffers.find("PixelConsts"_j); buffer != Xe::Microcode::createdBuffers.end())
      buffer->second->Bind(2);
  }
  if (auto buffer = Xe::Microcode::createdBuffers.find("CommonBoolConsts"_j); buffer != Xe::Microcode::createdBuffers.end())
    buffer->second->Bind(1);
  if (shader.program)
    shader.program->Bind();
  glBindVertexArray(VAO);
  // Bind textures
  for (u32 i = 0; i != shader.textures.size(); ++i) {
    glActiveTexture(GL_TEXTURE0 + i);
    shader.textures[i]->Bind();
  }
  s32 glPrimitive = ConvertToGLPrimitive(ePrimitiveType::xeTriangleList);
  glDrawArrays(glPrimitive, 0, 3);
  SDL_GL_SwapWindow(mainWindow);
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
    // Vertex: 0x8825F002
    // Pixel: 0xDE9ADE64
    crcHash = 0x8825F002;
  }
  Xe::Microcode::pixelShaderHash = 0xDE9ADE64;
  Xe::Microcode::vertexShaderHash = 0x8825F002;
  Xe::Microcode::Run(Xe::Microcode::pixelShaderHash);
  Xe::Microcode::Run(Xe::Microcode::vertexShaderHash);
  Xe::Microcode::Handle();
  XeShaderFloatConsts vsConsts = {};
  XeShaderFloatConsts psConsts = {};
  XeShaderBoolConsts boolConsts = {};
  // Unused
  memset(boolConsts.values, 0, sizeof(boolConsts.values));
  // VertexConsts[0]
  float rows[3][4] = {
    { -1.f, -1.f, 1.f, 1.f },
    {  0.f,  1.f, 1.f, 1.f },
    {  1.f, -1.f, 1.f, 1.f }
  };
  vsConsts.values[0] =  rows[0][0];
  vsConsts.values[1] =  rows[0][1];
  vsConsts.values[2] =  rows[0][2];
  vsConsts.values[3] =  rows[0][3];
  vsConsts.values[4] =  rows[1][0];
  vsConsts.values[5] =  rows[1][1];
  vsConsts.values[6] =  rows[1][2];
  vsConsts.values[7] =  rows[1][3];
  vsConsts.values[8] =  rows[2][0];
  vsConsts.values[9] =  rows[2][1];
  vsConsts.values[10] = rows[2][2];
  vsConsts.values[11] = rows[2][3];

  // PixelConsts[0] = red
  psConsts.values[0] = 1.0f;
  psConsts.values[1] = 0.0f;
  psConsts.values[2] = 0.0f;
  psConsts.values[3] = 1.0f;

  LOG_INFO(Xenos, "VS[0]: x = {}, y = {}, z = {}, w = {}", vsConsts.values[0], vsConsts.values[1], vsConsts.values[2], vsConsts.values[3]);

  CreateWindow();
  InitOpenGL();
  Xe::Microcode::CreateShader();
  Xe::Microcode::CreateBuffers(psConsts, vsConsts, boolConsts);
  u64 combinedHash = (static_cast<u64>(Xe::Microcode::vertexShaderHash) << 32) | Xe::Microcode::pixelShaderHash;
  auto shader = Xe::Microcode::linkedShaderPrograms[combinedHash];
  CreateVAOAndVBOFromShader(shader.vertexShader, VAO, VBO);
  while (XeRunning) {
    HandleEvents();
    RenderFrame();
  }
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  shaderFactory->Destroy();
  shaderFactory.reset();
  resourceFactory.reset();
  SDL_GL_DestroyContext(context);
  SDL_DestroyWindow(mainWindow);
  SDL_Quit();
  return 0;
}