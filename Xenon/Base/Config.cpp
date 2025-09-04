// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Config.h"
#include "PathUtil.h"
#include "StringUtil.h"

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
  quitOnWindowClosure =toml::find_or<bool>(value, "QuitOnWindowClosure", quitOnWindowClosure);
  pauseOnFocusLoss = toml::find_or<bool>(value, "PauseOnFocusLoss", pauseOnFocusLoss);
  gpuId = toml::find_or<s32&>(value, "GPU", gpuId);
  backend = toml::find_or<std::string>(value, "Backend", backend);
  debugValidation = toml::find_or<bool>(value, "DebugValidation", debugValidation);
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
  value["GPU"] = gpuId;
  value["GPU"].comments().push_back("# Chooses which GPU to use if there are multiple (Vulkan/DirectX only)");
  value["Backend"].comments().clear();
  value["Backend"] = backend;
  value["Backend"].comments().push_back("# Graphics API used for rendering (OpenGL, Vulkan & Dummy)");
  value["DebugValidation"].comments().clear();
  value["DebugValidation"] = debugValidation;
  value["DebugValidation"].comments().push_back("# Graphics API Validation");
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
  cache_value(gpuId);
  cache_value(backend);
  cache_value(debugValidation);
  from_toml(value);
  verify_value(enable);
  verify_value(enableGui);
  verify_value(window.width);
  verify_value(window.height);
  verify_value(isFullscreen);
  verify_value(vsync);
  verify_value(quitOnWindowClosure);
  verify_value(pauseOnFocusLoss);
  verify_value(gpuId);
  verify_value(backend);
  verify_value(debugValidation);
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
  haltOnGuestAssertion = toml::find_or<bool>(value, "HaltOnGuestAssertion", haltOnGuestAssertion);
  autoContinueOnGuestAssertion = toml::find_or<bool>(value, "AutoContinueOnGuestAssertion", autoContinueOnGuestAssertion);
#ifdef DEBUG_BUILD
  createTraceFile = toml::find_or<bool>(value, "CreateTraceFile", createTraceFile);
#endif
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
  value["HaltOnSLBMiss"].comments().clear();
  value["HaltOnSLBMiss"] = haltOnSlbMiss;
  value["HaltOnSLBMiss"].comments().push_back("# Halts when a SLB cache misses");
  value["HaltOnExceptions"].comments().clear();
  value["HaltOnExceptions"] = haltOnExceptions;
  value["HaltOnExceptions"].comments().push_back("# Halts on every exception (TODO: Separate toggles)");
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
  value["HaltOnGuestAssertion"].comments().clear();
  value["HaltOnGuestAssertion"] = haltOnGuestAssertion;
  value["HaltOnGuestAssertion"].comments().push_back("# Halts whenever a guest causes a TRAP opcode for asserting");
  value["AutoContinueOnGuestAssertion"].comments().clear();
  value["AutoContinueOnGuestAssertion"] = autoContinueOnGuestAssertion;
  value["AutoContinueOnGuestAssertion"].comments().push_back("# Automatically continues on guest assertion");
#ifdef DEBUG_BUILD
  value["CreateTraceFile"].comments().clear();
  value["CreateTraceFile"] = createTraceFile;
  value["CreateTraceFile"].comments().push_back("# Creates a trace file with every single jump/bc opcode");
  value["CreateTraceFile"].comments().push_back("# Note: This can create an log file of up to 20Gb without any limit");
