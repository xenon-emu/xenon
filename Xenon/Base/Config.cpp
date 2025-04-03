// Copyright 2025 Xenon Emulator Project

#include "Config.h"
#include "Path_util.h"

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

// Vali0004:
// Why do we clear comments? It's because when adding new comments, or saving the config after modification (ex, with the GUI),
// it'll have duplicates if we don't. So just follow what I did, and clear them for your sanity.
// Note: Even if they don't cause problems, I'll still yell at you :P

#define cache_value(x) auto prev_##x = x
#define verify_value(x) if (prev_##x != x) { LOG_INFO(Config, "Value '{}' didn't match!", #x); return false; }
void _rendering::from_toml(const toml::value &value) {
  enable = toml::find_or<bool>(value, "Enable", enable);
  enableGui = toml::find_or<bool>(value, "EnableGUI", enableGui);
  window.from_toml("Resolution", value);
  isFullscreen = toml::find_or<bool>(value, "Fullscreen", isFullscreen);
  vsync = toml::find_or<bool>(value, "VSync", vsync);
  quitOnWindowClosure =
    toml::find_or<bool>(value, "QuitOnWindowClosure", quitOnWindowClosure);
  //gpuId = toml::find_or<s32&>(gpu, "GPU", gpuId);
}
void _rendering::to_toml(toml::value &value) {
  value["Enable"].comments().clear();
  value["Enable"] = enable;
  value["Enable"].comments().push_back("# Enables/disables rendering entirely");
  value["EnableGUI"].comments().clear();
  value["EnableGUI"] = enableGui;
  value["EnableGUI"].comments().push_back("# Enables/disables GUI creation");
  value["Resolution"].comments().clear();
  window.to_toml(value["Resolution"]);
  value["Resolution"].comments().push_back("# Window Resolution");
  value["Fullscreen"].comments().clear();
  value["Fullscreen"] = isFullscreen;
  value["Fullscreen"].comments().push_back("# Fullscreen Mode");
  value["VSync"].comments().clear();
  value["VSync"] = vsync;
  value["VSync"].comments().push_back("# VSync is Variable Sync");
  value["QuitOnWindowClosure"].comments().clear();
  value["QuitOnWindowClosure"] = quitOnWindowClosure;
  value["QuitOnWindowClosure"].comments().push_back("# Closes the process when the Renderer is destroyed");
  //value["GPU"].comments().clear();
  //value["GPU"].comments().push_back("# Chooeses which GPU to use if there are multiple (Vulkan/DirectX only)");
  //value["GPU"] = gpuId;
}
bool _rendering::verify_toml(toml::value &value) {
  to_toml(value);
  cache_value(enable);
  cache_value(enableGui);
  cache_value(window);
  cache_value(isFullscreen);
  cache_value(vsync);
  cache_value(quitOnWindowClosure);
  from_toml(value);
  verify_value(enable);
  verify_value(enableGui);
  verify_value(window.width);
  verify_value(window.height);
  verify_value(isFullscreen);
  verify_value(vsync);
  verify_value(quitOnWindowClosure);
  return true;
}

void _imgui::from_toml(const toml::value &value) {
  configPath = toml::find_or<std::string>(value, "Config", configPath);
  viewports = toml::find_or<bool>(value, "Viewports", viewports);
  debugWindow = toml::find_or<bool>(value, "DebugWindow", debugWindow);
}
void _imgui::to_toml(toml::value &value) {
  value["Config"].comments().clear();
  value["Config"] = configPath;
  value["Config"].comments().push_back("# ImGui Ini Path");
  value["Config"].comments().push_back("# 'none' is disabled. It's relative based on the binary path");
  value["Viewports"].comments().clear();
  value["Viewports"] = viewports;
  value["Viewports"].comments().push_back("# Enables/Disables ImGui Viewports");
  value["Viewports"].comments().push_back("# This makes ImGui Windows have their own context, aka, 'detached'");
  value["DebugWindow"].comments().clear();
  value["DebugWindow"] = debugWindow;
  value["DebugWindow"].comments().push_back("# Debug ImGui Window");
  value["DebugWindow"].comments().push_back("# Contains the debugger and other things");
}
bool _imgui::verify_toml(toml::value &value) {
  to_toml(value);
  cache_value(configPath);
  cache_value(viewports);
  cache_value(debugWindow);
  from_toml(value);
  verify_value(configPath);
  verify_value(viewports);
  verify_value(debugWindow);
  return true;
}

