// Copyright 2025 Xenon Emulator Project

#pragma once

#include <string>

namespace Base::Log {

struct Entry;

/// Formats a log entry into the provided text buffer.
std::string FormatLogMessage(const Entry &entry);

/// Prints a log entry to stderr.
void PrintMessage(const Entry &entry);

/// Formats and prints a log entry to stderr.
void PrintMessageFmt(const Entry &entry);

/// Prints the same message as `PrintMessage`, but colored according to the severity level.
void PrintColoredMessage(const Entry &entry, bool withFmt = true);

} // namespace Base::Log
