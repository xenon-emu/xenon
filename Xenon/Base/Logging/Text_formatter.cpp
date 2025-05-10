// Copyright 2025 Xenon Emulator Project

#include "Text_formatter.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#include "Base/Assert.h"
#include "Base/Config.h"
#include "Filter.h"
#include "Log_entry.h"

namespace Base {
namespace Log {

std::string FormatLogMessage(const Entry &entry) {
  const char *className = GetLogClassName(entry.logClass);
  const char *levelName = GetLevelName(entry.logLevel);

  if (Config::log.advanced) {
    return fmt::format("[{}] <{}> {}:{}:{}: {}", className, levelName, entry.filename,
      entry.function, entry.lineNum, entry.message);
  } else {
    return fmt::format("[{}] <{}> {}", className, levelName, entry.message);
  }
}

void PrintMessage(const Entry &entry) {
  const auto str = entry.message;
  fputs(str.c_str(), stdout);
}

void PrintMessageFmt(const Entry &entry) {
  const auto str = FormatLogMessage(entry).append(1, '\n');
  fputs(str.c_str(), stdout);
}

void PrintColoredMessage(const Entry &entry, bool withFmt) {
  #define ESC "\x1b"
  // NOTE: Custom colors can be achieved
  // std::format("\x1b[{};2;{};{};{}m", color.bg ? 48 : 38, color.r, color.g, color.b)
  const char *color = "";
  switch (entry.logLevel) {
  case Level::Trace: // Grey
    color = ESC "[1;30m";
    break;
  case Level::Debug: // Cyan
    color = ESC "[0;36m";
    break;
  case Level::Info: // Bright gray
    color = ESC "[0;37m";
    break;
  case Level::Warning: // Bright yellow
    color = ESC "[1;33m";
    break;
  case Level::Error: // Bright red
    color = ESC "[1;31m";
    break;
  case Level::Critical: // Bright magenta
    color = ESC "[1;35m";
    break;
  case Level::Guest: // Green
    color = ESC "[0;92m";
    break;
  case Level::Count:
    UNREACHABLE();
  }

  fputs(color, stdout);

  if (withFmt)
    PrintMessageFmt(entry);
  else
    PrintMessage(entry);
  
  fputs(ESC "[0m", stdout);
  #undef ESC
}

} // namespace Log
} // namespace Base
