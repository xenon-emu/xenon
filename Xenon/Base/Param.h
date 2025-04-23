// Copyright 2025 Xenon Emulator Project

#pragma once

#include <iostream>
#include <string_view>
#include <cstring>
#include <format>

#include "Types.h"

namespace Base {

#define LOG(x, f, ...) std::cout << "[" << #x << "]" << std::format(f, __VA_ARGS__)
#define LOG_SECTIONLESS(...) std::cout << std::format(__VA_ARGS__)

class Param {
public:
  Param(const std::string_view& _name, bool _required, const std::string_view& _desc, bool _hasValue = true) :
    name(_name), desc(_desc), required(_required), hasValue(_hasValue)
  {
    value = nullptr;
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
            if (equal) {
              // Already parsed 'val'
            } else if (i + 1 < argc && argv[i + 1][0] != '-') {
              val = argv[++i];
            } else {
              LOG_SECTIONLESS("Missing value for parameter: -{}\n", p->name);
              Help();
              std::exit(1);
            }

            // Strip surrounding quotes if present
            if (!val.empty() && val.front() == '"' && val.back() == '"' && val.size() >= 2) {
              val = val.substr(1, val.size() - 2);
            }

            p->value = val.data();
          } else {
            // This is a flag - presence means enabled
            p->value = "true";
          }
          break;
        }
      }
    }

    // Check required parameters
    for (Param *p = First; p; p = p->next) {
      if (p->required && p->hasValue && !p->value) {
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
              fmt::print("\n");
            LOG_SECTIONLESS("Optional parameters:\n");
            printedOptionalParams = true;
          }
          LOG_SECTIONLESS(" -{}{}\n   {}\n", p->name, p->hasValue ? " <value>" : "", p->desc);
        }
      }
    }
  }

  const char *Get() const {
    return value;
  }
  bool IsSet() const {
    return value != nullptr;
  }
  bool HasValue() {
    return hasValue;
  }
private:
  static bool Initialized;
  std::string_view name{};
  std::string_view desc{};
  const char *value = nullptr;
  bool hasValue = true;
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
