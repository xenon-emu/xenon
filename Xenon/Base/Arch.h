// Copyright 2025 Xenon Emulator Project

#pragma once

#if defined(__x86_64__) || defined(_M_X64) || defined(_M_X86)
#define ARCH_X86_64 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#define ARCH_ARM64 1
#endif