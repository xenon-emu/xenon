// Copyright 2025 Xenon Emulator Project

#include "Config.h"
#include "Path_util.h"

#include "Logging/Log.h"

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
  pauseOnFocusLoss =
    toml::find_or<bool>(value, "PauseOnFocusLoss", pauseOnFocusLoss);
  gpuId = toml::find_or<s32&>(value, "GPU", gpuId);
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
  value["PauseOnFocusLoss"].comments().clear();
  value["PauseOnFocusLoss"] = pauseOnFocusLoss;
  value["PauseOnFocusLoss"].comments().push_back("# Pauses XeLL and GUI rendering on window focus loss");
  value["GPU"].comments().clear();
  value["GPU"].comments().push_back("# Chooeses which GPU to use if there are multiple (Vulkan/DirectX only)");
  value["GPU"] = gpuId;
}
bool _rendering::verify_toml(toml::value &value) {
  to_toml(value);
  cache_value(enable);
  cache_value(enableGui);
  cache_value(window);
  cache_value(isFullscreen);
  cache_value(vsync);
  cache_value(quitOnWindowClosure);
  cache_value(pauseOnFocusLoss);
  from_toml(value);
  verify_value(enable);
  verify_value(enableGui);
  verify_value(window.width);
  verify_value(window.height);
  verify_value(isFullscreen);
  verify_value(vsync);
  verify_value(quitOnWindowClosure);
  verify_value(pauseOnFocusLoss);
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
  haltOnAddress = toml::find_or<u64&>(value, "HaltOnAddress", haltOnAddress);
  haltOnSlbMiss = toml::find_or<bool>(value, "HaltOnSLBMiss", haltOnSlbMiss);
  haltOnExceptions = toml::find_or<bool>(value, "HaltOnExceptions", haltOnExceptions);
  startHalted = toml::find_or<bool>(value, "StartHalted", startHalted);
  softHaltOnAssertions = toml::find_or<bool>(value, "SoftHaltOnAssertions", softHaltOnAssertions);
  haltOnInvalidInstructions = toml::find_or<bool>(value, "HaltOnInvalidInstructions", haltOnInvalidInstructions);
}
void _debug::to_toml(toml::value &value) {
  value["HaltOnRead"].comments().clear();
  value["HaltOnRead"] = haltOnReadAddress;
  value["HaltOnRead"].as_integer_fmt().fmt = toml::integer_format::hex;
  value["HaltOnRead"].comments().push_back("# Address to halt on when the MMU reads from this address");
  value["HaltOnWrite"].comments().clear();
  value["HaltOnWrite"] = haltOnWriteAddress;
  value["HaltOnWrite"].as_integer_fmt().fmt = toml::integer_format::hex;
  value["HaltOnWrite"].comments().push_back("# Address to halt on when the MMU writes to this address");
  value["HaltOnAddress"].comments().clear();
  value["HaltOnAddress"] = haltOnAddress;
  value["HaltOnAddress"].as_integer_fmt().fmt = toml::integer_format::hex;
  value["HaltOnAddress"].comments().push_back("# Address to halt on when the CPU executes this address");
  value["HaltOnExceptions"].comments().clear();
  value["HaltOnExceptions"] = haltOnExceptions;
  value["HaltOnExceptions"].comments().push_back("# Halts on every exception (TODO: Separate toggles)");
  value["HaltOnSLBMiss"].comments().clear();
  value["HaltOnSLBMiss"] = haltOnSlbMiss;
  value["HaltOnSLBMiss"].comments().push_back("# Halts when a SLB cache misses");
  value["StartHalted"].comments().clear();
  value["StartHalted"] = startHalted;
  value["StartHalted"].comments().push_back("# Starts with the CPU halted");
  value["SoftHaltOnAssertions"].comments().clear();
  value["SoftHaltOnAssertions"] = softHaltOnAssertions;
  value["SoftHaltOnAssertions"].comments().push_back("# Soft-halts on asserts, in cases like implemented instructions");
  value["SoftHaltOnAssertions"].comments().push_back("# Disabling this causes assertions to do nothing");
  value["HaltOnInvalidInstructions"].comments().clear();
  value["HaltOnInvalidInstructions"] = haltOnInvalidInstructions;
  value["HaltOnInvalidInstructions"].comments().push_back("# Halts the PPU core on invalid instructions");
}
bool _debug::verify_toml(toml::value &value) {
  to_toml(value);
  cache_value(haltOnReadAddress);
  cache_value(haltOnWriteAddress);
  cache_value(haltOnAddress);
  cache_value(haltOnExceptions);
  cache_value(haltOnSlbMiss);
  cache_value(startHalted);
  cache_value(softHaltOnAssertions);
  cache_value(haltOnInvalidInstructions);
  from_toml(value);
  verify_value(haltOnReadAddress);
  verify_value(haltOnWriteAddress);
  verify_value(haltOnAddress);
  verify_value(haltOnExceptions);
  verify_value(haltOnSlbMiss);
  verify_value(startHalted);
  verify_value(softHaltOnAssertions);
  verify_value(haltOnInvalidInstructions);
  return true;
}