#endif
}
bool _debug::verify_toml(toml::value &value) {
  to_toml(value);
  cache_value(haltOnReadAddress);
  cache_value(haltOnWriteAddress);
  cache_value(haltOnAddress);
  cache_value(haltOnSlbMiss);
  cache_value(haltOnExceptions);
  cache_value(startHalted);
  cache_value(softHaltOnAssertions);
  cache_value(haltOnInvalidInstructions);
  cache_value(haltOnGuestAssertion);
  cache_value(autoContinueOnGuestAssertion);
#ifdef DEBUG_BUILD
  cache_value(createTraceFile);
#endif
  from_toml(value);
  verify_value(haltOnReadAddress);
  verify_value(haltOnWriteAddress);
  verify_value(haltOnAddress);
  verify_value(haltOnSlbMiss);
  verify_value(haltOnExceptions);
  verify_value(startHalted);
  verify_value(softHaltOnAssertions);
  verify_value(haltOnInvalidInstructions);
  verify_value(haltOnGuestAssertion);
  verify_value(autoContinueOnGuestAssertion);
#ifdef DEBUG_BUILD
  verify_value(createTraceFile);
#endif
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
  // Ensure it's lowercase
  uartSystem = Base::ToLower(uartSystem);
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
  value["UARTSystem"].comments().push_back("# socket is Socket, available via Netcat/Socat (ex, nc64 -Lp 7000)");
  value["UARTSystem"].comments().push_back("# print is Printf, directly to log");
  value["UARTSystem"].comments().push_back("# null is no UART driver");
#ifdef _WIN32
  value["COMPort"].comments().clear();
  value["COMPort"] = comPort;
  value["COMPort"].comments().push_back("# Virtual COM port or Loopback COM device used for UART");
  value["COMPort"].comments().push_back("# Do not modify if you do not have a Virtual COM driver");
  value["COMPort"].comments().push_back("# Modify UARTSystem to use 'socket', or use 'print' if you do not have a socket listener");
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
  verify_value(uartSystem);
#ifdef _WIN32
  verify_value(comPort);
#endif
  verify_value(socketIp);
  verify_value(socketPort);
  return true;
}

