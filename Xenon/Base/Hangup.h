/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#ifdef __linux__
#include <signal.h>
#elif defined(_WIN32)
#include <Windows.h>
#endif

namespace Base {

inline volatile int hupflag = 0;

[[nodiscard]] extern const s32 InstallHangup();
[[nodiscard]] extern const s32 RemoveHangup();

} // namespace Base
