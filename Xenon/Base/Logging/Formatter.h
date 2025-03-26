// Copyright 2025 Xenon Emulator Project

#pragma once

#include <fmt/format.h>

namespace fmt {
template <typename T = std::string_view>
struct UTF {
  T data;

  explicit UTF(const std::u8string_view view) {
    data = view.empty() ? T{} : T{(const char*)&view.front(), (const char*)&view.back() + 1};
  }

  explicit UTF(const std::u8string& str) : UTF(std::u8string_view{str}) {}
};
} // namespace fmt

template <>
struct fmt::formatter<fmt::UTF<std::string_view>, char> : formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const UTF<std::string_view>& wrapper, FormatContext& ctx) const {
    return formatter<std::string_view>::format(wrapper.data, ctx);
  }
};
