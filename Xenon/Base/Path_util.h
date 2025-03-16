// Copyright 2025 Xenon Emulator Project

#pragma once

#include <filesystem>

namespace Base::FS {

namespace fs = std::filesystem;

enum class PathType {
  RootDir,    // Execution Path
  ConsoleDir, // Where Xenon gets the console files
  LogDir,     // Where log files are stored
  FontDir     // Where fonts are stored
};

constexpr auto CONSOLE_DIR = "console";

constexpr auto LOG_DIR = "log";

constexpr auto FONT_DIR = "fonts";

constexpr auto LOG_FILE = "xenon_log.txt";

[[nodiscard]] std::string PathToUTF8String(const fs::path &path);

[[nodiscard]] const fs::path &GetUserPath(PathType user_path);

[[nodiscard]] std::string GetUserPathString(PathType user_path);

void SetUserPath(PathType user_path, const fs::path &new_path);

} // namespace Base::FS