void _smc::from_toml(const toml::value &value) {
  avPackType = toml::find_or<s32&>(value, "AvPackType", avPackType);
  powerOnReason = toml::find_or<s32&>(value, "PowerOnType", powerOnReason);
  uartSystem = toml::find_or<std::string>(value, "UARTSystem", uartSystem);
#ifdef _WIN32
  comPort = toml::find_or<s32&>(value, "COMPort", comPort);
#endif
  socketIp = toml::find_or<std::string>(value, "SocketIP", socketIp);
  socketPort = toml::find_or<u16&>(value, "SocketPort", socketPort);
}
void _smc::to_toml(toml::value &value) {
  value["AvPackType"].comments().clear();
  value["AvPackType"] = avPackType;
  value["AvPackType"].comments().push_back("# The current connected AV Pack");
  value["AvPackType"].comments().push_back("# Default value is 31 (HDMI_NO_AUDIO) = 1280*720");
  value["AvPackType"].comments().push_back("# Lowest value is 87 (COMPOSITE) = 640*480");
  value["AvPackType"].comments().push_back("# The window size must never be smaller than the internal resolution");
  value["PowerOnType"].comments().clear();
  value["PowerOnType"] = powerOnReason;
  value["PowerOnType"].comments().push_back("# SMC power-up type/cause (Power Button, Eject Button, etc...)");
  value["PowerOnType"].comments().push_back("# 17: Console is being powered by a Power button press");
  value["PowerOnType"].comments().push_back("# 18: Console is being powered by an Eject button press");
  value["PowerOnType"].comments().push_back("# When trying to boot Linux/XeLL Reloaded this must be set to 18");
  value["UARTSystem"].comments().clear();
  value["UARTSystem"] = uartSystem;
  value["UARTSystem"].comments().push_back("# UART System");
  value["UARTSystem"].comments().push_back("# vcom is vCOM, only present on Windows");
  value["UARTSystem"].comments().push_back("# socket is Socket, avaliable via Netcat/Socat (netcat -lvnp 7000)");
  value["UARTSystem"].comments().push_back("# print is Printf, directly to log");
#ifdef _WIN32
  value["COMPort"].comments().clear();
  value["COMPort"] = comPort;
  value["COMPort"].comments().push_back("# Virtual COM port or Loopback COM device used for UART");
  value["COMPort"].comments().push_back("# Do not modify if you do not have a Virtual COM driver");
  value["COMPort"].comments().push_back("# Modify UARTSystem to use 'print' if you do not have netcat (a socket listener), otherwise use 'socket' (netcat -lvnp 7000)");
#endif
  value["SocketIP"].comments().clear();
  value["SocketIP"] = socketIp;
  value["SocketIP"].comments().push_back("# Socket IP, which IP the UART netcat/socat implementation listens for");
  value["SocketPort"].comments().clear();
  value["SocketPort"] = socketPort;
  value["SocketPort"].comments().push_back("# Socket Port, which port the UART netcat/socat implementation listens for");
}
bool _smc::verify_toml(toml::value &value) {
  to_toml(value);
  cache_value(avPackType);
  cache_value(powerOnReason);
  cache_value(uartSystem);
#ifdef _WIN32
  cache_value(comPort);
#endif
  cache_value(socketIp);
  cache_value(socketPort);
  from_toml(value);
  verify_value(avPackType);
  verify_value(powerOnReason);
#ifdef _WIN32
  verify_value(comPort);
#endif
  verify_value(socketIp);
  verify_value(socketPort);
  return true;
}

