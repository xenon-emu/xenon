# Copyright 2025 Xenon Emulator Project

add_library(glad STATIC src/glad.c)
target_include_directories(glad PUBLIC include)
