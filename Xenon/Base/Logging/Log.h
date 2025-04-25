// Copyright 2025 Xenon Emulator Project

#pragma once

#include <algorithm>
#include <fmt/format.h>

#include "Log_types.h"
#include "Base/Config.h"

namespace Base::Log {

constexpr const char* TrimSourcePath(const std::string_view &source) {
  const auto rfind = [source](const std::string_view match) {
    return source.rfind(match) == source.npos ? 0 : (source.rfind(match) + match.size());
  };
  const auto idx = std::max({ rfind("/"), rfind("\\") });
  return source.data() + idx;
}

/// Logs a message to the global logger, using fmt
void FmtLogMessageImpl(Class logClass, Level logLevel, const char *filename,
                       u32 lineNum, const char *function, const char *format,
                       const fmt::format_args& args);

/// Logs a message without any formatting
void NoFmtMessage(Class logClass, Level logLevel, const std::string &message);

template <typename... Args>
void FmtLogMessage(Class logClass, Level logLevel, const char *filename, u32 lineNum,
                   const char *function, const char *format, const Args&... args) {
  FmtLogMessageImpl(logClass, logLevel, filename, lineNum, function, format,
                    fmt::make_format_args(args...));
}

} // namespace Base::Log

// Define the fmt lib macros
#define LOG_GENERIC(logClass, logLevel, ...)                                             \
  Base::Log::FmtLogMessage(logClass, logLevel, Base::Log::TrimSourcePath(__FILE__),      \
                             __LINE__, __func__, __VA_ARGS__)

#define LOG_TRACE(logClass, ...)                                                         \
if (Config::log.debugOnly)                                                               \
  Base::Log::FmtLogMessage(Base::Log::Class::logClass, Base::Log::Level::Trace,          \
                           Base::Log::TrimSourcePath(__FILE__), __LINE__, __func__,      \
                           __VA_ARGS__)

#define LOG_DEBUG(logClass, ...)                                                         \
  Base::Log::FmtLogMessage(Base::Log::Class::logClass, Base::Log::Level::Debug,          \
                           Base::Log::TrimSourcePath(__FILE__), __LINE__, __func__,      \
                           __VA_ARGS__)
#define LOG_INFO(logClass, ...)                                                          \
  Base::Log::FmtLogMessage(Base::Log::Class::logClass, Base::Log::Level::Info,           \
                           Base::Log::TrimSourcePath(__FILE__), __LINE__, __func__,      \
                           __VA_ARGS__)
#define LOG_WARNING(logClass, ...)                                                       \
  Base::Log::FmtLogMessage(Base::Log::Class::logClass, Base::Log::Level::Warning,        \
                           Base::Log::TrimSourcePath(__FILE__), __LINE__, __func__,      \
                           __VA_ARGS__)
#define LOG_ERROR(logClass, ...)                                                         \
  Base::Log::FmtLogMessage(Base::Log::Class::logClass, Base::Log::Level::Error,          \
                           Base::Log::TrimSourcePath(__FILE__), __LINE__, __func__,      \
                           __VA_ARGS__)
#define LOG_CRITICAL(logClass, ...)                                                      \
  Base::Log::FmtLogMessage(Base::Log::Class::logClass, Base::Log::Level::Critical,       \
                           Base::Log::TrimSourcePath(__FILE__), __LINE__, __func__,      \
                           __VA_ARGS__)
#define LOG_XBOX(logClass, ...)                                                          \
  Base::Log::FmtLogMessage(Base::Log::Class::logClass, Base::Log::Level::Guest,          \
                           Base::Log::TrimSourcePath(__FILE__), __LINE__, __func__,      \
                           __VA_ARGS__)