void _debug::from_toml(const toml::value &value) {
  haltOnReadAddress = toml::find_or<u64&>(value, "HaltOnRead", haltOnReadAddress);
  haltOnWriteAddress = toml::find_or<u64&>(value, "HaltOnWrite", haltOnWriteAddress);
  haltOnAddress = toml::find_or<u64&>(value, "HaltOnAddress", haltOnWriteAddress);
  startHalted = toml::find_or<bool>(value, "StartHalted", startHalted);
}
void _debug::to_toml(toml::value &value) {
  value["HaltOnRead"].comments().clear();
  value["HaltOnRead"] = haltOnReadAddress;
  value["HaltOnRead"].comments().push_back("# Address to halt on when the MMU reads from this address");
  value["HaltOnWrite"].comments().clear();
  value["HaltOnWrite"] = haltOnWriteAddress;
  value["HaltOnWrite"].comments().push_back("# Address to halt on when the MMU writes to this address");
  value["HaltOnWrite"].comments().clear();
  value["HaltOn"].comments().push_back("# Address to halt on when the CPU executes this address");
  value["HaltOn"] = haltOnAddress;
  value["HaltOn"].comments().clear();
  value["StartHalted"].comments().clear();
  value["StartHalted"] = startHalted;
  value["StartHalted"].comments().push_back("# Starts with the CPU halted");
}
bool _debug::verify_toml(toml::value &value) {
  to_toml(value);
  cache_value(haltOnReadAddress);
  cache_value(haltOnWriteAddress);
  cache_value(haltOnAddress);
  cache_value(startHalted);
  from_toml(value);
  verify_value(haltOnReadAddress);
  verify_value(haltOnWriteAddress);
  verify_value(haltOnAddress);
  verify_value(startHalted);
  return true;
}

void _smc::from_toml(const toml::value &value) {
  comPort = toml::find_or<s32&>(value, "COMPort", comPort);
  avPackType = toml::find_or<s32&>(value, "AvPackType", avPackType);
  powerOnReason = toml::find_or<s32&>(value, "PowerOnType", powerOnReason);
  useBackupUart = toml::find_or<bool>(value, "UseBackupUART", useBackupUart);
}
void _smc::to_toml(toml::value &value) {
  value["AvPackType"].comments().clear();
  value["AvPackType"] = avPackType;
  value["AvPackType"].comments().push_back("# The current connected AV Pack. Used to set Xenos internal render resolution");
  value["AvPackType"].comments().push_back("# Default value is 31 (HDMI_NO_AUDIO) = 1280*720");
  value["AvPackType"].comments().push_back("# Lowest value is 87 (COMPOSITE) = 640*480");
  value["AvPackType"].comments().push_back("# Note: The window size must never be smaller than the internal resolution");
  value["PowerOnType"].comments().clear();
  value["PowerOnType"] = powerOnReason;
  value["PowerOnType"].comments().push_back("# SMC power-up type/cause (Power Button, Eject Button, etc...)");
  value["PowerOnType"].comments().push_back("# Most used values are:");
  value["PowerOnType"].comments().push_back("# 17: Console is being powered by a Power button press");
  value["PowerOnType"].comments().push_back("# 18: Console is being powered by an Eject button press");
  value["PowerOnType"].comments().push_back("# Note: When trying to boot Linux/XeLL Reloaded this must be set to 18");
  value["COMPort"].comments().clear();

  value["COMPort"] = comPort;
  value["COMPort"].comments().push_back("# Current vCOM Port used for communication between Xenon and your PC");
  value["UseBackupUART"].comments().clear();
  value["UseBackupUART"] = useBackupUart;
  value["UseBackupUART"].comments().push_back("# If the selected vCOM port is unavailable, use printf instead");
}
bool _smc::verify_toml(toml::value &value) {
  to_toml(value);
  cache_value(comPort);
  cache_value(avPackType);
  cache_value(powerOnReason);
  cache_value(useBackupUart);
  from_toml(value);
  verify_value(comPort);
  verify_value(avPackType);
  verify_value(powerOnReason);
  verify_value(useBackupUart);
  return true;
}

