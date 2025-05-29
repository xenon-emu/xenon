# Copyright 2025 Xenon Emulator Project

cmake_minimum_required(VERSION 3.22)

set(Project XenonTools)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Set compiler ID
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(msvc TRUE)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(gcc TRUE)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(clang TRUE)
endif()

# Optimizations
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
if (msvc)
  add_compile_options($<$<CONFIG:Release>:/Oi>)
  add_compile_options($<$<CONFIG:Release>:/Ot>)
else()
  add_compile_options($<$<CONFIG:Release>:-Ofast>)
endif()

project(${Project})

# Provides a dead-simple logger, designed to avoid threading for tools
file(GLOB_RECURSE SimpleBase ../Base/*.cpp ../Base/*.h)

# Registers for the XGPU
set(Xenon
  ../../Xenon/Core/XGPU/PM4Opcodes.h
  ../../Xenon/Core/XGPU/ShaderConstants.h
  ../../Xenon/Core/XGPU/Xenos.h
  ../../Xenon/Core/XGPU/XenosRegisters.h
)

# Basics to get types, param, and hashe algos
set(XenonBase
  ../../Xenon/Base/Logging/Log.h
  ../../Xenon/Base/Arch.h
  ../../Xenon/Base/CRCHash.h
  ../../Xenon/Base/Exit.h
  ../../Xenon/Base/Hash.h
  ../../Xenon/Base/Param.h
  ../../Xenon/Base/Types.h
  ../../Xenon/Base/Vector128.h
)

# Sets as tool, and disables the logger
add_compile_definitions(TOOL)

add_executable(Byteswap
  byteswap.cpp
  ${SimpleBase}
  ${XenonBase}
)

target_include_directories(Byteswap PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ../Xenon
)

add_executable(GetIndex
  get_idx.cpp
  ${SimpleBase}
  ${XenonBase}
)

target_include_directories(GetIndex PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ../Xenon
)

add_executable(GetOpcode
  get_opcode.cpp
  ${SimpleBase}
  ${XenonBase}
)

target_include_directories(GetOpcode PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ../Xenon
)

add_executable(VPUTests
  vpu_tests.cpp
  ${SimpleBase}
  ${XenonBase}
)

target_include_directories(VPUTests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ../Xenon
)
