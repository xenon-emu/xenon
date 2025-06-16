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
#include <Render/OpenGL/OGLShader.h>

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

// Vertex Shader (GLSL)
const char *vertexShaderSource = R"glsl(
#version 450 core
layout(location = 0) in vec4 aPos;
void main() {
  gl_Position = vec4(aPos);
}
)glsl";

// Fragment Shader (GLSL)
const char *fragmentShaderSource = R"glsl(
#version 450 core
out vec4 FragColor;
void main() {
  FragColor = vec4(1.0, 0.0, 0.0, 1.0); // Red
}
)glsl";

XeShaderFloatConsts vsConsts = {};
XeShaderFloatConsts psConsts = {};
XeShaderBoolConsts boolConsts = {};

namespace Xe::Microcode {

u32 pixelShaderHash = 0;
u32 vertexShaderHash = 0;
std::unordered_map<u32, std::pair<std::vector<u32>, AST::Shader*>> shaders{};
std::unordered_map<u64, XeShader> linkedShaderPrograms{};
std::unordered_map<u32, std::shared_ptr<Render::Buffer>> createdBuffers{};

void Write(u32 hash, eShaderType shaderType, std::vector<u32> code) {
  fs::path shaderPath{ "shaders" };
  std::string typeString = shaderType == Xe::eShaderType::Pixel ? "pixel" : "vertex";
  std::string baseString = std::format("{}_shader_{:X}", typeString, hash);
  {
    std::ofstream f{ shaderPath / (baseString + ".spv"), std::ios::out | std::ios::binary };
    f.write(reinterpret_cast<char *>(code.data()), code.size() * 4);
    f.close();
  }
}

void Handle() {
  auto vertexShader = shaders[vertexShaderHash];
  auto pixelShader = shaders[pixelShaderHash];
  Write(vertexShaderHash, eShaderType::Vertex, vertexShader.first);
  Write(pixelShaderHash, eShaderType::Pixel, pixelShader.first);
}

void Run(u32 hash) {
  eShaderType shaderType = eShaderType::Unknown;
  fs::path shaderPath{ fs::current_path() / "shaders" };
  LOG_INFO(Base, "Shader path: {}", shaderPath.string());
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
  Microcode::AST::ShaderCodeWriterSirit writer{ shaderType };
  if (shader) {
    shader->EmitShaderCode(writer);
  }
  std::vector<u32> code = writer.module.Assemble();
  shaders.insert({ hash, { code, shader } });
}

std::vector<u32> CreateVertexShader() {
  Sirit::Module module;
  // Types
  auto void_type = module.TypeVoid();
  auto func_type = module.TypeFunction(void_type);
  auto float_type = module.TypeFloat(32);
  auto vec4_type = module.TypeVector(float_type, 4);

  // Input and output
  auto input_ptr = module.TypePointer(spv::StorageClass::Input, vec4_type);
  auto output_ptr = module.TypePointer(spv::StorageClass::Output, vec4_type);

  // Variables
  auto aPos = module.AddGlobalVariable(input_ptr, spv::StorageClass::Input);
  module.Name(aPos, "aPos");
  module.Decorate(aPos, spv::Decoration::Location, 0);

  auto gl_Position = module.AddGlobalVariable(output_ptr, spv::StorageClass::Output);
  module.Name(gl_Position, "gl_Position");
  module.Decorate(gl_Position, spv::Decoration::BuiltIn, spv::BuiltIn::Position);

  // Function
  auto func = module.OpFunction(void_type, spv::FunctionControlMask::MaskNone, func_type);
  module.Name(func, "main");
  module.AddEntryPoint(spv::ExecutionModel::Vertex, func, "main", aPos, gl_Position);

  auto label = module.AddLabel();

  // Load input
  auto pos_val = module.OpLoad(vec4_type, aPos);

  // Write to gl_Position
  module.OpStore(gl_Position, pos_val);

  // Return
  module.OpReturn();
  module.OpFunctionEnd();
  return module.Assemble();
}

void CreateShader() {
  auto pixelShader = shaders[pixelShaderHash];
  auto vertexShader = shaders[vertexShaderHash];
  u64 combinedHash = (static_cast<u64>(vertexShaderHash) << 32) | pixelShaderHash;

  auto shader = std::make_shared<Render::OGLShader>();
  auto vertexShaderBinary = CreateVertexShader();
  auto fragmentShaderBinary = pixelShader.first;
  Write(vertexShaderHash, eShaderType::Vertex, vertexShaderBinary);
  Write(pixelShaderHash, eShaderType::Pixel, fragmentShaderBinary);
  shader->CompileFromBinary(Render::eShaderType::Vertex, (u8*)vertexShaderBinary.data(), vertexShaderBinary.size() * 4);
  shader->CompileFromBinary(Render::eShaderType::Fragment, (u8*)fragmentShaderBinary.data(), fragmentShaderBinary.size() * 4);
  //shader->CompileFromSource(Render::eShaderType::Fragment, fragmentShaderSource);
  shader->Link();
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
  const u32 vertexFetchStart = 95 * 4;
  const u32 vertexFetchFloatCount = (256 - 95) * 4;
  auto vsData = std::vector<u8>(
    reinterpret_cast<u8 *>(&vsConsts.values[vertexFetchStart]),
    reinterpret_cast<u8 *>(&vsConsts.values[vertexFetchStart + vertexFetchFloatCount])
  );
  std::shared_ptr<Render::Buffer> vsBuffer = resourceFactory->CreateBuffer();
  vsBuffer->CreateBuffer(static_cast<u32>(vsData.size()), vsData.data(), Render::eBufferUsage::DynamicDraw, Render::eBufferType::Storage);
  auto psData = std::vector<u8>(
    reinterpret_cast<u8 *>(psConsts.values),
    reinterpret_cast<u8 *>(psConsts.values) + sizeof(vsConsts)
  );
  std::shared_ptr<Render::Buffer> psBuffer = resourceFactory->CreateBuffer();
  psBuffer->CreateBuffer(static_cast<u32>(psData.size()), psData.data(), Render::eBufferUsage::DynamicDraw, Render::eBufferType::Storage);
  auto boolData = std::vector<u8>(
    reinterpret_cast<u8*>(boolConsts.values),
    reinterpret_cast<u8*>(boolConsts.values) + sizeof(boolConsts)
  );
  std::shared_ptr<Render::Buffer> boolBuffer = resourceFactory->CreateBuffer();
  boolBuffer->CreateBuffer(static_cast<u32>(boolData.size()), boolData.data(), Render::eBufferUsage::DynamicDraw, Render::eBufferType::Storage);
  createdBuffers.insert({ "VertexConsts"_jLower, vsBuffer });
  createdBuffers.insert({ "PixelConsts"_jLower, psBuffer });
  createdBuffers.insert({ "CommonBoolConsts"_jLower, boolBuffer });
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
bool LoadConstantsFromFile(XeShaderFloatConsts &vsConsts, XeShaderFloatConsts &psConsts, XeShaderBoolConsts &boolConsts) {
  std::ifstream in("shader_consts_dump.bin", std::ios::binary);
  if (!in.is_open()) {
    std::cerr << "Failed to open consts dump file for reading.\n";
    return false;
  }

  in.read(reinterpret_cast<char*>(vsConsts.values), sizeof(vsConsts.values));
  in.read(reinterpret_cast<char*>(psConsts.values), sizeof(psConsts.values));
  in.read(reinterpret_cast<char*>(boolConsts.values), sizeof(boolConsts.values));

  in.close();
  return true;
}
void CreateVAOAndVBOFromShader(const Xe::Microcode::AST::Shader *vertexShader, u32 &vaoOut, u32 &vboOut) {
  // Always generate a VAO (even if we ultimately bind no real attributes)
  glGenVertexArrays(1, &vaoOut);
  glBindVertexArray(vaoOut);
  glGenBuffers(1, &vboOut);
  glBindBuffer(GL_ARRAY_BUFFER, vboOut);

  // If there are no fetches, fall back to a dummy fullscreen triangle
  if (!vertexShader || vertexShader->vertexFetches.empty()) {
    LOG_WARNING(Xenos, "No vertex fetches in shader — using dummy fullscreen triangle.");

    // We still need to bind SOME VBO, because GL_VALIDATE might complain otherwise.
    // Create a tiny 3-vertex buffer of {0,0,0}, {0,0,0}, {0,0,0}, then draw(3).
    std::vector<f32> dummy = {
      0.f, 0.f, 0.f,
      0.f, 0.f, 0.f,
      0.f, 0.f, 0.f
    };
    glBufferData(GL_ARRAY_BUFFER, dummy.size() * sizeof(f32), dummy.data(),  GL_STATIC_DRAW);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return;
  }

  struct VertexElement {
    u32 slot;
    u32 components;
    bool isFloat;
    bool normalized;
    u32 offset;
    u32 stride;
  };

  std::map<u32, VertexElement> attributes;
  for (auto const *fetch : vertexShader->vertexFetches) {
    VertexElement ve;
    ve.slot = fetch->fetchSlot;
    ve.components = fetch->GetComponentCount();
    ve.isFloat = fetch->isFloat;
    ve.normalized = fetch->isNormalized;
    ve.offset = fetch->fetchOffset;
    ve.stride = fetch->fetchStride;
    attributes[ve.slot] = ve;
  }

  const f32 *vertexDataPtr = &vsConsts.values[95 * 4];
  const u32 vertexDataSize = (256 - 95) * 4;

  std::vector<f32> vertexData(vertexDataPtr, vertexDataPtr + vertexDataSize);

  glBindBuffer(GL_ARRAY_BUFFER, vboOut);
  glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(f32), vertexData.data(), GL_STATIC_DRAW);

  u64 offset = 0;
  for (auto const &kv : attributes) {
    const VertexElement &ve = kv.second;
    u32 glLocation = ve.slot - 95;
    u32 glType = ve.isFloat ? GL_FLOAT : GL_UNSIGNED_INT;
    glEnableVertexAttribArray(glLocation);
    glVertexAttribPointer(glLocation, ve.components, glType, ve.normalized ? GL_TRUE : GL_FALSE, glLocation, reinterpret_cast<void*>(static_cast<u64>(offset)));
    offset += ve.components * sizeof(f32);
  }

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void RenderFrame() {
  Clear();
  u64 combinedHash = (static_cast<u64>(Xe::Microcode::vertexShaderHash) << 32) | Xe::Microcode::pixelShaderHash;
  auto shader = Xe::Microcode::linkedShaderPrograms[combinedHash];
  if (shader.vertexShader) {
    if (auto buffer = Xe::Microcode::createdBuffers.find("VertexConsts"_jLower); buffer != Xe::Microcode::createdBuffers.end())
      buffer->second->Bind(0);
  }
  if (shader.pixelShader) {
    if (auto buffer = Xe::Microcode::createdBuffers.find("PixelConsts"_jLower); buffer != Xe::Microcode::createdBuffers.end())
      buffer->second->Bind(2);
  }
  if (auto buffer = Xe::Microcode::createdBuffers.find("CommonBoolConsts"_jLower); buffer != Xe::Microcode::createdBuffers.end())
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
  Xe::Microcode::pixelShaderHash = 0x3D0F8ECE;
  Xe::Microcode::vertexShaderHash = 0x490721B7;
  Xe::Microcode::Run(Xe::Microcode::pixelShaderHash);
  Xe::Microcode::Run(Xe::Microcode::vertexShaderHash);
  Xe::Microcode::Handle();
  //// Unused
  //memset(boolConsts.values, 0, sizeof(boolConsts.values));
  //// VertexConsts[0]
  //float rows[3][4] = {
  //  { -1.f, -1.f, 1.f, 1.f },
  //  {  0.f,  1.f, 1.f, 1.f },
  //  {  1.f, -1.f, 1.f, 1.f }
  //};

  //float matMV[16];
  //float matP[16];
  //// Identity modelview matrix
  //memset(matMV, 0, sizeof(matMV));
  //matMV[0] = 1.0f;
  //matMV[5] = 1.0f;
  //matMV[10] = 1.0f;
  //matMV[15] = 1.0f;

  //// Translate Z back a bit
  //matMV[14] = -5.0f;

  //// Simple perspective projection matrix
  //float fov = 60.0f * (3.1415926535f / 180.0f);
  //float aspect = 640.0f / 480.0f;
  //float znear = 0.1f, zfar = 100.0f;
  //float f = 1.0f / tanf(fov / 2.0f);

  //memset(matP, 0, sizeof(matP));
  //matP[0] = f / aspect;
  //matP[5] = f;
  //matP[10] = (zfar + znear) / (znear - zfar);
  //matP[11] = -1.0f;
  //matP[14] = (2.0f * zfar * znear) / (znear - zfar);

  //memcpy(&vsConsts.values[0], matP, sizeof(matP));        // c0–c3
  //memcpy(&vsConsts.values[16], matMV, sizeof(matMV));     // c4–c7

  //memcpy(&vsConsts.values[95 * 4], rows, sizeof(rows));

  //// PixelConsts[0] = red
  //for (int i = 0; i != (3 * 4); i += 4) {
  //  psConsts.values[i + 0] = 0.0f;
  //  psConsts.values[i + 1] = 1.0f;
  //  psConsts.values[i + 2] = 0.0f;
  //  psConsts.values[i + 3] = 1.0f;
  //}

  //LOG_INFO(Xenos, "VS[95]: x = {}, y = {}, z = {}, w = {}", vsConsts.values[(95*4)+0], vsConsts.values[(95*4)+1], vsConsts.values[(95*4)+2], vsConsts.values[(95*4)+3]);

  //CreateWindow();
  //InitOpenGL();
  //Xe::Microcode::CreateShader();
  //Xe::Microcode::CreateBuffers(psConsts, vsConsts, boolConsts);
  //u64 combinedHash = (static_cast<u64>(Xe::Microcode::vertexShaderHash) << 32) | Xe::Microcode::pixelShaderHash;
  //auto shader = Xe::Microcode::linkedShaderPrograms[combinedHash];
  //CreateVAOAndVBOFromShader(shader.vertexShader, VAO, VBO);
  //while (XeRunning) {
  //  HandleEvents();
  //  RenderFrame();
  //}
  //glDeleteVertexArrays(1, &VAO);
  //glDeleteBuffers(1, &VBO);
  //shaderFactory->Destroy();
  //shaderFactory.reset();
  //resourceFactory.reset();
  //SDL_GL_DestroyContext(context);
  //SDL_DestroyWindow(mainWindow);
  //SDL_Quit();
  return 0;
}
