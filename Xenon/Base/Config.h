// Copyright 2025 Xenon Emulator Project

#pragma once

#include <cstdlib>
#include <filesystem>

#include "Types.h"
#include "Logging/Backend.h"

namespace Config {
// General
inline bool gpuRenderThreadEnabled = true;
inline bool isFullscreen = false;
inline bool vsyncEnabled = true;
inline bool shouldQuitOnWindowClosure = false;
inline Base::Log::Level currentLogLevel = Base::Log::Level::Warning;
inline bool islogAdvanced = false;

// SMC
inline int smcPowerOnReason = 0x11; // SMC_PWR_REAS_EJECT
inline int smcAvPackType = 31; // Set to HDMI_NO_AUDIO. See SMC.cpp for a list of values.
inline int comPort = 2;
inline bool useBackupUart = false;
inline std::string com = "";

// PowerPC
inline u64 SKIP_HW_INIT_1 = 0;
inline u64 SKIP_HW_INIT_2 = 0;

// GPU
inline s32 screenWidth = 1280;
inline s32 screenHeight = 720;
inline s32 internalWidth = 1280;
inline s32 internalHeight = 720;
// inline s32 gpuId = -1; // Vulkan physical device index. Set to negative for auto select

// Filepaths
#ifdef _WIN32
#define PATH_PREPEND "C:/"
#else
#define PATH_PREPEND "/"
#endif
inline std::string fusesTxtPath = PATH_PREPEND "Xbox/fuses.txt";
inline std::string oneBlBinPath = PATH_PREPEND "Xbox/1bl.bin";
inline std::string nandBinPath = PATH_PREPEND "Xbox/nand.bin";
inline std::string elfBinaryPath = PATH_PREPEND "Xbox/kernel.elf";
inline std::string oddDiscImagePath = PATH_PREPEND "Xbox/xenon.iso";

// ImGui
// None is disabled, and it is relative
inline std::string imguiConfigPath = "none";
// Debug Window
inline bool imguiDebugWindow = true;

// Debug
inline u64 haltOnReadAddress = 0;
inline u64 haltOnWriteAddress = 0;
inline u64 haltOnAddress = 0;
inline bool startHalted = false;

// Highly experimental
inline int clocksPerInstruction = 0;
inline bool elfLoader = false;

void loadConfig(const std::filesystem::path &path);
void saveConfig(const std::filesystem::path &path);

//
// General Options.
//
// Show in fullscreen.
bool fullscreenMode();
// Enable VSync.
bool vsync();
// Enable GPU Render thread.
bool gpuThreadEnabled();
// Should we quit when our rendering window is closed?
bool quitOnWindowClosure();
// Current log level.
Base::Log::Level getCurrentLogLevel();
// Show more details on log.
bool logAdvanced();

//
// SMC Options.
//

// SMC Detected AV Pack. Tells the system what kind of video output it is connected to.
// This is used to detect the current resolution.
int smcCurrentAvPack();
// SMC Power On type (PowerButton, eject button, controller, etc...).
int smcPowerOnType();
// Selected COM Port.
std::string& COMPort();
// Selected COM Port.
bool useBackupUART();

//
// PowerPC Options.
//

// HW_INIT_SKIP.
u64 HW_INIT_SKIP1();
u64 HW_INIT_SKIP2();

//
// GPU Options.
//

// Screen Size.
s32 windowWidth();
s32 windowHeight();
// Intermal Size.
s32 internalWindowWidth();
s32 internalWindowHeight();
// GPU ID Selection (Only for Vulkan)
// s32 getGpuId();

//
// Filepaths.
//

// Fuses path
const std::string fusesPath();
// 1bl.bin path
const std::string oneBlPath();
// nand.bin path
const std::string nandPath();
// Elf path
const std::string elfPath();
// ODD Image path
const std::string oddImagePath();

//
// ImGui
// 
// ImGui Ini path
const std::string imguiIniPath();
// ImGui Debug Window
bool imguiDebug();

//
// Debug
// 
// Halt on mmu read address
u64 haltOnRead();
// Halt on mmu write address
u64 haltOnWrite();
// Halt on execution of this address
u64 haltOn();
// Start the CPU halted
bool startCPUHalted();

//
// Highly experimental. (things that can either break the emulator or drastically increase performance)
//
int cpi();
bool loadElfs();

} // namespace Config