void _xcpu::from_toml(const toml::value &value) {
  ramSize = toml::find_or<std::string>(value, "RAMSize", ramSize);
  elfLoader = toml::find_or<bool>(value, "ElfLoader", elfLoader);
  clocksPerInstruction = toml::find_or<s32&>(value, "CPI", clocksPerInstruction);
  overrideInitSkip = toml::find_or<bool>(value, "OverrideHWInit", overrideInitSkip);
  HW_INIT_SKIP_1 = toml::find_or<u64&>(value, "HW_INIT_SKIP1", HW_INIT_SKIP_1);
  HW_INIT_SKIP_2 = toml::find_or<u64&>(value, "HW_INIT_SKIP2", HW_INIT_SKIP_2);
  simulate1BL = toml::find_or<bool>(value, "Simulate1BL", simulate1BL);
  runInstrTests = toml::find_or<bool>(value, "RunInstrTests", runInstrTests);
  instrTestsMode = toml::find_or<u8&>(value, "InstrTestsMode", instrTestsMode);
}
void _xcpu::to_toml(toml::value &value) {
  value["RAMSize"].comments().clear();
  value["RAMSize"] = ramSize;
  value["RAMSize"].comments().push_back("# CPU RAM Size");
  value["RAMSize"].comments().push_back("# Supports Bytes, (Kilobytes, Kibibytes), (Megabytes, Mebibytes), and (Gigabytes, Gibibytes)");
  value["RAMSize"].comments().push_back("# 512MiB = 536.870912MB");
  value["RAMSize"].comments().push_back("# 1GiB = 1024MiB");

  value["ElfLoader"].comments().clear();
  value["ElfLoader"] = elfLoader;
  value["ElfLoader"].comments().push_back("# Disables normal codeflow and loads an elf from ElfBinary");

  value["CPI"].comments().clear();
  value["CPI"] = clocksPerInstruction;
  value["CPI"].comments().push_back("# [DO NOT MODIFY] Clocks Per Instruction [DO NOT MODIFY]");
  value["CPI"].comments().push_back("# If your system has a lower than average CPI, use CPI Bypass in HighlyExperimental");
  value["CPI"].comments().push_back("# Note: This will mess with execution timing and may break time-sensitive things like XeLL");

  value["OverrideHWInit"].comments().clear();
  value["OverrideHWInit"] = overrideInitSkip;
  value["OverrideHWInit"].comments().push_back("# Uses manual init skips below if true, otherwise, it uses the auto-detected values");

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

  value["Simulate1BL"].comments().clear();
  value["Simulate1BL"] = simulate1BL;
  value["Simulate1BL"].comments().push_back("# Simulates the behavior of the 1BL inside the XCPU. Allows for bootup without said binary being required.");
  value["Simulate1BL"].comments().push_back("# Currently WIP, do not use.");

  value["RunInstrTests"].comments().clear();
  value["RunInstrTests"] = runInstrTests;
  value["RunInstrTests"].comments().push_back("# Runs a set of PPC instruction tests derived from the Xenia Project tests.");
  value["RunInstrTests"].comments().push_back("# See their README on how to generate the tests. Not meant for end users.");
  value["RunInstrTests"].comments().clear();
  value["InstrTestsMode"] = instrTestsMode;
  value["RunInstrTests"].comments().push_back("# Specifies the backend to test.");
  value["RunInstrTests"].comments().push_back("# 0 = Interpreter, 1 = JITx86.");
}
bool _xcpu::verify_toml(toml::value &value) {
  to_toml(value);
  cache_value(ramSize);
  cache_value(elfLoader);
  cache_value(clocksPerInstruction);
  cache_value(overrideInitSkip);
  cache_value(HW_INIT_SKIP_1);
  cache_value(HW_INIT_SKIP_2);
  cache_value(simulate1BL);
  cache_value(runInstrTests);
  cache_value(instrTestsMode);
  from_toml(value);
  verify_value(ramSize);
  verify_value(elfLoader);
  verify_value(clocksPerInstruction);
  verify_value(overrideInitSkip);
  verify_value(HW_INIT_SKIP_1);
  verify_value(HW_INIT_SKIP_2);
  verify_value(simulate1BL);
  verify_value(runInstrTests);
  verify_value(instrTestsMode);
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
  hddImage = toml::find_or<std::string>(value, "HDDImage", hddImage);
  elfBinary = toml::find_or<std::string>(value, "ElfBinary", elfBinary);
  instrTestsPath = toml::find_or<std::string>(value, "InstrTestsPath", instrTestsPath);
  instrTestsBinPath = toml::find_or<std::string>(value, "InstrTestsBinPath", instrTestsBinPath);
}
void _filepaths::to_toml(toml::value &value) {
  value.comments().clear();
  value.comments().push_back("# Only Fuses, OneBL, and Nand are required");
  value.comments().push_back("# ElfBinary is used in the elf loader");
  value.comments().push_back("# ODDImage is Optical Disc Drive Image, takes an ISO file for Linux");
  value.comments().push_back("# HDDImage is the Hard Drive Disc Image, takes an Xbox360 Formatted (FATX) HDD image for the Xbox System/Linux storage purposes");
  value.comments().push_back("# InstrTestsPath is the base path for instruction test files (.s) for use in the test runner");
  value.comments().push_back("# InstrTestsBinPath is the path for the generated binary instruction test files (.bin)");
  value["Fuses"] = fuses;
  value["OneBL"] = oneBl;
  value["Nand"] = nand;
  value["ODDImage"] = oddImage;
  value["HDDImage"] = hddImage;
  value["ElfBinary"] = elfBinary;
  value["InstrTestsPath"] = instrTestsPath;
  value["InstrTestsBinPath"] = instrTestsBinPath;
}
bool _filepaths::verify_toml(toml::value &value) {
  to_toml(value);
  cache_value(fuses);
  cache_value(oneBl);
  cache_value(nand);
  cache_value(oddImage);
  cache_value(hddImage);
  cache_value(elfBinary);
  cache_value(instrTestsPath);
  cache_value(instrTestsBinPath);
  from_toml(value);
  verify_value(fuses);
  verify_value(oneBl);
  verify_value(nand);
  verify_value(oddImage);
  verify_value(hddImage);
  verify_value(elfBinary);
  verify_value(instrTestsPath);
  verify_value(instrTestsBinPath);
  return true;
}

