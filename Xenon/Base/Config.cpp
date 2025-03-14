// Copyright 2025 Xenon Emulator Project

#include <toml.hpp>

#include "Config.h"

#include "Logging/Log.h"

namespace toml {
template <typename TC, typename K>
std::filesystem::path find_fs_path_or(const basic_value<TC> &v, const K &ky,
                                      std::filesystem::path opt) {
  try {
    auto str = find<std::string>(v, ky);
    if (str.empty()) {
      return opt;
    }
    std::u8string u8str{(char8_t *)&str.front(), (char8_t *)&str.back() + 1};
    return std::filesystem::path{u8str};
  } catch (...) {
    return opt;
  }
}
} // namespace toml

namespace Config {

bool fullscreenMode() { return isFullscreen; }
bool vsync() { return vsyncEnabled; }
bool gpuThreadEnabled() { return gpuRenderThreadEnabled; }
bool quitOnWindowClosure() { return shouldQuitOnWindowClosure; }
Base::Log::Level getCurrentLogLevel() { return currentLogLevel; }
bool logAdvanced() { return islogAdvanced; }

int smcCurrentAvPack() { return smcAvPackType; }
int smcPowerOnType() { return smcPowerOnReason; }
std::string& COMPort() { return com = "\\\\.\\COM" + std::to_string(comPort); }
bool useBackupUART() { return useBackupUart; }

u64 HW_INIT_SKIP1() { return SKIP_HW_INIT_1; }
u64 HW_INIT_SKIP2() { return SKIP_HW_INIT_2; }

s32 windowWidth() { return screenWidth; }
s32 windowHeight() { return screenHeight; }
s32 internalWindowWidth() { return internalWidth; }
s32 internalWindowHeight() { return internalHeight; }

std::string fusesPath() { return fusesTxtPath; }
std::string oneBlPath() { return oneBlBinPath; }
std::string nandPath() { return nandBinPath; }
std::string elfPath() { return elfBinaryPath; }
std::string oddImagePath() { return oddDiscImagePath; }

std::string imguiIniPath() { return imguiConfigPath; }
bool imguiDebug() { return imguiDebugWindow; }

//s32 getGpuId() {
//  return gpuId;
//}

int cpi() { return clocksPerInstruction; }

bool loadElfs() { return elfLoader; }

void loadConfig(const std::filesystem::path &path) {
  // If the configuration file does not exist, create it and return.
  LOG_INFO(Config, "Loading configuration from: {}", path.string());
  std::ifstream configFile{ path };
  std::error_code error;
  if (!std::filesystem::exists(path, error) && !configFile.is_open()) {
#ifdef _WIN32
    std::string pathPrefix{};
#else
    // TODO(Vali0004): Pull if Linux or MacOS
    std::string pathPrefix{ getenv("HOME") };
    fusesTxtPath.insert(0, pathPrefix.c_str());
    oneBlBinPath.insert(0, pathPrefix.c_str());
    nandBinPath.insert(0, pathPrefix.c_str());
    elfBinaryPath.insert(0, pathPrefix.c_str());
    oddDiscImagePath.insert(0, pathPrefix.c_str());
#endif
    saveConfig(path);
    return;
  }
  configFile.close();

  toml::value data;

  try {
    data = toml::parse(path);
  } catch (std::exception &ex) {
    LOG_ERROR(Config, "Got exception trying to load config file. Exception: {}",
              ex.what());
    return;
  }

  if (data.contains("General")) {
    const toml::value &general = data.at("General");
    gpuRenderThreadEnabled =
        toml::find_or<bool>(general, "GPURenderThreadEnabled", gpuRenderThreadEnabled);
    isFullscreen = toml::find_or<bool>(general, "Fullscreen", isFullscreen);
    vsyncEnabled = toml::find_or<bool>(general, "VSync", vsyncEnabled);
    shouldQuitOnWindowClosure =
        toml::find_or<bool>(general, "QuitOnWindowClosure", shouldQuitOnWindowClosure);
    int logLevel = static_cast<int>(currentLogLevel);
    logLevel = toml::find_or<int&>(general, "LogLevel", logLevel);
    currentLogLevel = static_cast<Base::Log::Level>(logLevel);
    islogAdvanced = toml::find_or<bool>(general, "logAdvanced", islogAdvanced);
  }

  if (data.contains("SMC")) {
    const toml::value &smc = data.at("SMC");
    comPort = toml::find_or<int&>(smc, "COMPort", comPort);
    smcAvPackType = toml::find_or<int&>(smc, "SMCAvPackType", smcAvPackType);
    smcPowerOnReason = toml::find_or<int&>(smc, "SMCPowerOnType", smcPowerOnReason);
    useBackupUart = toml::find_or<bool>(smc, "UseBackupUART", useBackupUart);
  }

  if (data.contains("PowerPC")) {
    const toml::value &powerpc = data.at("PowerPC");
    SKIP_HW_INIT_1 = toml::find_or<u64&>(powerpc, "HW_INIT_SKIP1", SKIP_HW_INIT_1);
    SKIP_HW_INIT_2 = toml::find_or<u64&>(powerpc, "HW_INIT_SKIP2", SKIP_HW_INIT_2);
  }

  if (data.contains("GPU")) {
    const toml::value &gpu = data.at("GPU");
    screenWidth = toml::find_or<int&>(gpu, "screenWidth", screenWidth);
    screenHeight = toml::find_or<int&>(gpu, "screenHeight", screenHeight);
    internalWidth = toml::find_or<int&>(gpu, "internalWidth", internalWidth);
    internalHeight = toml::find_or<int&>(gpu, "internalHeight", internalHeight);
    // gpuId = toml::find_or<int&>(gpu, "gpuId", gpuId);
  }

  if (data.contains("Paths")) {
    const toml::value &paths = data.at("Paths");
    fusesTxtPath =
        toml::find_or<std::string>(paths, "Fuses", fusesTxtPath);
    oneBlBinPath =
        toml::find_or<std::string>(paths, "OneBL", oneBlBinPath);
    nandBinPath =
        toml::find_or<std::string>(paths, "Nand", nandBinPath);
    oddDiscImagePath =
        toml::find_or<std::string>(paths, "ODDImage", oddDiscImagePath);
    elfBinaryPath =
      toml::find_or<std::string>(paths, "ElfBinary", elfBinaryPath);
  }

  if (data.contains("ImGui")) {
    const toml::value &imgui = data.at("HighlyExperimental");
    imguiConfigPath =
      toml::find_or<std::string>(imgui, "Config", imguiConfigPath);
    imguiDebugWindow =
        toml::find_or<bool>(imgui, "DebugWindow", imguiDebugWindow);
  }

  if (data.contains("HighlyExperimental")) {
    const toml::value &highlyExperimental = data.at("HighlyExperimental");
    clocksPerInstruction =
        toml::find_or<int&>(highlyExperimental, "CPI", clocksPerInstruction);
    elfLoader =
        toml::find_or<bool>(highlyExperimental, "ElfLoader", elfLoader);
  }
}

void saveConfig(const std::filesystem::path &path) {
  toml::value data;

  std::error_code error;
  if (std::filesystem::exists(path, error)) {
    try {
      data = toml::parse(path);
    } catch (const std::exception &ex) {
      LOG_ERROR(Config, "Exception trying to parse config file. Exception: {}",
          ex.what());
      return;
    }
  } else {
    if (error) {
      LOG_ERROR(Config, "Filesystem error: {}", error.message());
    }
    LOG_INFO(Config, "Config not found. Saving new configuration file: {}", path.string());
  }

  // Vali0004:
  // Why do we clear comments? It's because when adding new comments, or saving the config after modification (ex, with a GUI in the future),
  // it'll have duplicates if we don't. So just follow what I did, and clear them for your sanity.
  // Note: Even if they don't cause problems, I'll still yell at you :P

  // General.
  data["General"]["GPURenderThreadEnabled"].comments().clear();
  data["General"]["LogLevel"].comments().clear();

  data["General"]["GPURenderThreadEnabled"].comments().push_back("# Enable the GPU Render thread and main window.");
  data["General"]["GPURenderThreadEnabled"] = gpuRenderThreadEnabled;
  data["General"]["Fullscreen"] = isFullscreen;
  data["General"]["VSync"] = vsyncEnabled;
  data["General"]["QuitOnWindowClosure"] = shouldQuitOnWindowClosure;
  data["General"]["LogLevel"].comments().push_back("# Controls the current log level output filter");
  data["General"]["LogLevel"] = static_cast<int>(currentLogLevel);
  data["General"]["LogAdvanced"] = islogAdvanced;

  // SMC.
  data["SMC"]["COMPort"].comments().clear();
  data["SMC"]["SMCAvPackType"].comments().clear();
  data["SMC"]["SMCPowerOnType"].comments().clear();
  data["SMC"]["UseBackupUART"].comments().clear();

  data["SMC"]["COMPort"].comments().push_back("# Current vCOM Port used for communication between Xenon and your PC");
  data["SMC"]["COMPort"] = comPort;
  data["SMC"]["SMCAvPackType"].comments().push_back("# The current connected AV Pack. Used to set Xenos internal render resolution");
  data["SMC"]["SMCAvPackType"].comments().push_back("# Default value is 31 (HDMI_NO_AUDIO) = 1280*720");
  data["SMC"]["SMCAvPackType"].comments().push_back("# Lowest value is 87 (COMPOSITE) = 640*480");
  data["SMC"]["SMCAvPackType"].comments().push_back("# Note: The window size must never be smaller than the internal resolution");
  data["SMC"]["SMCAvPackType"] = smcAvPackType;
  data["SMC"]["SMCPowerOnType"].comments().push_back("# SMC power-up type/cause (Power Button, Eject Button, etc...)");
  data["SMC"]["SMCPowerOnType"].comments().push_back("# Most used values are:");
  data["SMC"]["SMCPowerOnType"].comments().push_back("# 17: Console is being powered by a Power button press");
  data["SMC"]["SMCPowerOnType"].comments().push_back("# 18: Console is being powered by an Eject button press");
  data["SMC"]["SMCPowerOnType"].comments().push_back("# Note: When trying to boot Linux/XeLL Reloaded this must be set to 18");
  data["SMC"]["SMCPowerOnType"] = smcPowerOnReason;
  data["SMC"]["UseBackupUART"].comments().push_back("# If the selected vCOM port is unavailable, use printf instead");
  data["SMC"]["UseBackupUART"] = useBackupUart;

  // PowerPC.
  data["PowerPC"]["HW_INIT_SKIP1"].comments().clear();
  data["PowerPC"]["HW_INIT_SKIP2"].comments().clear();

  data["PowerPC"]["HW_INIT_SKIP1"].comments().push_back("# Hardware Init Skip address 1");
  data["PowerPC"]["HW_INIT_SKIP1"] = SKIP_HW_INIT_1;
  data["PowerPC"]["HW_INIT_SKIP2"].comments().push_back("# Hardware Init Skip address 2");
  data["PowerPC"]["HW_INIT_SKIP2"] = SKIP_HW_INIT_2;

  // GPU.
  data["GPU"]["screenWidth"].comments().clear();
  data["GPU"]["screenHeight"].comments().clear();
  data["GPU"]["internalWidth"].comments().clear();
  data["GPU"]["internalHeight"].comments().clear();

  data["GPU"]["screenWidth"].comments().push_back("# Window Width");
  data["GPU"]["screenWidth"] = screenWidth;
  data["GPU"]["screenHeight"].comments().push_back("# Window Height");
  data["GPU"]["screenHeight"] = screenHeight;
  data["GPU"]["internalHeight"].comments().push_back("# Internal Width (The width of what XeLL uses, do not modify)");
  data["GPU"]["internalWidth"] = internalWidth;
  data["GPU"]["internalHeight"].comments().push_back("# Internal Height (The height of what XeLL uses, do not modify)");
  data["GPU"]["internalHeight"] = internalHeight;
  //data["GPU"]["gpuId"] = gpuId;

  // Paths.
  data["Paths"]["ImGuiConfig"].comments().clear();

  data["Paths"]["Fuses"] = fusesTxtPath;
  data["Paths"]["OneBL"] = oneBlBinPath;
  data["Paths"]["Nand"] = nandBinPath;
  data["Paths"]["ODDImage"] = oddDiscImagePath;
  data["Paths"]["ElfBinary"] = elfBinaryPath;

  // ImGui
  data["ImGui"]["Config"].comments().push_back("# ImGui Ini Path");
  data["ImGui"]["Config"].comments().push_back("# 'none' is disabled. It's relative based on the binary path");
  data["ImGui"]["Config"] = imguiConfigPath;

  // HighlyExperimental.
  data["HighlyExperimental"].comments().clear();
  data["HighlyExperimental"]["CPI"].comments().clear();
  data["HighlyExperimental"]["ElfLoader"].comments().clear();

  data["HighlyExperimental"].comments().push_back("# Do not touch these options unless you know what you're doing!");
  data["HighlyExperimental"].comments().push_back("# It can break execution! User beware");
  data["HighlyExperimental"]["CPI"].comments().push_back("# Clocks Per Instruction. If your system has a lower-than-average CPI, adjust accordingly");
  data["HighlyExperimental"]["CPI"].comments().push_back("# Note: This will mess with execution timing, and may break time-sensitive things like XeLL");
  data["HighlyExperimental"]["CPI"].comments().push_back("# Zero will use the estimated CPI for your system (check log for more info)");
  data["HighlyExperimental"]["CPI"] = clocksPerInstruction;
  data["HighlyExperimental"]["ElfLoader"].comments().push_back("# Disables normal codeflow and loads kernel.elf");
  data["HighlyExperimental"]["ElfLoader"] = elfLoader;

  std::ofstream file(path, std::ios::binary);
  file << data;
  file.close();
}

} // namespace Config
