#***************************************************************
#* Copyright 2026 Xenon Emulator Project. All rights reserved. *
#***************************************************************

add_library(glad STATIC src/glad.c)
target_include_directories(glad PUBLIC include)
