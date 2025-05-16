// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Backend.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#include <fmt/format.h>
#include <functional>
#include <map>

#include "Base/Bounded_threadsafe_queue.h"
#include "Base/io_file.h"
#include "Base/Path_util.h"
#include "Base/Thread.h"

#include "Log.h"
#include "Log_entry.h"
#include "Text_formatter.h"

namespace Base {
namespace Log {

using namespace Base::FS;

// Base backend with shell functions
class BaseBackend {
public:
  virtual ~BaseBackend() = default;
  virtual void Write(const Entry &entry, bool formatted = true) = 0;
  virtual void Flush() = 0;
};


// Backend that writes to stdout and with color
class ColorConsoleBackend : public BaseBackend {
public:
  explicit ColorConsoleBackend() = default;

  ~ColorConsoleBackend() = default;

  void Write(const Entry &entry, bool formatted) override {
    if (enabled.load(std::memory_order_relaxed)) {
      PrintColoredMessage(entry, formatted);
    }
  }

  void Flush() override {
    // stdout shouldn't be buffered
  }

  void SetEnabled(bool enabled_) {
    enabled = enabled_;
  }

private:
  std::atomic<bool> enabled = true;
};

// Backend that writes to a file passed into the constructor
class FileBackend : public BaseBackend {
public:
  explicit FileBackend(const std::filesystem::path &filename)
    : file(filename, FS::FileAccessMode::Write, FS::FileMode::TextMode)
  {}

  ~FileBackend() {
    file.Close();
  }

  void Write(const Entry &entry, bool formatted) override {
    if (!enabled) {
      return;
    }

    if (formatted) {
      bytesWritten += file.WriteString(FormatLogMessage(entry).append(1, '\n'));
    }
    else {
      bytesWritten += file.WriteString(entry.message);
    }

    // Prevent logs from exceeding a set maximum size in the event that log entries are spammed.
    constexpr u64 writeLimit = 100_MB;
    const bool writeLimitExceeded = bytesWritten > writeLimit;
    if (entry.logLevel >= Level::Error || writeLimitExceeded) {
      if (writeLimitExceeded) {
        // Stop writing after the write limit is exceeded.
        // Don't close the file so we can print a stacktrace if necessary
        enabled = false;
      }
      file.Flush();
    }
  }

  void Flush() override {
    file.Flush();
  }

private:
  Base::FS::IOFile file;
  std::atomic<bool> enabled = true;
  size_t bytesWritten = 0;
};

bool currentlyInitialising = true;

// Static state as a singleton.
class Impl {
public:
  static Impl& Instance() {
    if (!instance) {
      throw std::runtime_error("Using Logging instance before its initialization");
    }
    return *instance;
  }

  static void Initialize(const std::string_view logFile) {
    if (instance) {
      LOG_WARNING(Log, "Reinitializing logging backend");
      return;
    }
    const auto logDir = GetUserPath(PathType::LogDir);
    Filter filter;
    //filter.ParseFilterString(Config::getLogFilter());
    instance = std::unique_ptr<Impl, decltype(&Deleter)>(new Impl(logDir / logFile, filter), Deleter);
    currentlyInitialising = false;
  }

  static bool IsActive() {
    return instance != nullptr;
  }

  static void Start() {
    instance->StartBackendThread();
  }

  static void Stop() {
    instance->StopBackendThread();
  }

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  Impl(Impl&&) = delete;
  Impl& operator=(Impl&&) = delete;

  void SetGlobalFilter(const Filter& f) {
    filter = f;
  }

  void SetColorConsoleBackendEnabled(bool enabled) {
    colorConsoleBackend->SetEnabled(enabled);
  }

  void PushEntry(Class logClass, Level logLevel, const char *filename, u32 lineNum,
           const char *function, const std::string &message) {

    if (!filter.CheckMessage(logClass, logLevel)) {
      return;
    }

    using std::chrono::duration_cast;
    using std::chrono::microseconds;
    using std::chrono::steady_clock;

    const Entry entry = {
      .timestamp = duration_cast<microseconds>(steady_clock::now() - timeOrigin),
      .logClass = logClass,
      .logLevel = logLevel,
      .filename = filename,
      .lineNum = lineNum,
      .function = function,
      .message = std::move(message),
    };
    ForEachBackend([&entry](BaseBackend* backend) { if (backend) { backend->Write(entry); } });
    std::fflush(stdout);
    /*if (Config::getLogType() == "async") {
      messageQueue.EmplaceWait(entry);
    } else {
      ForEachBackend([&entry](BaseBackend* backend) { if (backend) { backend->Write(entry); } });
      std::fflush(stdout);
    }*/
  }

