/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "PathUtil.h"

#include "Base/Logging/Log.h"

#include <unordered_map>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#endif // _WIN32
#ifdef __APPLE__
#include <unistd.h>
#include <libproc.h>
#endif // __APPLE__

namespace Base {

namespace FS {

static fs::path GetEnvPath(const char *name) {
  const char *value = std::getenv(name);
  return (value && *value) ? fs::path(value) : fs::path{};
}

#ifdef _WIN32
static fs::path GetKnownFolder(REFKNOWNFOLDERID id) {
  PWSTR wide_path = nullptr;
  HRESULT hr = SHGetKnownFolderPath(id, KF_FLAG_CREATE, nullptr, &wide_path);
  if (FAILED(hr) || !wide_path) {
    throw std::runtime_error("SHGetKnownFolderPath failed");
  }

  fs::path result(wide_path);
  CoTaskMemFree(wide_path);
  return result;
}
#endif

const fs::path GetBinaryDirectory() {
  fs::path exe_path;

#ifdef _WIN32
  wchar_t path[MAX_PATH];
  DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
  if (len == 0 || len == MAX_PATH) {
    throw std::runtime_error("GetModuleFileNameW failed");
  }
  exe_path = fs::path(path);

#elif __linux__
  exe_path = fs::canonical("/proc/self/exe");

#elif __APPLE__
  pid_t pid = getpid();
  char path[PROC_PIDPATHINFO_MAXSIZE] = {};
  int ret = proc_pidpath(pid, path, sizeof(path));
  if (ret <= 0) {
    throw std::runtime_error("proc_pidpath failed");
  }
  exe_path = fs::path(path);

#else
  exe_path = fs::current_path() / APP_NAME;
#endif

  return fs::weakly_canonical(exe_path.parent_path());
}

static void EnsureDir(const fs::path &p) {
  std::error_code ec;
  fs::create_directories(p, ec);
  if (ec) {
    throw std::runtime_error(FMT("Failed to create directory '{}': {}",
      p.string(), ec.message()));
  }
}

static auto Paths = [] {
  std::unordered_map<PathType, fs::path> paths{};

  const auto insert_path = [&](PathType type, const fs::path& path, bool create = true) {
    if (create) {
      EnsureDir(path);
    }
    paths.insert_or_assign(type, path);
  };

  insert_path(PathType::BinaryDir, GetBinaryDirectory(), false);

#ifdef _WIN32
  fs::path roaming = GetKnownFolder(FOLDERID_RoamingAppData) / APP_NAME;
  fs::path local = GetKnownFolder(FOLDERID_LocalAppData) / APP_NAME;

  fs::path configDir = roaming;
  fs::path dataDir = roaming;
  fs::path cacheDir  = local / "Cache";
  fs::path stateDir = local / "State";
  fs::path logDir = local / "Logs";

#elif defined(__APPLE__)
  fs::path home = GetEnvPath("HOME");
  if (home.empty()) {
    throw std::runtime_error("HOME not set");
  }

  fs::path appSupport = home / "Library" / "Application Support" / APP_NAME;
  fs::path cacheDir = home / "Library" / "Caches" / APP_NAME;
  fs::path logDir = home / "Library" / "Logs" / APP_NAME;

  fs::path configDir = appSupport;
  fs::path dataDir = appSupport;
  fs::path stateDir = appSupport / "State";
#else
  fs::path home = GetEnvPath("HOME");
  if (home.empty()) {
    throw std::runtime_error("HOME not set");
  }

  fs::path configBase = GetEnvPath("XDG_CONFIG_HOME");
  fs::path dataBase = GetEnvPath("XDG_DATA_HOME");
  fs::path cacheBase = GetEnvPath("XDG_CACHE_HOME");
  fs::path stateBase = GetEnvPath("XDG_STATE_HOME");

  if (configBase.empty()) configBase = home / ".config";
  if (dataBase.empty()) dataBase = home / ".local" / "share";
  if (cacheBase.empty()) cacheBase = home / ".cache";
  if (stateBase.empty()) stateBase = home / ".local" / "state";

  fs::path configDir = configBase / "xenon";
  fs::path dataDir = dataBase / "xenon";
  fs::path cacheDir = cacheBase / "xenon";
  fs::path stateDir = stateBase / "xenon";
  fs::path logDir = stateDir / LOG_DIR;
#endif

  insert_path(PathType::UserConfigDir, configDir);
  insert_path(PathType::UserDataDir, dataDir);
  insert_path(PathType::UserCacheDir, cacheDir);
  insert_path(PathType::UserStateDir, stateDir);
  insert_path(PathType::LogDir, logDir);

  insert_path(PathType::ConsoleDir, dataDir / CONSOLE_DIR);

  insert_path(PathType::ShaderDir, dataDir / SHADER_DIR);
  insert_path(PathType::ShaderCacheDir, cacheDir / SHADER_DIR);
  insert_path(PathType::ShaderSpirvDir, dataDir / SHADER_DIR / "spirv");
  insert_path(PathType::ShaderOpenGLDir, dataDir / SHADER_DIR / "opengl");
  insert_path(PathType::ShaderVulkanDir, dataDir / SHADER_DIR / "vulkan");

  return paths;
}();

std::string PathToUTF8String(const fs::path &path) {
#ifdef _WIN32
  // On CXX 20+ u8string returns an std::u8string type, convert that back to std::string and return.
  std::u8string u8str = path.u8string();
  std::string utf8(u8str.begin(), u8str.end());
  return utf8;
#else
  return path.string();
#endif
}

const fs::path &GetPath(const PathType pathType) {
  return Paths.at(pathType);
}

std::string GetPathString(const PathType pathType) {
  return PathToUTF8String(GetPath(pathType));
}

std::vector<FileInfo> ListFilesFromPath(const fs::path &path) {
  std::vector<FileInfo> fileList;

  fs::path canonicalPath = fs::weakly_canonical(path);

  for (const auto& entry : fs::directory_iterator{ canonicalPath }) {
    FileInfo fileInfo{};

    if (entry.is_directory()) {
      fileInfo.fileSize = 0;
      fileInfo.fileType = FileType::Directory;
    } else {
      fileInfo.fileSize = fs::file_size(entry.path());
      fileInfo.fileType = FileType::File;
    }

    fileInfo.filePath = entry.path();
    fileInfo.fileName = entry.path().filename();
    fileList.push_back(std::move(fileInfo));
  }

  return fileList;
}

void SetPath(const PathType pathType, const fs::path &newPath) {
  EnsureDir(newPath);
  Paths.insert_or_assign(pathType, newPath);
}

void DumpPaths() {
  for (const auto &type : kAllPathTypes) {
    const auto &path = GetPath(type.pathType);
    LOG_INFO(System, "{:<22}: {}", type.humanName, PathToUTF8String(path));
  }
}

} // namespace FS

} // namespace Base