// Copyright 2025 Xenon Emulator Project

#pragma once

#ifdef _WIN32

#include <string>

#include "Types.h"

namespace Base {

[[nodiscard]] std::string UTF16ToUTF8(const std::wstring_view input);
[[nodiscard]] std::wstring UTF8ToUTF16W(const std::string_view str);

} // namespace Base
#endif