void _xcpu::from_toml(const toml::value &value) {
  elfLoader = toml::find_or<bool>(value, "ElfLoader", elfLoader);
  clocksPerInstruction = toml::find_or<s32&>(value, "CPI", clocksPerInstruction);
  HW_INIT_SKIP_1 = toml::find_or<u64&>(value, "HW_INIT_SKIP1", HW_INIT_SKIP_1);
  HW_INIT_SKIP_2 = toml::find_or<u64&>(value, "HW_INIT_SKIP2", HW_INIT_SKIP_2);
}
void _xcpu::to_toml(toml::value &value) {
  value["ElfLoader"].comments().clear();
  value["ElfLoader"] = elfLoader;
  value["ElfLoader"].comments().push_back("# Disables normal codeflow and loads an elf from ElfBinary");

  value["CPI"].comments().clear();
  value["CPI"] = clocksPerInstruction;
  value["CPI"].comments().push_back("# [DO NOT MODIFY] Clocks Per Instruction [DO NOT MODIFY]");
  value["CPI"].comments().push_back("# If your system has a lower than average CPI, use CPI Bypass in HighlyExperimental");
  value["CPI"].comments().push_back("# Note: This will mess with execution timing, and may break time-sensitive things like XeLL");

  value["HW_INIT_SKIP1"].comments().clear();
  value["HW_INIT_SKIP1"] = HW_INIT_SKIP_1;
  value["HW_INIT_SKIP1"].comments().push_back("# Hardware Init Skip address 1");
  value["HW_INIT_SKIP1"].comments().push_back("# Possible HW1 Addresses:");
  value["HW_INIT_SKIP1"].comments().push_back("# Corona: 0x3003DC0");

  value["HW_INIT_SKIP2"].comments().clear();
  value["HW_INIT_SKIP2"] = HW_INIT_SKIP_2;
  value["HW_INIT_SKIP2"].comments().push_back("# Hardware Init Skip address 2");
  value["HW_INIT_SKIP2"].comments().push_back("# Possible HW2 Addresses:");
  value["HW_INIT_SKIP2"].comments().push_back("# Corona: 0x3003E54");
}
bool _xcpu::verify_toml(toml::value &value) {
  to_toml(value);
  cache_value(elfLoader);
  cache_value(clocksPerInstruction);
  cache_value(HW_INIT_SKIP_1);
  cache_value(HW_INIT_SKIP_2);
  from_toml(value);
  verify_value(elfLoader);
  verify_value(clocksPerInstruction);
  verify_value(HW_INIT_SKIP_1);
  verify_value(HW_INIT_SKIP_2);
  return true;
}

void _xgpu::from_toml(const toml::value &value) {
  internal.from_toml("Internal", value);
}
void _xgpu::to_toml(toml::value &value) {
  value["Internal"].comments().clear();
  internal.to_toml(value["Internal"]);
  value["Internal"].comments().push_back("# Internal Resolution (The width of what XeLL uses, do not modify)");
}
bool _xgpu::verify_toml(toml::value &value) {
  to_toml(value);
  cache_value(internal);
  from_toml(value);
  verify_value(internal.width);
  verify_value(internal.height);
  return true;
}

