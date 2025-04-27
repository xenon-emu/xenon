// Copyright 2025 Xenon Emulator Project

#pragma once

#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace Base::FS {

enum class PathType {
  BinaryDir,  // Binary Path
  RootDir,    // Execution Path
  ConsoleDir, // Where Xenon gets the console files
  LogDir,     // Where log files are stored
  FontDir,    // Where fonts are stored
  RDocDir,    // RenderDoc Path
};

enum FileType {
  Directory,
  File
};

// Represents a given file inside a directory.
typedef struct _FileInfo {
  fs::path fileName;  // The file name and extension
  fs::path filePath;  // The file path
  size_t fileSize;    // File size
  FileType fileType;  // File Type (directory/file)
} FileInfo;

constexpr auto CONSOLE_DIR = "console";

constexpr auto LOG_DIR = "log";

constexpr auto FONT_DIR = "fonts";

constexpr auto RDOC_DIR = "RenderDoc";

constexpr auto LOG_FILE = "xenon_log.txt";

// Converts a given fs::path to a UTF8 string.
[[nodiscard]] std::string PathToUTF8String(const fs::path &path);

// Returns a fs::path object containing the current 'User' path.
[[nodiscard]] const fs::path &GetUserPath(PathType user_path);

// Returns a string containing the current 'User' path.
[[nodiscard]] std::string GetUserPathString(PathType user_path);

// Returns a container with a list of the files inside the specified path.
[[nodiscard]] std::vector<FileInfo> ListFilesFromPath(const fs::path &path);

// Sets the current Path for a given PathType.
void SetUserPath(PathType user_path, const fs::path &new_path);

} // namespace Base::FS
