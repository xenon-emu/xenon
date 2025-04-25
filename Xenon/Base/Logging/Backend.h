// Copyright 2025 Xenon Emulator Project

#pragma once

#include <filesystem>

#include "Filter.h"

namespace Base::Log {

class Filter;

/// Cleans up logs from previous days, and any logs within the desired limit
void CleanupOldLogs(const std::string_view &logFileBase, const std::filesystem::path &logDir, const u16 logLimit = 50);

/// Initializes the logging system
void Initialize(const std::string_view &logFile = {});

bool IsActive();

/// Starts the logging threads
void Start();

/// Explictily stops the logger thread and flushes the buffers
void Stop();

/// The global filter will prevent any messages from even being processed if they are filtered
void SetGlobalFilter(const Filter &filter);

void SetColorConsoleBackendEnabled(bool enabled);

} // namespace Base::Log
