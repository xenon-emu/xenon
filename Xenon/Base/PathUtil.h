/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace Base {

namespace FS {

inline constexpr const char *APP_NAME = "Xenon";
inline constexpr const char *CONSOLE_DIR = "console";
inline constexpr const char *LOG_DIR = "logs";
inline constexpr const char *SHADER_DIR = "shaders";
inline constexpr const char *LOG_FILE = "xenon_log.txt";

enum class PathType {
  BinaryDir, // Where the Xenon executable resides

  UserConfigDir, // User configuration files (settings, configs)
                 // Linux:   ~/.config/xenon
                 // Windows: %AppData%/Xenon
                 // macOS:   ~/Library/Application Support/Xenon

  UserDataDir, // Persistent application data (saves, dumps, assets)
               // Linux:   ~/.local/share/xenon
               // Windows: %AppData%/Xenon
               // macOS:   ~/Library/Application Support/Xenon

  UserCacheDir, // Non-essential cache (rebuildable data, shader cache)
                // Linux:   ~/.cache/xenon
                // Windows: %localappdata%/Xenon/Cache
                // macOS:   ~/Library/Caches/Xenon

  UserStateDir, // Runtime state (session data, runtime-generated files)
                // Linux:   ~/.local/state/xenon
                // Windows: %localappdata%/Xenon/State
                // macOS:   ~/Library/Application Support/Xenon/State

  LogDir, // Log files (may be inside state or separate)
          // Linux:   ~/.local/state/xenon/log
          // Windows: %localappdata%/Xenon/Logs
          // macOS:   ~/Library/Logs/Xenon

  ConsoleDir, // Emulated console storage (NAND, profiles, title data)
              // Typically under UserDataDir

  ShaderDir, // Shader-related data (sources, dumps)
             // Typically under UserDataDir

  ShaderCacheDir, // Compiled shader cache (safe to delete)
                  // Typically under UserCacheDir

  ShaderSpirvDir, // SPIR-V output (debug/generated)
                  // Typically under ShaderDir

  ShaderOpenGLDir, // OpenGL output (debug/generated)
                   // Typically under ShaderDir

  ShaderVulkanDir, // Vulkan output (debug/generated)
                  // Typically under ShaderDir

  NoneDir // Mark end of enum, acts as a size and a default value for PathType
};

struct PathTypePair {
  const PathType pathType = PathType::NoneDir;
  const char *humanName = "";
};

static constexpr PathTypePair kAllPathTypes[] = {
  { PathType::BinaryDir, "Binary Directory" },
  { PathType::UserConfigDir, "User Config Directory" },
  { PathType::UserDataDir, "User Data Directory" },
  { PathType::UserCacheDir, "User Cache Directory" },
  { PathType::UserStateDir, "User State Directory" },
  { PathType::LogDir, "Log Directory" },
  { PathType::ConsoleDir, "Console Directory" },
  { PathType::ShaderDir, "Shader Directory" },
  { PathType::ShaderCacheDir, "Shader Cache Directory" },
  { PathType::ShaderSpirvDir, "Shader SPIR-V Directory" },
  { PathType::ShaderOpenGLDir, "Shader OpenGL Directory" },
  { PathType::ShaderVulkanDir, "Shader Vulkan Directory" }
};

enum class FileType {
  Directory,
  File
};

struct FileInfo {
  fs::path fileName; // The file name and extension
  fs::path filePath; // The file path
  size_t fileSize;   // File size
  FileType fileType; // File type (directory/file)
};

// Converts a given fs::path to a UTF8 string
[[nodiscard]] std::string PathToUTF8String(const fs::path &path);

// Returns a fs::path object containing the current path
[[nodiscard]] const fs::path &GetPath(const PathType pathType);

// Returns a string containing the current path
[[nodiscard]] std::string GetPathString(const PathType pathType);

// Returns a container with a list of the files inside the specified path
[[nodiscard]] std::vector<FileInfo> ListFilesFromPath(const fs::path &path);

// Sets the current Path for a given PathType
[[nodiscard]] void SetPath(const PathType pathType, const fs::path &newPath);

[[nodiscard]] void DumpPaths();

[[nodiscard]] constexpr const char *PathTypeToString(const PathType type) {
  for (const auto &path : kAllPathTypes) {
    if (path.pathType == type)
      return path.humanName;
  }

  return "Unknown Directory";
}

} // namespace FS

} // namespace Base