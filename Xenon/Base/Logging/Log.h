// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#ifndef TOOL

#include <algorithm>
#include <fmt/format.h>

#include "Base/Config.h"

#include "LogTypes.h"

namespace Base {
namespace Log {

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

} // namespace Log
} // namespace Base

// Define the fmt lib macros
#define LOG_GENERIC(logClass, logLevel, ...)                                             \
  Base::Log::FmtLogMessage(logClass, logLevel, Base::Log::TrimSourcePath(__FILE__),      \
                             __LINE__, __func__, __VA_ARGS__)
#ifdef DEBUG_BUILD
#define LOG_TRACE(logClass, ...)                                                         \
if (Config::log.debugOnly)                                                               \
  Base::Log::FmtLogMessage(Base::Log::Class::logClass, Base::Log::Level::Trace,          \
                           Base::Log::TrimSourcePath(__FILE__), __LINE__, __func__,      \
                           __VA_ARGS__)
#else
#define LOG_TRACE(logClass, ...) ;
#endif

#ifdef DEBUG_BUILD
#define LOG_DEBUG(logClass, ...)                                                         \
  Base::Log::FmtLogMessage(Base::Log::Class::logClass, Base::Log::Level::Debug,          \
                           Base::Log::TrimSourcePath(__FILE__), __LINE__, __func__,      \
                           __VA_ARGS__)
#else
#define LOG_DEBUG(logClass, ...) ;
#endif
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
#else

#include <format>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;
#include "Base/Types.h"

#define LOG_BASE(x, s, ...) std::cout << "[" #x "] <" #s "> " << std::format(__VA_ARGS__)
#define LOG_INFO_BASE(x, ...) LOG_BASE(x, Info, __VA_ARGS__)
#define LOG_WARNING_BASE(x, ...) LOG_BASE(x, Warning, __VA_ARGS__)
#define LOG_ERROR_BASE(x, ...) LOG_BASE(x, Error, __VA_ARGS__)
#define LOG_CRITICAL_BASE(x, ...) LOG_BASE(x, Critical, __VA_ARGS__)
#define LOG_DEBUG_BASE(x, ...) LOG_BASE(x, Debug, __VA_ARGS__)
#define LOG_XBOX_BASE(x, ...) LOG_BASE(x, Xbox, __VA_ARGS__)

#define LOG_INFO(x, ...) LOG_INFO_BASE(x, __VA_ARGS__) << std::endl
#define LOG_WARNING(x, ...) LOG_WARNING_BASE(x, __VA_ARGS__) << std::endl
#define LOG_ERROR(x, ...) LOG_ERROR_BASE(x, __VA_ARGS__) << std::endl
#define LOG_CRITICAL(x, ...) LOG_CRITICAL_BASE(x, __VA_ARGS__) << std::endl
#define LOG_DEBUG(x, ...) LOG_DEBUG_BASE(x, __VA_ARGS__) << std::endl
#define LOG_XBOX(x, ...) LOG_XBOX_BASE(x, __VA_ARGS__) << std::endl

#endif