  void PushEntryNoFmt(Class logClass, Level logLevel, const std::string &message) {
    if (!filter.CheckMessage(logClass, logLevel)) {
      return;
    }

    using std::chrono::duration_cast;
    using std::chrono::microseconds;
    using std::chrono::steady_clock;

    const Entry entry = {
      .timestamp = duration_cast<microseconds>(steady_clock::now() - timeOrigin),
      .logClass = logClass,
      .logLevel = logLevel,
      .message = std::move(message),
    };
    ForEachBackend([&entry](BaseBackend* backend) { if (backend) { backend->Write(entry, false); } });
    std::fflush(stdout);
    /*if (Config::getLogType() == "async") {
      messageQueue.EmplaceWait(entry);
    } else {
      ForEachBackend([&entry](BaseBackend* backend) { if (backend) { backend->Write(entry); } });
      std::fflush(stdout);
    }*/
  }

private:
  Impl(const std::filesystem::path &fileBackendFilename, const Filter &filter) :
    filter(filter) {
#ifdef _WIN32
    HANDLE conOut = GetStdHandle(STD_OUTPUT_HANDLE);
    // Get current console mode
    ul32 mode = 0;
    GetConsoleMode(conOut, &mode);
    // Set WinAPI to use a more 'modern' approach, by enabling VT
    // Allows ASCII escape codes
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    // Write adjusted mode back
    SetConsoleMode(conOut, mode);
#endif
    colorConsoleBackend = std::make_unique<ColorConsoleBackend>();
    fileBackend = std::make_unique<FileBackend>(fileBackendFilename);
  }

  ~Impl() {
    Stop();
    fileBackend.reset();
    colorConsoleBackend.reset();
  }

  void StartBackendThread() {
    backendThread = std::jthread([this](std::stop_token stop_token) {
      Base::SetCurrentThreadName("[Xe] Log");
      Entry entry;
      const auto write_logs = [this, &entry]() {
        ForEachBackend([&entry](BaseBackend* backend) { backend->Write(entry); });
      };
      while (!stop_token.stop_requested()) {
        messageQueue.PopWait(entry, stop_token);
        if (entry.filename != nullptr) {
          write_logs();
        }
      }
      // Drain the logging queue. Only writes out up to MAX_LOGS_TO_WRITE to prevent a
      // case where a system is repeatedly spamming logs even on close.
      int max_logs_to_write = filter.IsDebug() ? std::numeric_limits<s32>::max() : 100;
      while (max_logs_to_write-- && messageQueue.TryPop(entry)) {
        write_logs();
      }
    });
  }

  void StopBackendThread() {
    backendThread.request_stop();
    if (backendThread.joinable()) {
      backendThread.join();
    }

    ForEachBackend([](BaseBackend* backend) { backend->Flush(); });
  }

  void ForEachBackend(std::function<void(BaseBackend*)> lambda) {
    lambda(colorConsoleBackend.get());
    lambda(fileBackend.get());
  }

  static void Deleter(Impl* ptr) {
    delete ptr;
  }

  static inline std::unique_ptr<Impl, decltype(&Deleter)> instance{nullptr, Deleter};

  Filter filter;
  std::unique_ptr<ColorConsoleBackend> colorConsoleBackend = {};
  std::unique_ptr<FileBackend> fileBackend = {};

