// Copyright 2025 Xenon Emulator Project

#pragma once

#include <cstdlib>
#include <map>
#include <filesystem>
#include <string>

#include <toml.hpp>

#include "Types.h"
#include "Logging/Backend.h"

namespace Config {
//
// A helper class for width/height values
//
struct _resolution {
  // Width
  s32 width;
  // Height
  s32 height;

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
    width = toml::find_or<s32&>(_key, "Width", width);
    height = toml::find_or<s32&>(_key, "Height", height);
  }
};

//
// Rendering
//
inline struct _rendering {
  // Enable GPU Render thread
  bool enable = true;
  // Whether to create the GUI handle
  bool enableGui = true;
  // Window Resolution
  _resolution window{ 1280, 720 };
  // Render in fullscreen
  bool isFullscreen = false;
  // Is VSync is present or not?
  bool vsync = true;
  // Should we quit when our rendering window is closed?
  bool quitOnWindowClosure = false;
  // GPU ID Selection (Only for Vulkan/DirectX)
  //s32 gpuId = -1;

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
  int avPackType = 31; // Set to HDMI_NO_AUDIO. See SMC.cpp for a list of values.
  // SMC Power On type (Power button, eject button, controller, etc...)
  int powerOnReason = 0x11; // SMC_PWR_REAS_EJECT
  // Selected vCOM Port
  int comPort = 2;
  // Backup UART, kicks on when a vCOM is not present
  bool useBackupUart = false;
  std::string COMPort() {
    return "\\\\.\\COM" + std::to_string(comPort);
  }

  // TOML Conversion
  void to_toml(toml::value &value);
  void from_toml(const toml::value &value);
  bool verify_toml(toml::value& value);
} smc;

//
// XCPU
//
inline struct _xcpu {
  // HW_INIT_SKIP
  u64 HW_INIT_SKIP_1 = 0;
  u64 HW_INIT_SKIP_2 = 0;

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
  // Elf binary path
  std::string elfBinary = "kernel.elf";
  // ODD Image path
  std::string oddImage = "xenon.iso";

  // Corrects the paths on first time creation
  void correct(const std::filesystem::path &basePath) {
    fuses = std::filesystem::path(basePath).append(fuses).string();
    oneBl = std::filesystem::path(basePath).append(oneBl).string();
    nand = std::filesystem::path(basePath).append(nand).string();
    elfBinary = std::filesystem::path(basePath).append(elfBinary).string();
    oddImage = std::filesystem::path(basePath).append(oddImage).string();
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
  Base::Log::Level currentLevel = Base::Log::Level::Warning;
  // Show more details on log
  bool advanced = false;

  // TOML Conversion
  void to_toml(toml::value &value);
  void from_toml(const toml::value &value);
  bool verify_toml(toml::value &value);
} log;

//
// Highly experimental (things that can either break the emulator or drastically increase performance)
//
inline struct _highlyExperimental {
  int clocksPerInstruction = 0;
  bool elfLoader = false;

  // TOML Conversion
  void to_toml(toml::value &value);
  void from_toml(const toml::value &value);
  bool verify_toml(toml::value &value);
} highlyExperimental;

void loadConfig(const std::filesystem::path &path);
void saveConfig(const std::filesystem::path &path);

} // namespace Config
