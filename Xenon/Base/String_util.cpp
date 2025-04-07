// Copyright 2025 Xenon Emulator Project

#ifdef _WIN32

#include "String_util.h"

#include <windows.h>

namespace Base {
static std::wstring CPToUTF16(u32 code_page, std::string_view input) {
  const auto size =
    MultiByteToWideChar(code_page, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);

  if (size == 0) {
    return {};
  }

  std::wstring output(size, L'\0');

  if (size != MultiByteToWideChar(code_page, 0, input.data(), static_cast<int>(input.size()),
                  &output[0], static_cast<int>(output.size()))) {
    output.clear();
  }

  return output;
}

std::string UTF16ToUTF8(std::wstring_view input) {
  const auto size = WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()),
                      nullptr, 0, nullptr, nullptr);
  if (size == 0) {
    return {};
  }

  std::string output(size, '\0');

  if (size != WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()),
                  &output[0], static_cast<int>(output.size()), nullptr,
                  nullptr)) {
    output.clear();
  }

  return output;
}

std::wstring UTF8ToUTF16W(std::string_view input) {
  return CPToUTF16(CP_UTF8, input);
}
} // namespace Base
#endif