void _filepaths::from_toml(const toml::value &value) {
  fuses = toml::find_or<std::string>(value, "Fuses", fuses);
  oneBl = toml::find_or<std::string>(value, "OneBL", oneBl);
  nand = toml::find_or<std::string>(value, "Nand", nand);
  oddImage = toml::find_or<std::string>(value, "ODDImage", oddImage);
  elfBinary = toml::find_or<std::string>(value, "ElfBinary", elfBinary);
}
void _filepaths::to_toml(toml::value &value) {
  value["Fuses"] = fuses;
  value["OneBL"] = oneBl;
  value["Nand"] = nand;
  value["ODDImage"] = oddImage;
  value["ElfBinary"] = elfBinary;
}
bool _filepaths::verify_toml(toml::value &value) {
  to_toml(value);
  cache_value(fuses);
  cache_value(oneBl);
  cache_value(nand);
  cache_value(oddImage);
  cache_value(elfBinary);
  from_toml(value);
  verify_value(fuses);
  verify_value(oneBl);
  verify_value(nand);
  verify_value(oddImage);
  verify_value(elfBinary);
  return true;
}

void _log::from_toml(const toml::value &value) {
  s32 tmpLevel = static_cast<s32>(currentLevel);
  tmpLevel = toml::find_or<s32&>(value, "Level", tmpLevel);
  advanced = toml::find_or<bool>(value, "Advanced", advanced);
  currentLevel = static_cast<Base::Log::Level>(tmpLevel);
}
void _log::to_toml(toml::value &value) {
  value.comments().clear();
  value.comments().push_back("# Controls the current output filter level");
  s32 tmpLevel = static_cast<s32>(currentLevel);
  value["Level"] = tmpLevel;
  value["Advanced"] = advanced;
}
bool _log::verify_toml(toml::value &value) {
  to_toml(value);
  cache_value(currentLevel);
  cache_value(advanced);
  from_toml(value);
  verify_value(currentLevel);
  verify_value(advanced);
  return true;
}

void _highlyExperimental::from_toml(const toml::value &value) {
  clocksPerInstructionBypass = toml::find_or<s32&>(value, "CPIBypass", clocksPerInstructionBypass);
}
void _highlyExperimental::to_toml(toml::value &value) {
  value.comments().clear();
  value.comments().push_back("# Do not touch these options unless you know what you're doing!");
  value.comments().push_back("# It can break execution! User beware");
  value["CPIBypass"].comments().clear();
  value["CPIBypass"].comments().push_back("# Zero will use the estimated CPI for your system (view XCPU for more info)");
  value["CPIBypass"] = clocksPerInstructionBypass;
}
bool _highlyExperimental::verify_toml(toml::value &value) {
  to_toml(value);
  cache_value(clocksPerInstructionBypass);
  from_toml(value);
  verify_value(clocksPerInstructionBypass);
  return true;
}

#define verify_section(x, n) if (!x.verify_toml(data[#n])) { LOG_INFO(Config, "Failed to write '{}'! Section '{}' had a bad value", path.filename().string(), #x); return false; }
bool verifyConfig(const std::filesystem::path &path, toml::value &data) {
  std::error_code error{};
  // If it exists, parse it as a base
  if (std::filesystem::exists(path, error)) {
    try {
      data = toml::parse(path);
    } catch (std::exception &ex) {
      // Parse failed, bad read
      LOG_ERROR(Config, "Got an exception trying to load config file. {}", ex.what());
      return false;
    }
  }
#ifndef NO_GFX
  verify_section(rendering, Rendering);
  verify_section(imgui, ImGui);
#endif
  verify_section(debug, Debug);
  verify_section(smc, SMC);
  verify_section(xcpu, XCPU);
  verify_section(xgpu, XGPU);
  verify_section(filepaths, Paths);
  verify_section(log, Log);
  verify_section(highlyExperimental, HighlyExperimental);
  return true;
}

