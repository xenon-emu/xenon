// Copyright 2025 Xenon Emulator Project

#include "Filter.h"

#include "Base/Assert.h"

namespace Base::Log {
template <typename It>
Level GetLevelByName(const It begin, const It end) {
  for (u8 i = 0; i < static_cast<u8>(Level::Count); ++i) {
    const char* level_name = GetLevelName(static_cast<Level>(i));
    if (std::string_view(begin, end).compare(level_name) == 0) {
      return static_cast<Level>(i);
    }
  }
  return Level::Count;
}

template <typename It>
Class GetClassByName(const It begin, const It end) {
  for (u8 i = 0; i < static_cast<u8>(Class::Count); ++i) {
    const char* level_name = GetLogClassName(static_cast<Class>(i));
    if (std::string_view(begin, end).compare(level_name) == 0) {
      return static_cast<Class>(i);
    }
  }
  return Class::Count;
}

template <typename Iterator>
bool ParseFilterRule(Filter& instance, Iterator begin, Iterator end) {
  auto level_separator = std::find(begin, end, ':');
  if (level_separator == end) {
    LOG_ERROR(Log, "Invalid log filter. Must specify a log level after `:`: {}", std::string_view(begin, end));
    return false;
  }

  const Level level = GetLevelByName(level_separator + 1, end);
  if (level == Level::Count) {
    LOG_ERROR(Log, "Unknown log level in filter: {}", std::string_view(begin, end));
    return false;
  }

  if (std::string_view(begin, level_separator).compare("*") == 0) {
    instance.ResetAll(level);
    return true;
  }

  const Class log_class = GetClassByName(begin, level_separator);
  if (log_class == Class::Count) {
    LOG_ERROR(Log, "Unknown log class in filter: {}", std::string(begin, end));
    return false;
  }

  instance.SetClassLevel(log_class, level);
  return true;
}

/// Macro listing all log classes. Code should define CLS and SUB as desired before invoking this.
#define ALL_LOG_CLASSES()                                                                        \
  CLS(Log)                                                                                       \
  CLS(Base)                                                                                      \
  SUB(Base, Filesystem)                                                                          \
  CLS(Config)                                                                                    \
  CLS(Debug)                                                                                     \
  CLS(System)                                                                                    \
  CLS(Xenon)                                                                                     \
  SUB(Xenon, IIC)                                                                                \
  SUB(Xenon, MMU)                                                                                \
  SUB(Xenon, PostBus)                                                                            \
  CLS(Xenos)                                                                                     \
  CLS(RootBus)                                                                                   \
  CLS(HostBridge)                                                                                \
  CLS(PCIBridge)                                                                                 \
  CLS(AudioController)                                                                           \
  CLS(EHCI)                                                                                      \
  CLS(OHCI)                                                                                      \
  CLS(ETH)                                                                                       \
  CLS(HDD)                                                                                       \
  CLS(ODD)                                                                                       \
  CLS(SFCX)                                                                                      \
  CLS(XMA)                                                                                       \
  CLS(DebugPrint)                                                                                \
  CLS(SMC)                                                                                       \
  CLS(UART)  

// GetClassName is a macro defined by Windows.h, grrr...
const char* GetLogClassName(Class log_class) {
  switch (log_class) {
#define CLS(x)                                                                                   \
  case Class::x:                                                                                 \
    return #x;
#define SUB(x, y)                                                                                \
  case Class::x##_##y:                                                                           \
    return #x "." #y;
    ALL_LOG_CLASSES()
#undef CLS
#undef SUB
  case Class::Count:
  default:
    break;
  }
  UNREACHABLE();
}

const char* GetLevelName(Level log_level) {
#define LVL(x)                                                                                     \
  case Level::x:                                                                                 \
    return #x
  switch (log_level) {
  LVL(Trace);
  LVL(Debug);
  LVL(Info);
  LVL(Warning);
  LVL(Error);
  LVL(Critical);
  LVL(Guest);
  case Level::Count:
  default:
    break;
  }
#undef LVL
  UNREACHABLE();
}

Filter::Filter(Level default_level) {
  ResetAll(default_level);
}

void Filter::ResetAll(Level level) {
  class_levels.fill(level);
}

void Filter::SetClassLevel(Class log_class, Level level) {
  class_levels[static_cast<size_t>(log_class)] = level;
}

void Filter::ParseFilterString(std::string_view filter_view) {
  auto clause_begin = filter_view.cbegin();
  while (clause_begin != filter_view.cend()) {
    auto clause_end = std::find(clause_begin, filter_view.cend(), ' ');

    // If clause isn't empty
    if (clause_end != clause_begin) {
      ParseFilterRule(*this, clause_begin, clause_end);
    }

    if (clause_end != filter_view.cend()) {
      // Skip over the whitespace
      ++clause_end;
    }
    clause_begin = clause_end;
  }
}

bool Filter::CheckMessage(Class log_class, Level level) const {
  return static_cast<u8>(level) >=
         static_cast<u8>(class_levels[static_cast<size_t>(log_class)]);
}

bool Filter::IsDebug() const {
  return std::any_of(class_levels.begin(), class_levels.end(), [](const Level& l) {
    return static_cast<u8>(l) <= static_cast<u8>(Level::Debug);
  });
}

} // namespace Base::Log