void _log::from_toml(const toml::value &value) {
  s32 tmpLevel = static_cast<s32>(currentLevel);
  tmpLevel = toml::find_or<s32&>(value, "Level", tmpLevel);
  currentLevel = static_cast<Base::Log::Level>(tmpLevel);
  type = toml::find_or<std::string>(value, "Type", type);
  advanced = toml::find_or<bool>(value, "Advanced", advanced);
#ifdef DEBUG_BUILD
  debugOnly = toml::find_or<bool>(value, "EnableDebugOnly", debugOnly);
#endif
  // Ensure it's lowercase
  type = Base::ToLower(type);
}
void _log::to_toml(toml::value &value) {
  value["Level"].comments().clear();
  value["Level"] = static_cast<s32>(currentLevel);
  value["Level"].comments().push_back("# Controls the current output filter level");
  value["Level"].comments().push_back("# 0: Trace | 1: Debug | 2: Info | 3: Warning | 4: Error | 5: Critical | 6: Guest | 7: Count");
  value["Type"].comments().clear();
  value["Type"] = Base::ToLower(type);
  value["Type"].comments().push_back("# Determines how log is handled");
  value["Type"].comments().push_back("# Types:");
  value["Type"].comments().push_back("# async - (Recommended) Pushes to a queue and handles in a different thread");
  value["Type"].comments().push_back("# sync - Waits for the log to be completed in the same thread");
  value["Advanced"].comments().clear();
  value["Advanced"] = advanced;
  value["Advanced"].comments().push_back("# Show more details on the log (ex, debug symbols)");
#ifdef DEBUG_BUILD
  value["EnableDebugOnly"].comments().clear();
  value["EnableDebugOnly"] = debugOnly;
  value["EnableDebugOnly"].comments().push_back("# Debug-only log options (Note: Floods the log and shows trace log options)");
#endif
}
bool _log::verify_toml(toml::value &value) {
  to_toml(value);
  cache_value(currentLevel);
  cache_value(type);
  cache_value(advanced);
#ifdef DEBUG_BUILD
  cache_value(debugOnly);
#endif
  from_toml(value);
  verify_value(currentLevel);
  verify_value(type);
  verify_value(advanced);
#ifdef DEBUG_BUILD
  verify_value(debugOnly);
#endif
  return true;
}