void loadConfig(const std::filesystem::path &path) {
  // If the configuration file does not exist, create it and return.
  LOG_INFO(Config, "Loading configuration from: {}", path.string());
  std::ifstream configFile{ path };
  std::error_code error;
  if (!std::filesystem::exists(path, error) && !configFile.is_open()) {
    filepaths.correct(Base::FS::GetUserPath(Base::FS::PathType::ConsoleDir));
    saveConfig(path);
    return;
  }
  configFile.close();

  toml::value data;

  try {
    data = toml::parse(path);
  } catch (std::exception &ex) {
    LOG_ERROR(Config, "Got an exception trying to load config file. {}",
              ex.what());
    return;
  }

#ifndef NO_GFX
  if (data.contains("Rendering")) {
    const toml::value &rendering_ = data.at("Rendering");
    rendering.from_toml(rendering_);
  }
  if (data.contains("ImGui")) {
    const toml::value &imgui_ = data.at("ImGui");
    imgui.from_toml(imgui_);
  }
#endif

  if (data.contains("SMC")) {
    const toml::value &smc_ = data.at("SMC");
    smc.from_toml(smc_);
  }

  if (data.contains("XCPU")) {
    const toml::value &xcpu_ = data.at("XCPU");
    xcpu.from_toml(xcpu_);
  }

  if (data.contains("XGPU")) {
    const toml::value &xgpu_ = data.at("XGPU");
    xgpu.from_toml(xgpu_);
  }

  if (data.contains("Paths")) {
    const toml::value &paths = data.at("Paths");
    filepaths.from_toml(paths);
  }

  if (data.contains("Debug")) {
    const toml::value &debug_ = data.at("Debug");
    debug.from_toml(debug_);
  }
  
  if (data.contains("Log")) {
    const toml::value &log_ = data.at("Log");
    log.from_toml(log_);
  }

  if (data.contains("HighlyExperimental")) {
    const toml::value &highlyExperimental_ = data.at("HighlyExperimental");
    highlyExperimental.from_toml(highlyExperimental_);
  }
}

void saveConfig(const std::filesystem::path &path) {
  toml::value data{};
  std::error_code error{};
  if (std::filesystem::exists(path, error)) {
    try {
      data = toml::parse(path);
    } catch (const std::exception &ex) {
      LOG_ERROR(Config, "Exception trying to parse config file. {}", ex.what());
      return;
    }
  } else {
    if (error) {
      LOG_ERROR(Config, "Filesystem error: {}", error.message());
    }
    LOG_INFO(Config, "Config not found. Saving new configuration file: {}", path.string());
  }

  std::string prev_path_str{ path.string() };
  std::string prev_file_str{ path.filename().string() };
  std::filesystem::path base_path{ prev_path_str.substr(0, prev_path_str.length() - prev_file_str.length() - 1) };
  std::filesystem::path new_path{ base_path / (prev_file_str + ".tmp") };

  // Write to toml, then verify contents
  if (!verifyConfig(new_path, data)) {
    return;
  }

  // Write to file
  try {
    std::ofstream file(new_path, std::ios::binary);
    file << data;
    file.close();
  } catch (const std::exception &ex) {
    LOG_ERROR(Config, "Exception trying to write config. {}", ex.what());
    return;
  }

  // Copy file config to current version, if it's valid
  try {
    std::error_code fs_error;
    u64 fSize = std::filesystem::file_size(new_path, fs_error);
    if (fSize != -1 && fSize) {
      std::filesystem::rename(new_path, path);
    } else {
      fSize = 0;
      if (fs_error) {
        LOG_ERROR(Config, "Filesystem error: {} ({})", fs_error.message(), fs_error.value());
      }
    }
  } catch (const std::exception &ex) {
    LOG_ERROR(Config, "Exception trying to copy backup config. {}", ex.what());
    return;
  }
}

} // namespace Config
