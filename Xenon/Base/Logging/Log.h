// Copyright 2025 Xenon Emulator Project

#pragma once

#include <algorithm>

#include "Formatter.h"
#include "Types.h"

namespace Base::Log {

constexpr const char* TrimSourcePath(std::string_view source) {
  const auto rfind = [source](const std::string_view match) {
    return source.rfind(match) == source.npos ? 0 : (source.rfind(match) + match.size());
  };
  auto idx = std::max({rfind("/"), rfind("\\")});
  return source.data() + idx;
}

/// Logs a message to the global logger, using fmt
void FmtLogMessageImpl(Class log_class, Level log_level, const char* filename,
                       unsigned int line_num, const char* function, const char* format,
                       const fmt::format_args& args);

template <typename... Args>
void FmtLogMessage(Class log_class, Level log_level, const char* filename, unsigned int line_num,
                   const char* function, const char* format, const Args&... args) {
  FmtLogMessageImpl(log_class, log_level, filename, line_num, function, format,
                    fmt::make_format_args(args...));
}

} // namespace Base::Log

// Define the fmt lib macros
#define LOG_GENERIC(log_class, log_level, ...)                                             \
  Base::Log::FmtLogMessage(log_class, log_level, Base::Log::TrimSourcePath(__FILE__),      \
                             __LINE__, __func__, __VA_ARGS__)

#ifdef _DEBUG
#define LOG_TRACE(log_class, ...)                                                          \
  Base::Log::FmtLogMessage(Base::Log::Class::log_class, Base::Log::Level::Trace,           \
                           Base::Log::TrimSourcePath(__FILE__), __LINE__, __func__,        \
                           __VA_ARGS__)
#else
#define LOG_TRACE(log_class, fmt, ...) (void(0))
#endif

#define LOG_DEBUG(log_class, ...)                                                          \
  Base::Log::FmtLogMessage(Base::Log::Class::log_class, Base::Log::Level::Debug,           \
                           Base::Log::TrimSourcePath(__FILE__), __LINE__, __func__,        \
                           __VA_ARGS__)
#define LOG_INFO(log_class, ...)                                                           \
  Base::Log::FmtLogMessage(Base::Log::Class::log_class, Base::Log::Level::Info,            \
                           Base::Log::TrimSourcePath(__FILE__), __LINE__, __func__,        \
                           __VA_ARGS__)
#define LOG_WARNING(log_class, ...)                                                        \
  Base::Log::FmtLogMessage(Base::Log::Class::log_class, Base::Log::Level::Warning,         \
                           Base::Log::TrimSourcePath(__FILE__), __LINE__, __func__,        \
                           __VA_ARGS__)
#define LOG_ERROR(log_class, ...)                                                          \
  Base::Log::FmtLogMessage(Base::Log::Class::log_class, Base::Log::Level::Error,           \
                           Base::Log::TrimSourcePath(__FILE__), __LINE__, __func__,        \
                           __VA_ARGS__)
#define LOG_CRITICAL(log_class, ...)                                                       \
  Base::Log::FmtLogMessage(Base::Log::Class::log_class, Base::Log::Level::Critical,        \
                           Base::Log::TrimSourcePath(__FILE__), __LINE__, __func__,        \
                           __VA_ARGS__)
#define LOG_XBOX(log_class, ...)                                                           \
  Base::Log::FmtLogMessage(Base::Log::Class::log_class, Base::Log::Level::Guest,           \
                           Base::Log::TrimSourcePath(__FILE__), __LINE__, __func__,        \
                           __VA_ARGS__)
