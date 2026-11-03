/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/PathUtil.h"
#include "Filter.h"
#include "LogEntry.h"

#include <filesystem>
#include <string_view>
#include <vector>

namespace Base {
  namespace Log {

    class Filter;

    /// Cleans up logs from previous days, and any logs within the desired limit
    void CleanupOldLogs(const std::string_view &logFileBase, const fs::path &logDir, const u16 logLimit = 50);

    /// Initializes the logging system
    void Initialize(const std::string_view &logFile = {});

    bool IsActive();

    /// Starts the logging threads
    void Start();

    /// Explicitly stops the logger thread and flushes the buffers
    void Stop();

    /// The global filter will prevent any messages from even being processed if they are filtered
    void SetGlobalFilter(const Filter &filter);

    void SetColorConsoleBackendEnabled(bool enabled);

    /// A copy of the most recent log entries (the last 1000), for UI consumers such as the ImGui Log window. Safe to
    /// call from the render thread while other threads keep logging.
    std::vector<Entry> GetRecentEntries();

  } // namespace Log
} // namespace Base
