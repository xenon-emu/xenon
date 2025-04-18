#pragma once
#include <filesystem>

#ifdef _DEBUG
#define MICROPROFILE_ENABLED 1
#else
#define MICROPROFILE_ENABLED 0
#define MICROPROFILE_WEBSERVER 0
#endif

#if defined(MICROPROFILE_ENABLED) && MICROPROFILE_ENABLED == 1
#define MICROPROFILE_WEBSERVER 1
#endif
#if defined(MICROPROFILE_ENABLED) && MICROPROFILE_ENABLED == 1
// Screw it, we can leave it dangling on the stack. Not recommended, but good enough
#define MICROPROFILE_GET_SETTINGS_FILE_PATH std::filesystem::current_path().string().c_str()
#define MICROPROFILE_SETTINGS_FILE "microprofile.cfg"
#define MICROPROFILE_SETTINGS_FILE_TEMP "microprofile.cfg.tmp"
#endif
