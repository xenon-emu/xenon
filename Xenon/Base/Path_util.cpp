// Copyright 2025 Xenon Emulator Project

#include "Path_util.h"

#include <unordered_map>
#include <fstream>

namespace Base::FS {

static auto UserPaths = [] {
  auto currentDir = fs::current_path();

  std::unordered_map<PathType, fs::path> paths;

  const auto insert_path = [&](PathType xenon_path, const fs::path &new_path, bool create = true) {
    if (create && !fs::exists(new_path))
      fs::create_directory(new_path);

    paths.insert_or_assign(xenon_path, new_path);
  };

  insert_path(PathType::RootDir, currentDir, false);
  insert_path(PathType::ConsoleDir, currentDir / CONSOLE_DIR);
  insert_path(PathType::LogDir, currentDir / LOG_DIR);
  fs::path fontDir = currentDir / FONT_DIR;
  bool createFontsDir = true;
  if (!fs::exists(fontDir) && fs::exists(currentDir / ".." / "share" / FONT_DIR)) {
    createFontsDir = false;
    fontDir = currentDir / ".." / "share" / FONT_DIR / "truetype";
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

void SetUserPath(PathType xenon_path, const fs::path &new_path) {
  UserPaths.insert_or_assign(xenon_path, new_path);
}
} // namespace Base::FS