void _xcpu::from_toml(const toml::value &value) {
  elfLoader = toml::find_or<bool>(value, "ElfLoader", elfLoader);
  clocksPerInstruction = toml::find_or<s32&>(value, "CPI", clocksPerInstruction);
  skipHWInit = toml::find_or<bool>(value, "SkipHWInit", skipHWInit);
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

  value["SkipHWInit"].comments().clear();
  value["SkipHWInit"].comments().push_back("Enable CB/SB HW_INIT stage skip (Hack)");
  value["SkipHWInit"] = skipHWInit;

  value["HW_INIT_SKIP1"].comments().clear();
  value["HW_INIT_SKIP1"] = HW_INIT_SKIP_1;
  value["HW_INIT_SKIP1"].as_integer_fmt().fmt = toml::integer_format::hex;
  value["HW_INIT_SKIP1"].comments().push_back("# Manual Hardware Init Skip address 1 override");
  value["HW_INIT_SKIP1"].comments().push_back("# RGH3 Trinity: 0x3003F48");
  value["HW_INIT_SKIP1"].comments().push_back("# RGH3 Corona:  0x3003DC0");

  value["HW_INIT_SKIP2"].comments().clear();
  value["HW_INIT_SKIP2"] = HW_INIT_SKIP_2;
  value["HW_INIT_SKIP2"].as_integer_fmt().fmt = toml::integer_format::hex;

  value["HW_INIT_SKIP2"].comments().push_back("# Manual Hardware Init Skip address 2 override");
  value["HW_INIT_SKIP2"].comments().push_back("# RGH3 Trinity: 0x3003FDC");
  value["HW_INIT_SKIP2"].comments().push_back("# RGH3 Corona:  0x3003E54");
}
bool _xcpu::verify_toml(toml::value &value) {
  to_toml(value);
  cache_value(elfLoader);
  cache_value(clocksPerInstruction);
  cache_value(skipHWInit);
  cache_value(HW_INIT_SKIP_1);
  cache_value(HW_INIT_SKIP_2);
  from_toml(value);
  verify_value(elfLoader);
  verify_value(clocksPerInstruction);
  verify_value(skipHWInit);
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
  value.comments().clear();
  value.comments().push_back("# Only Fuses, OneBL, and Nand are required.");
  value.comments().push_back("# ElfBinary is used in the elf loader");
  value.comments().push_back("# ODDImage is Optical Disc Drive Image, takes a iso file for Linux");
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
  debugOnly = toml::find_or<bool>(value, "EnableDebugOnly", debugOnly);
  simpleDebugLog = toml::find_or<bool>(value, "SimpleDebugLog", simpleDebugLog);
  currentLevel = static_cast<Base::Log::Level>(tmpLevel);
}
void _log::to_toml(toml::value &value) {
  s32 tmpLevel = static_cast<s32>(currentLevel);
  value["Level"].comments().clear();
  value["Level"] = tmpLevel;
  value["Level"].comments().push_back("# Controls the current output filter level");
  value["Level"].comments().push_back("# 0: Trace | 1: Debug | 2: Info | 3: Warning | 4: Error | 5: Critical | 6: Guest | 7: Count");
  value["Advanced"].comments().clear();
  value["Advanced"] = advanced;
  value["Advanced"].comments().push_back("# Show more details on the log (ex, debug symbols)");
  value["EnableDebugOnly"].comments().clear();
  value["EnableDebugOnly"] = debugOnly;
  value["EnableDebugOnly"].comments().push_back("# Debug-only log options (Note: Floods the log and shows trace log options)");
  value["SimpleDebugLog"].comments().clear();
  value["SimpleDebugLog"] = simpleDebugLog;
  value["SimpleDebugLog"].comments().push_back("# Disables SoC prints, and other 'spam' debug statements");
}
bool _log::verify_toml(toml::value &value) {
  to_toml(value);
  cache_value(currentLevel);
  cache_value(advanced);
  cache_value(debugOnly);
  cache_value(simpleDebugLog);
  from_toml(value);
  verify_value(currentLevel);
  verify_value(advanced);
  verify_value(debugOnly);
  verify_value(simpleDebugLog);
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
#define write_section(x, n) x.to_toml(data[#n])
#define read_section(x, n) \
  if (data.contains(#n)) { \
    const toml::value &x ## _ = data.at(#n); \
    x.from_toml(x ## _); \
  }
bool verifyConfig(const std::filesystem::path &path, toml::value &data) {
#ifndef NO_GFX
  verify_section(rendering, Rendering);
  verify_section(imgui, ImGui);
#endif
  verify_section(smc, SMC);
  verify_section(xcpu, XCPU);
  verify_section(xgpu, XGPU);
  verify_section(filepaths, Paths);
  verify_section(debug, Debug);
  verify_section(log, Log);
  verify_section(highlyExperimental, HighlyExperimental);
  return true;
}

void loadConfig(const std::filesystem::path &path) {
  // If the configuration file does not exist, create it and return.
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
  read_section(rendering, Rendering);
  read_section(imgui, ImGui);
#endif
  read_section(smc, SMC);
  read_section(xcpu, XCPU);
  read_section(xgpu, XGPU);
  read_section(filepaths, Paths);
  read_section(debug, Debug);
  read_section(log, Log);
  read_section(highlyExperimental, HighlyExperimental);
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
      std::filesystem::remove(path);
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
