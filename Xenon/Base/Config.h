// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include <chrono>
#include <filesystem>
#include <map>
#include <string>
#include <thread>

#include <toml.hpp>

#include "Logging/Backend.h"

using namespace std::chrono_literals;

namespace Config {
//
// A helper class for width/height values
//
struct _resolution {
  // Width
  u32 width;
  // Height
  u32 height;

  // TOML Conversion
  void to_toml(toml::value& value) {
    value["Width"].comments().clear();
    value["Width"] = width;
    value["Width"].comments().push_back("# Width");
    value["Height"].comments().clear();
    value["Height"] = height;
    value["Height"].comments().push_back("# Height");
  }
  void from_toml(const std::string &key, const toml::value& value) {
    const toml::value& _key = value.at(key);
    width = toml::find_or<u32&>(_key, "Width", width);
    height = toml::find_or<u32&>(_key, "Height", height);
  }
};

//
// Rendering
//
inline struct _rendering {
  // Enable GPU Render thread
  bool enable = true;
  // Window Resolution
  _resolution window{ 1280, 720 };
  // Render in fullscreen
  bool isFullscreen = false;
  // Is VSync is present or not?
  bool vsync = true;
  // Should we quit when our rendering window is closed?
  bool quitOnWindowClosure = true;
  // Pause on focus loss
  bool pauseOnFocusLoss = false;
  // GPU ID Selection (Only for Vulkan/DirectX)
  s32 gpuId = -1;
  // Backend selection
  std::string backend = "OpenGL";
  // Debug validation
  bool debugValidation = false;

  // TOML Conversion
  void to_toml(toml::value &value);
  void from_toml(const toml::value &value);
  bool verify_toml(toml::value &value);
} rendering;

//
// ImGui
//
inline struct _imgui {
  // None is disabled, and it is relative
  std::string configPath = "none";
  // Viewports
  bool viewports = false;
  // Debug Window
  bool debugWindow = true;

  // TOML Conversion
  void to_toml(toml::value &value);
  void from_toml(const toml::value &value);
  bool verify_toml(toml::value &value);
} imgui;

//
// Debug
//
inline struct _debug {
  // Halt on mmu write to this address
  u64 haltOnReadAddress = 0;
  // Halt on mmu read to this address
  u64 haltOnWriteAddress = 0;
  // Halt on execution of this address
  u64 haltOnAddress = 0;
  // Start the CPU halted
  bool startHalted = false;
  // Soft halts on assertions, otherwise, ignores them
  bool softHaltOnAssertions = true;
  // Automatically continue on guest assertion
  bool autoContinueOnGuestAssertion = false;
#ifdef DEBUG_BUILD
  // Create a trace file | NOTE: This can create up to a 20GB file
  bool createTraceFile = false;
#endif

  // TOML Conversion
  void to_toml(toml::value &value);
  void from_toml(const toml::value &value);
  bool verify_toml(toml::value &value);
} debug;

//
// SMC
//
inline struct _smc {
  // SMC Detected AV Pack. Tells the system what kind of video output it is connected to
  // This is used to detect the current resolution
  s32 avPackType = 31; // Set to HDMI_NO_AUDIO. See SMC.cpp for a list of values.
  // SMC Power On type (Power button, eject button, controller, etc...)
  s32 powerOnReason = 0x11; // SMC_PWR_REASON_EJECT
  // UART System
  // vcom is vCOM, only present on Windows
  // socket is Socket, avaliable via Netcat/socat
  // print is Printf, directly to log
#ifdef _WIN32
  std::string uartSystem = "vcom";
#else
  std::string uartSystem = "socket";
#endif // _WIN32
#ifdef _WIN32
  // Selected vCOM Port
  s32 comPort = 2;

  std::string COMPort() {
    return "\\\\.\\COM" + std::to_string(comPort);
  }
#endif
  // Socket IP to listen on, default is localhost
  std::string socketIp = "127.0.0.1";
  // Socket Port to listen on, default is 7000
  u16 socketPort = 7000;

  // TOML Conversion
  void to_toml(toml::value &value);
  void from_toml(const toml::value &value);
  bool verify_toml(toml::value& value);
} smc;

//
// XCPU
//
inline struct _xcpu {
  // CPU RAM Size
  std::string ramSize = "512MiB";
  // Loads an elf from the ElfBinary path
  bool elfLoader = false;
  // CPI for your system, do not modify
  s32 clocksPerInstruction = 0;
  // CB/SB HW_INIT_SKIP
  bool overrideInitSkip = false;
  u64 HW_INIT_SKIP_1 = 0;
  u64 HW_INIT_SKIP_2 = 0;
  // 1BL Simulation
  bool simulate1BL = false;
  // Instruction tests execution
  bool runInstrTests = false;
  // Instruction tests mode
  u8 instrTestsMode = 0; // See ePPUTestingMode
  // TOML Conversion
  void to_toml(toml::value &value);
  void from_toml(const toml::value &value);
  bool verify_toml(toml::value &value);
} xcpu;

//
// XGPU
//
inline struct _xgpu {
  // Internal Resolution | The resolution XeLL uses
  _resolution internal{ 1280, 720 };

  // TOML Conversion
  void to_toml(toml::value &value);
  void from_toml(const toml::value &value);
  bool verify_toml(toml::value &value);
} xgpu;

//
// Filepaths
//
inline struct _filepaths {
  // Fuses path
  std::string fuses = "fuses.txt";
  // 1bl.bin path
  std::string oneBl = "1bl.bin";
  // nand.bin path
  std::string nand = "nand.bin";
  // ODD Image path
  std::string oddImage = "xenon.iso";
  // HDD Image path
  std::string hddImage = "xenonHDD.img";
  // Elf binary path
  std::string elfBinary = "kernel.elf";
  // Instruction tests base path.
  std::string instrTestsPath = "tests";
  // Instruction tests bin path.
  std::string instrTestsBinPath = "bin";

  // Corrects the paths on first time creation
  void correct(const fs::path &basePath) {
    auto fusesPath = basePath / fuses;
    fuses = fusesPath.string();
    auto oneBlPath = basePath / oneBl;
    oneBl = oneBlPath.string();
    auto nandPath = basePath / nand;
    nand = nandPath.string();
    auto oddImagePath = basePath / oddImage;
    oddImage = oddImagePath.string();
    auto hddImagePath = basePath / hddImage;
    hddImage = hddImagePath.string();
    auto elfBinaryPath = basePath / elfBinary;
    elfBinary = elfBinaryPath.string();
    auto instrTestsBasePath = basePath / instrTestsPath;
    instrTestsPath = instrTestsBasePath.string();
    auto instrTestsBinaryPath = basePath / instrTestsBinPath;
    instrTestsBinPath = instrTestsBinaryPath.string();
  }

  // TOML Conversion
  void to_toml(toml::value &value);
  void from_toml(const toml::value &value);
  bool verify_toml(toml::value &value);
} filepaths;

//
// Log
//
inline struct _log {
  // Current log level
  Base::Log::Level currentLevel = Base::Log::Level::Info;
  // Log type
  // async = Queues message
  // sync = waits for log to finish
  std::string type = "async";
  // Show more details on log
  bool advanced = false;
  // Show debug-only log statements
#ifdef DEBUG_BUILD
  bool debugOnly = false;
#endif

  // TOML Conversion
  void to_toml(toml::value &value);
  void from_toml(const toml::value &value);
  bool verify_toml(toml::value &value);
} log;

enum class eConsoleRevision : const u8 {
  Xenon,
//  Opus, - Not supported atm
  Zephyr,
  Falcon,
  Jasper,
  Trinity,
  Corona,
  Corona4GB,
  Winchester
};

//
// Highly experimental (things that can either break the emulator or drastically increase performance)
//
inline struct _highlyExperimental {
  eConsoleRevision consoleRevison = eConsoleRevision::Corona;
  // Executor modes:
  // Interpreted - Cached Interpreter
  // Hybrid - JIT with Cached Interpreter fallback
  // JIT - Just In Time
  std::string cpuExecutor = "Interpreted";
  s32 clocksPerInstructionBypass = 0;

  // TOML Conversion
  void to_toml(toml::value &value);
  void from_toml(const toml::value &value);
  bool verify_toml(toml::value &value);
} highlyExperimental;

void loadConfig(const fs::path &path);
void saveConfig(const fs::path &path);

} // namespace Config
