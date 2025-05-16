// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include <string>
#include <string_view>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#include <Windows.h>
#else
#include <filesystem>
#endif

namespace Base {

[[nodiscard]] std::string UTF16ToUTF8(const std::wstring_view &input);
[[nodiscard]] std::wstring UTF8ToUTF16W(const std::string_view &str);
[[nodiscard]] std::string ToLower(const std::string_view &str);

} // namespace Base
