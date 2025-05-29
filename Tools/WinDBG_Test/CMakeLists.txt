cmake_minimum_required(VERSION 3.22)

set(Project WinDBG_Test)

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

add_executable(${Project})

file(GLOB Main *.cpp *.h)

# Basics to get types, param, and hashe algos
set(XenonBase
  ../../Xenon/Base/Arch.h
  ../../Xenon/Base/CRCHash.h
  ../../Xenon/Base/Exit.h
  ../../Xenon/Base/Hash.h
  ../../Xenon/Base/Param.h
  ../../Xenon/Base/Types.h
)

# Sets as tool, and disables the logger
add_compile_definitions(TOOL)

target_sources(${Project} PRIVATE
  ${Main}
  ${XenonBase}
)

target_include_directories(${Project} BEFORE PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ../../Xenon
)

target_link_options(${Project} PRIVATE -static)