void _highlyExperimental::from_toml(const toml::value &value) {
  s32 tmpConsoleRevison = static_cast<s32>(consoleRevison);
  tmpConsoleRevison = toml::find_or<s32&>(value, "ConsoleRevison", tmpConsoleRevison);
  consoleRevison = static_cast<eConsoleRevision>(tmpConsoleRevison);
  cpuExecutor = toml::find_or<std::string>(value, "CPUExecutor", cpuExecutor);
  clocksPerInstructionBypass = toml::find_or<s32&>(value, "CPIBypass", clocksPerInstructionBypass);
}
void _highlyExperimental::to_toml(toml::value &value) {
  value.comments().clear();
  value.comments().push_back("# Do not touch these options unless you know what you're doing!");
  value.comments().push_back("# It can break execution! User beware");
  value["ConsoleRevison"].comments().clear();
  value["ConsoleRevison"] = static_cast<u32>(consoleRevison);
  value["ConsoleRevison"].comments().push_back("# Console motherboard revision, used for PVR and XGPU Init");
  value["ConsoleRevison"].comments().push_back("# Xenon = 0 | Zephyr = 1 | Falcon = 2 | Jasper = 3 | Trinity = 4 | Corona = 5 | Corona 4GB = 6 | Winchester = 7");
  value["CPUExecutor"].comments().clear();
  value["CPUExecutor"] = cpuExecutor;
  value["CPUExecutor"].comments().push_back("# PowerPC CPU Executor:");
  value["CPUExecutor"].comments().push_back("# Interpreted - Cached Interpreter, uses regular interpreted execution with caching");
  value["CPUExecutor"].comments().push_back("# JIT - Just In Time compilation, runs opcodes in 'blocks'");
  value["CPUExecutor"].comments().push_back("# Hybrid - JIT with Cached Interpreter fallback, uses faster block system with Interpreter opcodes");
  value["CPUExecutor"].comments().push_back("# [WARN] This is unfinished, you *will* break the emulator changing this");
  value["CPIBypass"].comments().clear();
  value["CPIBypass"] = clocksPerInstructionBypass;
  value["CPIBypass"].comments().push_back("# Zero will use the estimated CPI for your system (view XCPU for more info)");
}
bool _highlyExperimental::verify_toml(toml::value &value) {
  to_toml(value);
  cache_value(consoleRevison);
  cache_value(cpuExecutor);
  cache_value(clocksPerInstructionBypass);
  from_toml(value);
  verify_value(consoleRevison);
  verify_value(cpuExecutor);
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
bool verifyConfig(const fs::path &path, toml::value &data) {
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

void loadConfig(const fs::path &path) {
  // If the configuration file does not exist, create it and return.
  std::ifstream configFile{ path };
  bool valid = configFile.is_open() && configFile.good();
  configFile.close();
  std::error_code error;
  if (!fs::exists(path, error) && !valid) {
    filepaths.correct(Base::FS::GetUserPath(Base::FS::PathType::ConsoleDir));
    saveConfig(path);
    return;
  }

  // Read file to data, then close
  toml::value data = toml::parse(path);

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

void saveConfig(const fs::path &path) {
  std::ifstream configFile{ path };
  bool valid = configFile.is_open() && configFile.good();
  configFile.close();
  std::error_code error{};
  if (!fs::exists(path, error) || !valid) {
    if (error) {
      LOG_ERROR(Config, "Filesystem error: {}", error.message());
    }
    LOG_INFO(Config, "Config not found! Saving new configuration file to {}", path.string());
  }

  // Read file to data, then close
  toml::value data = valid ? toml::parse(path) : toml::value{};

  std::string prevPath{ path.string() };
  std::string prevFile{ path.filename().string() };
  fs::path basePath{ prevPath.substr(0, prevPath.length() - prevFile.length() - 1) };
  // If we didn't have a config, just write directly. Nothing to save
  fs::path newPath{ valid ? basePath / (prevFile + ".tmp") : path };

  // If it wasn't valid, write before
  if (!valid) {
    // Write to toml, then verify contents
    if (!verifyConfig(newPath, data)) {
      return;
    }
  }

  // Write initial file
  try {
    std::ofstream file{ newPath };
    file << data;
    file.close();
  }
  catch (const std::exception &ex) {
    LOG_ERROR(Config, "Exception trying to write config. {}", ex.what());
    return;
  }

  // If it is, just ensure there is a backup config
  if (valid) {
    // Write to toml, then verify contents
    if (!verifyConfig(newPath, data)) {
      return;
    }
  }

  // Do the final write
  try {
    std::ofstream file{ newPath };
    file << data;
    file.close();
  }
  catch (const std::exception &ex) {
    LOG_ERROR(Config, "Exception trying to write config. {}", ex.what());
    return;
  }

  // Copy file config to current version, if it's valid
  if (valid) {
    try {
      std::error_code fsError;
      u64 fileSize = fs::file_size(newPath, fsError);
      if (fileSize != -1 && fileSize) {
        fs::rename(newPath, path);
      } else {
        fileSize = 0;
        if (fsError) {
          LOG_ERROR(Config, "Filesystem error: {} ({})", fsError.message(), fsError.value());
        }
      }
    } catch (const std::exception &ex) {
      LOG_ERROR(Config, "Exception trying to copy backup config. {}", ex.what());
      return;
    }
  }
}

} // namespace Config
