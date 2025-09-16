/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "PathUtil.h"
#include "Base/Logging/Log.h"

#include <fmt/format.h>

#include <unordered_map>
#include <fstream>
#ifdef _WIN32
#include <Windows.h>
#endif // _WIN32
#ifdef __APPLE__
#include <unistd.h>
#include <libproc.h>
#endif // __APPLE__

namespace Base {
namespace FS {

const fs::path GetBinaryDirectory() {
  fs::path fspath = {};
#ifdef _WIN32
  char path[256];
  GetModuleFileNameA(nullptr, path, sizeof(path));
  fspath = path;
#elif __linux__
  fspath = fs::canonical("/proc/self/exe");
#elif __APPLE__
  pid_t pid = getpid();
  char path[PROC_PIDPATHINFO_MAXSIZE];
  // While this is fine for a raw executable,
  // an application bundle is read-only and these files
  // should instead be placed in Application Support.
  proc_pidpath(pid, path, sizeof(path));
  fspath = path;
#else
  // Unknown, just return rootdir
  fspath = fs::current_path() / "Xenon";
#endif
  return fs::weakly_canonical(fmt::format("{}/..", fspath.string()));
}

static auto UserPaths = [] {
  auto currentDir = fs::current_path();
  auto binaryDir = GetBinaryDirectory();
  bool nixos = false;
#ifdef _WIN32
  const char *appdata = std::getenv("APPDATA");
  if (!appdata) {
    throw std::runtime_error("APPDATA not set");
  }
#else
  const char *home = std::getenv("HOME");
  if (!home) {
    throw std::runtime_error("HOME not set");
  }
  fs::path configDir(fs::path(home) / ".local" / "share");
#endif

  std::unordered_map<PathType, fs::path> paths{};

  const auto insert_path = [&](PathType xenon_path, const fs::path &new_path, bool create = true) {
    if (create && !fs::exists(new_path))
      fs::create_directory(new_path);

    paths.insert_or_assign(xenon_path, new_path);
  };

  configDir /= "Xenon";
  insert_path(PathType::BinaryDir, binaryDir, false);
  insert_path(PathType::RootDir, currentDir, false);
  insert_path(PathType::ConsoleDir, configDir / CONSOLE_DIR);
  insert_path(PathType::LogDir, configDir / LOG_DIR);
  insert_path(PathType::ShaderDir, configDir / SHADER_DIR);
  fs::create_directory(configDir / SHADER_DIR / "cache");
  fs::create_directory(configDir / SHADER_DIR / "spirv");
  fs::create_directory(configDir / SHADER_DIR / "opengl");
  fs::create_directory(configDir / SHADER_DIR / "vulkan");
  return paths;
}();

std::string PathToUTF8String(const fs::path &path) {
  const auto u8_string = path.u8string();
  return std::string{u8_string.begin(), u8_string.end()};
}

const fs::path &GetUserPath(PathType xenon_path) {
  return UserPaths.at(xenon_path);
}

std::string GetUserPathString(PathType xenon_path) {
  return PathToUTF8String(GetUserPath(xenon_path));
}

std::vector<FileInfo> ListFilesFromPath(const fs::path &path) {
  std::vector<FileInfo> fileList;

  fs::path _path = fs::weakly_canonical(path);

  for (auto &entry : fs::directory_iterator{ _path }) {
    FileInfo fileInfo;
    if (entry.is_directory()) {
      fileInfo.fileSize = 0;
      fileInfo.fileType = FileType::Directory;
    } else {
      fileInfo.fileSize = fs::file_size(_path);
      fileInfo.fileType = FileType::File;
    }

    fileInfo.filePath = entry.path();
    fileInfo.fileName = entry.path().filename();
    fileList.push_back(fileInfo);
  }

    return fileList;
}

void SetUserPath(PathType xenon_path, const fs::path &new_path) {
  UserPaths.insert_or_assign(xenon_path, new_path);
}
} // namespace FS
} // namespace Base
