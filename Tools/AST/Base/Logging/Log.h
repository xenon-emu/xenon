// Copyright 2025 Xenon Emulator Project

#pragma once

#include <format>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

#include "Types.h"

#define THROW_MSG(x, ...) if (x) { LOG_ERROR(Assert, "Assertion! " __VA_ARGS__); }

#define LOG_INFO_BASE(x, ...) std::cout << "[" << #x << "] " << "<Info> " << std::format(__VA_ARGS__)
#define LOG_ERROR_BASE(x, ...) std::cout << "[" << #x << "] " << "<Error> " << std::format(__VA_ARGS__)
#define LOG_DEBUG_BASE(x, ...) std::cout << "[" << #x << "] " << "<Debug> " << std::format(__VA_ARGS__)
#define LOG_INFO(x, ...) LOG_INFO_BASE(x, __VA_ARGS__) << std::endl
#define LOG_ERROR(x, ...) LOG_ERROR_BASE(x, __VA_ARGS__) << std::endl
#define LOG_DEBUG(x, ...) LOG_DEBUG_BASE(x, __VA_ARGS__) << std::endl