  MPSCQueue<Entry> messageQueue = {};
  std::chrono::steady_clock::time_point timeOrigin = std::chrono::steady_clock::now();
  std::jthread backendThread;
};

std::vector<std::filesystem::path> filepaths{};

void DeleteOldLogs(const std::filesystem::path& path, u64 num_logs, const u16 logLimit) {
  const std::string filename = path.filename().string();
  const std::chrono::time_point Now = std::chrono::system_clock::now();
  const time_t timeNow = std::chrono::system_clock::to_time_t(Now);
  const tm* time = std::localtime(&timeNow);
  // We want to get rid of anything that isn't that current day's date
  const std::string currentDate = fmt::format("{}-{}-{}", time->tm_mon + 1, time->tm_mday, 1900 + time->tm_year);
  if (filename.find(currentDate) == std::string::npos) {
    std::filesystem::remove_all(path);
    return;
  }
  // We want to delete in date of creation, so just add it to a array
  if (num_logs >= logLimit) {
    filepaths.push_back(path);
  }
}

u64 CreateIntegralTimestamp(const std::string &date) {
  const u64 monthPos = date.find('-');
  if (monthPos == std::string::npos) {
    return 0;
  }
  const std::string month = date.substr(0, monthPos);
  const u64 dayPos = date.find('-', monthPos);
  if (dayPos == std::string::npos) {
    return 0;
  }
  const u64 yearPos = date.find('-', dayPos);
  const std::string day = date.substr(monthPos+1);
  if (yearPos == std::string::npos) {
    return 0;
  }
  const std::string year = date.substr(yearPos + 1);
  const u64 yearInt = std::stoull(year);
  const std::string timestamp = fmt::format("{}{}{}", month, day, yearInt - 1900);
  return std::stoull(timestamp);
}

void CleanupOldLogs(const std::string_view &logFileBase, const std::filesystem::path &logDir, const u16 logLimit) {
  const std::filesystem::path LogFile = logFileBase;
  // Track how many logs we have
  size_t numLogs = 0;
  for (auto &entry : std::filesystem::directory_iterator(logDir)) {
    if (entry.is_regular_file()) {
      const std::filesystem::path path = entry.path();
      const std::string ext = path.extension().string();
      if (!path.has_extension()) {
        // Skip anything that isn't a log file
        continue;
      }
      if (ext != LogFile.extension()) {
        // Skip anything that isn't a log file
        continue;
      }
      numLogs++;
      DeleteOldLogs(path, numLogs, logLimit);
    } else {
      // Skip anything that isn't a file
      continue;
    }
  }
  if (filepaths.empty()) {
    return;
  }
  u64 numToDelete{ logLimit };
  std::map<u64, std::filesystem::path> date_sorted_paths{};
  for (const auto &path : filepaths) {
    const std::string stem = path.stem().string();
    u64 basePos = stem.find('_');
    // If we cannot get the base, just delete it
    if (basePos == std::string::npos) {
      numToDelete--;
      std::filesystem::remove_all(path);
    } else {
      const std::string base = stem.substr(0, basePos);
      const u64 datePos = base.find('_', basePos+1);
      const std::string date = base.substr(datePos+1);
      const u64 dateInt = CreateIntegralTimestamp(date);
      if (datePos == std::string::npos) {
        // If we cannot find the date, just delete it
        numToDelete--;
        std::filesystem::remove_all(path);
      } else {
        const u64 timePos = base.find('_', datePos+1);
        const std::string time = base.substr(timePos+1);
        const u64 timestamp = CreateIntegralTimestamp(time);
        if (!timestamp) {
          numToDelete--;
          std::filesystem::remove_all(path);
          continue;
        }
        date_sorted_paths.insert({ dateInt + timestamp, path });
      }
    }
  }
  // Start deleting based off timestamp
  for (const auto &entry : date_sorted_paths) {
    std::filesystem::remove_all(entry.second);
  }
}

void Initialize(const std::string_view &logFile) {
  // Create directory vars to so we can use std::filesystem::path::stem
  const std::filesystem::path LogDir = GetUserPath(PathType::LogDir);
  const std::filesystem::path LogFile = LOG_FILE;
  const std::filesystem::path LogFileStem = LogFile.stem();
  const std::filesystem::path LogFileName = LogFile.filename();
  // This is to make string_view happy
  const std::string LogFileStemStr = LogFileStem.string();
  const std::string LogFileNameStr = LogFileName.string();
  // Setup filename
  const std::string_view filestemBase = logFile.empty() ? LogFileStemStr : logFile;
  const std::string_view filenameBase = logFile.empty() ? LogFileNameStr : logFile;
  const std::chrono::time_point now = std::chrono::system_clock::now();
  const time_t timeNow = std::chrono::system_clock::to_time_t(now);
  const tm *time = std::localtime(&timeNow);
  const std::string currentTime = fmt::format("{}-{}-{}", time->tm_hour, time->tm_min, time->tm_sec);
  const std::string currentDate = fmt::format("{}-{}-{}", time->tm_mon + 1, time->tm_mday, 1900 + time->tm_year);
  const std::string filename = fmt::format("{}_{}_{}.txt", filestemBase, currentDate, currentTime);
  CleanupOldLogs(filenameBase, LogDir);
  Impl::Initialize(logFile.empty() ? filename : logFile);
}

bool IsActive() {
  return Impl::IsActive();
}

void Start() {
  Impl::Start();
}

void Stop() {
  Impl::Stop();
}

void SetGlobalFilter(const Filter &filter) {
  Impl::Instance().SetGlobalFilter(filter);
}

void SetColorConsoleBackendEnabled(bool enabled) {
  Impl::Instance().SetColorConsoleBackendEnabled(enabled);
}

void FmtLogMessageImpl(Class logClass, Level logLevel, const char *filename,
             u32 lineNum, const char *function, const char *format,
             const fmt::format_args &args) {
  if (!currentlyInitialising) [[likely]] {
    Impl::Instance().PushEntry(logClass, logLevel, filename, lineNum, function,
                   fmt::vformat(format, args));
  }
}

void NoFmtMessage(Class logClass, Level logLevel, const std::string &message) {
  if (!currentlyInitialising) [[likely]] {
    Impl::Instance().PushEntryNoFmt(logClass, logLevel, message);
  }
}
} // namespace Log
} // namespace Base
