// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Text_formatter.h"

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

#define ESC "\x1b"
void PrintMessage(const std::string &color, const Entry &entry) {
  const std::string str = color + entry.message + ESC "[0m";
  fputs(str.c_str(), stdout);
}

void PrintMessageFmt(const std::string &color, const Entry &entry) {
  const std::string str = color + FormatLogMessage(entry).append(ESC "[0m\n");
  fputs(str.c_str(), stdout);
}

void PrintColoredMessage(const Entry &entry, bool withFmt) {
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

  if (withFmt)
    PrintMessageFmt(color, entry);
  else
    PrintMessage(color, entry);
}
#undef ESC

} // namespace Log
} // namespace Base
