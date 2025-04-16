// Copyright 2025 Xenon Emulator Project

#include "Path_util.h"
#include "Base/Logging/Log.h"

#include <fmt/format.h>

#include <unordered_map>
#include <fstream>
#ifdef _WIN32
#include <Windows.h>
#endif // _WIN32

namespace Base::FS {

const fs::path GetBinaryDirectory() {
  fs::path fspath = {};
#ifdef _WIN32
  char path[256];
  GetModuleFileNameA(NULL, path, sizeof(path));
  fspath = path;
#elif __linux__
  fspath = fs::canonical("/proc/self/exe");
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

  std::unordered_map<PathType, fs::path> paths;

  const auto insert_path = [&](PathType xenon_path, const fs::path &new_path, bool create = true) {
    if (create && !fs::exists(new_path))
      fs::create_directory(new_path);

    paths.insert_or_assign(xenon_path, new_path);
  };

  insert_path(PathType::BinaryDir, binaryDir, false);
  // If we are in the nix store, it's read-only. Change to currentDir if needed
  if (binaryDir.string().find("/nix/store/") != std::string::npos) {
    nixos = true;
  }
  if (nixos) {
    currentDir /= "files";
    insert_path(PathType::RootDir, currentDir);
    insert_path(PathType::ConsoleDir, currentDir / CONSOLE_DIR);
    insert_path(PathType::LogDir, currentDir / LOG_DIR);
  }
  else {
    insert_path(PathType::RootDir, currentDir, false);
    insert_path(PathType::ConsoleDir, binaryDir / CONSOLE_DIR);
    insert_path(PathType::LogDir, binaryDir / LOG_DIR);
  }
  fs::path fontDir = binaryDir / FONT_DIR;
  // Check if fonts are in ../share/FONT_DIR
  bool createFontsDir = true;
  if (!fs::exists(fontDir) && fs::exists(binaryDir / ".." / "share" / FONT_DIR)) {
    createFontsDir = false;
    fontDir = binaryDir / ".." / "share" / FONT_DIR / "truetype";
  }
  if (createFontsDir && nixos) {
    // We cannot create a directory in the nix-store. Change font dir to currentDir / files
    fontDir = currentDir / FONT_DIR;
  }
  insert_path(PathType::FontDir, fontDir, createFontsDir);

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

std::vector<FileInfo> ListFilesFromPath(const std::filesystem::path& path)
{
  std::vector<FileInfo> fileList;

#ifdef _WIN32 // _WIN32
  // WIN32 Find Data structure.
  WIN32_FIND_DATAW findData;

  // Create a handle to the first file found in the specified path.
  HANDLE fileHandle = FindFirstFileW((path / "*").c_str(), &findData);

  if (fileHandle == INVALID_HANDLE_VALUE) {
    return fileList; // No files in path or bad directory.
  }

  do {
    if (std::wcscmp(findData.cFileName, L".") == 0 || std::wcscmp(findData.cFileName, L"..") == 0) {
      continue;
    }
    //  New file info.
    FileInfo fileInfo;
    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { // Directory?
      fileInfo.fileSize = 0;
      fileInfo.fileType = FileType::Directory;
    }
    else {
      fileInfo.fileSize = (findData.nFileSizeHigh * (size_t(MAXDWORD) + 1)) + findData.nFileSizeLow;
      fileInfo.fileType = FileType::File;
    }

    fileInfo.filePath = path;
    fileInfo.fileName = findData.cFileName;
    fileList.push_back(fileInfo);
  } while (FindNextFileW(fileHandle, &findData) != 0);

  // Close the file handle.
  FindClose(fileHandle);

#else // _LINUX/MACOS
  LOG_ERROR(Base_Filesystem, "ListFilesFromPath is unimplemented on this platform.");
#endif
    return fileList;
}

void SetUserPath(PathType xenon_path, const fs::path &new_path) {
  UserPaths.insert_or_assign(xenon_path, new_path);
}
} // namespace Base::FS
