// Copyright 2025 Xenon Emulator Project

#pragma once

#include <cstdlib>
#include <filesystem>

#include "Types.h"
#include "Logging/Backend.h"

namespace Config {
// General.
inline bool gpuRenderThreadEnabled = true;
inline bool isFullscreen = false;
inline bool vsyncEnabled = true;
inline bool shouldQuitOnWindowClosure = false;
inline Base::Log::Level currentLogLevel = Base::Log::Level::Warning;
inline bool islogAdvanced = false;

// SMC.
inline int smcPowerOnReason = 0x11; // SMC_PWR_REAS_EJECT
inline int smcAvPackType = 31; // Set to HDMI_NO_AUDIO. See SMC.cpp for a list of values.
inline int comPort = 2;
inline std::string com = "";

// PowerPC.
inline u64 SKIP_HW_INIT_1 = 0;
inline u64 SKIP_HW_INIT_2 = 0;

// GPU.
inline s32 screenWidth = 1280;
inline s32 screenHeight = 720;
inline s32 internalWidth = 1280;
inline s32 internalHeight = 720;
// inline s32 gpuId = -1; // Vulkan physical device index. Set to negative for auto select

// Filepaths.
#ifdef _WIN32
inline std::string fusesTxtPath = "C:/Xbox/fuses.txt";
inline std::string oneBlBinPath = "C:/Xbox/1bl.bin";
inline std::string nandBinPath = "C:/Xbox/nand.bin";
inline std::string oddDiscImagePath = "C:/Xbox/xenon.iso";
#else
inline std::string fusesTxtPath = "/Xbox/fuses.txt";
inline std::string oneBlBinPath = "/Xbox/1bl.bin";
inline std::string nandBinPath = "/Xbox/nand.bin";
inline std::string oddDiscImagePath = "/Xbox/xenon.iso";
#endif

// Highly experimental.
inline int ticksPerInstruction = 1;

void loadConfig(const std::filesystem::path &path);
void saveConfig(const std::filesystem::path &path);

//
// General Options.
//

// Selected COM Port.
std::string* COMPort();
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
std::string fusesPath();
// 1bl.bin path
std::string oneBlPath();
// nand.bin path
std::string nandPath();
// ODD Image path
std::string oddImagePath();

//
// Highly experimental. (things that can either break the emulator or drastically increase performance)
//
int tpi();

} // namespace Config
