/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <cstring>
#ifndef TOOL
#include <fmt/format.h>
#else
#include <format>
#endif
#include <iostream>
#include <string_view>
#include <vector>

#include "Base/Types.h"

namespace Base {

#define LOG_SECTIONLESS(...) std::cout << FMT(__VA_ARGS__)

class Param {
public:
  Param(const std::string_view& _name, bool _required, const std::string_view& _desc, bool _hasValue = true, bool _array = false) :
    name(_name), desc(_desc), required(_required), hasValue(_hasValue), isArray(_array)
  {
    next = First;
    First = this;
  }

  static void Init(s32 argc, char **argv) {
    Initialized = true;
    Argc = argc;
    Argv = argv;

    for (s32 i = 1; i != argc; ++i) {
      const char *arg = argv[i];
      if (arg[0] != '-')
        continue;

      const char *equal = std::strchr(arg, '=');
      std::string_view key{};
      std::string_view val{};

      if (equal) {
        key = std::string_view(arg + 1, equal - arg - 1);
        val = equal + 1;
      } else {
        key = arg + 1;
      }
      
      for (Param *p = First; p; p = p->next) {
        if (p->name == key) {
          if (p->hasValue) {
            std::vector<std::string> tokens = {};

            if (!equal && i + 1 < argc && argv[i + 1][0] != '-') {
              ++i;
              tokens.emplace_back(argv[i]);
            } else if (equal) {
              tokens.emplace_back(std::string(equal + 1));
            } else {
              LOG_SECTIONLESS("Missing value for parameter: -{}\n", p->name);
              Help();
              std::exit(1);
            }

            while (i + 1 < argc && argv[i + 1][0] != '-') {
              tokens.emplace_back(argv[++i]);
            }

            for (const std::string &token : tokens) {
              std::string working = token;

              // Strip surrounding quotes
              if (!working.empty() && working.front() == '"' && working.back() == '"' && working.size() >= 2)
                working = working.substr(1, working.size() - 2);

              size_t start = 0;
              while (start < working.size()) {
                // Allow both comma and space as delimiters, skip over duplicates
                while (start < working.size() && (working[start] == ',' || working[start] == ' '))
                  ++start;
                if (start >= working.size()) break;

                size_t end = start;
                while (end < working.size() && working[end] != ',' && working[end] != ' ')
                  ++end;

                std::string val = working.substr(start, end - start);
                if (!val.empty())
                  p->values.emplace_back(std::move(val));

                start = end;
              }
            }
          } else {
            // Flag: mark as present
            p->values = { "true" };
          }
          break;
        }
      }
    }

    // Check required parameters
    for (Param *p = First; p; p = p->next) {
      if (p->required && p->hasValue && !p->Present()) {
        LOG_SECTIONLESS("Missing required parameter: -{}\n", p->name);
        Help();
        std::exit(1);
      }
    }
  }

  static void Help(const char *matchSection = nullptr, bool sectionNamesOnly = false) {
    if (!matchSection || matchSection[0] != '=') {
      bool printedRequiredParams = false;
      for (Param *p = First; p; p = p->next) {
        if (p->required) {
          if (!printedRequiredParams) {
            LOG_SECTIONLESS("Required parameters:\n");
            printedRequiredParams = true;
          }
          LOG_SECTIONLESS(" -{}{}\n   {}", p->name, p->hasValue ? " <value>" : "", p->desc);
        }
      }

      bool printedOptionalParams = false;
      for (Param *p = First; p; p = p->next) {
        if (!p->required) {
          if (!printedOptionalParams) {
            if (printedRequiredParams)
              std::cout << std::endl;
            LOG_SECTIONLESS("Optional parameters:\n");
            printedOptionalParams = true;
          }
          LOG_SECTIONLESS(" -{}{}\n   {}\n", p->name, p->hasValue ? " <value>" : "", p->desc);
        }
      }
    }
  }
  
  template <typename T>
    requires (std::is_integral_v<T> && std::is_unsigned_v<T>)
  T Get(u64 i = 0) const {
    if (!Present() || i >= values.size())
      return 0;
    return static_cast<T>(strtoull(values[i].c_str(), nullptr, 16));
  }

  template <typename T>
    requires (std::is_integral_v<T> && std::is_signed_v<T>)
  T Get(u64 i = 0) const {
    if (!Present() || i >= values.size())
      return 0;
    return static_cast<T>(strtoll(values[i].c_str(), nullptr, 10));
  }

  template <typename T = bool>
    requires (std::is_same_v<T, bool>)
  T Get(u64 i = 0) const {
    if (!Present() || i >= values.size())
      return 0;
    const std::string &str = values[i];
    if (str.length() == 1) {
      return !str.compare("1");
    } else if (str.length() >= 4) {
      return !str.compare("true");
    }
    return false;
  }

  std::string Get(u64 i = 0) const {
    return (i < values.size()) ? values[i] : "";
  }

  const std::vector<std::string> &GetAll() const {
    return values;
  }

  size_t Count() const {
    return values.size();
  }

  bool Present() const {
    return !values.empty();
  }

  bool HasValue() {
    return hasValue;
  }
private:
  static bool Initialized;
  std::string_view name{};
  std::string_view desc{};
  std::vector<std::string> values{};
  bool hasValue = true;
  bool isArray = true;
  bool required = false;
  static Param *First;
  Param *next = nullptr;
  static s32 Argc;
  static char **Argv;
};

inline Param *Param::First = nullptr;
inline bool Param::Initialized = false;
inline s32 Param::Argc = 0;
inline char **Param::Argv = nullptr;

} // namespace Base

#define PARAM(x, desc, ...) ::Base::Param PARAM_##x(#x, false, desc, ##__VA_ARGS__)
#define REQ_PARAM(x, desc, ...) ::Base::Param PARAM_##x(#x, true, desc, ##__VA_ARGS__)
#define XPARAM(x) extern ::Base::Param PARAM_##x
